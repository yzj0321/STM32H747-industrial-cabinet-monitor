/**
 * @file    bsp_w5500_app.c
 * @brief   W5500 启动流程：以太网链路 + OTA TCP 客户端
 *
 * 状态机概要：
 *   OFF/BOOT -> NET_READY -> TCP_CONNECTING -> TCP_CONNECTED
 *            \-> NET_ERROR --(retry)--> BOOT
 *   TCP_CONNECTED 掉线 / 连接失败 -> TCP_ERROR --(retry)--> TCP_CONNECTING
 *   TCP_ERROR 且 PHY down -> NET_ERROR（先恢复网络再建连）
 *
 * OTA 活动期间：仅喂狗与短延时，不主动重连，避免打断 SOIP 停等。
 */
#include "bsp_w5500_app.h"

#include "bsp_w5500_conf.h"
#include "bsp_w5500_net.h"
#include "bsp_w5500_tcp.h"
#include "bsp_log.h"
#include "bsp_ota_app.h"
#include "bsp_watchdog.h"

#include <stdio.h>

#include "cmsis_os.h"

#define W5500_APP_TAG   "W5500"

static volatile bsp_w5500_state_t s_state = BSP_W5500_STATE_OFF;
/** 最近一次成功获取的本机 IP 字符串，供查询接口使用 */
static char s_ip[20];

static uint8_t w5500_try_net(void);
static uint8_t w5500_try_tcp(void);

/**
 * @brief  更新应用层状态（可被 TCP 关闭通知等异步路径读取）
 */
static void w5500_set_state(bsp_w5500_state_t state)
{
    s_state = state;
}

/**
 * @brief  初始化网卡并等待链路稳定
 * @retval 1 成功进入 NET_READY
 * @retval 0 失败，状态为 NET_ERROR
 *
 * @note   初始化期间置长操作标志，避免看门狗误复位
 */
static uint8_t w5500_try_net(void)
{
    w5500_set_state(BSP_W5500_STATE_BOOT);
    BSP_Watchdog_Kick(BSP_WDG_W5500);
    BSP_Watchdog_SetLongOp(1U);

    if (bsp_w5500_net_init() != 0) {
        BSP_Watchdog_SetLongOp(0U);
        w5500_set_state(BSP_W5500_STATE_NET_ERROR);
        return 0U;
    }

    if (bsp_w5500_net_get_ip_str(s_ip, sizeof(s_ip)) != 0) {
        s_ip[0] = '\0';
    }

    BSP_Watchdog_SetLongOp(0U);
    w5500_set_state(BSP_W5500_STATE_NET_READY);
    LOG_I(W5500_APP_TAG, "net ready IP=%s PHY=%s",
          s_ip, bsp_w5500_net_phy_link_up() ? "up" : "down");
    /* 给 PHY/DHCP 留稳定窗口，减少立刻建连失败 */
    osDelay(BSP_W5500_LINK_STABLE_MS);
    return 1U;
}

/**
 * @brief  连接配置中的 OTA TCP Server
 * @retval 1 成功进入 TCP_CONNECTED
 * @retval 0 失败，状态为 TCP_ERROR
 */
static uint8_t w5500_try_tcp(void)
{
    w5500_set_state(BSP_W5500_STATE_TCP_CONNECTING);
    BSP_Watchdog_Kick(BSP_WDG_W5500);
    BSP_Watchdog_SetLongOp(1U);

    if (!bsp_w5500_tcp_connect(BSP_W5500_OTA_HOST, BSP_W5500_OTA_PORT)) {
        BSP_Watchdog_SetLongOp(0U);
        w5500_set_state(BSP_W5500_STATE_TCP_ERROR);
        LOG_W(W5500_APP_TAG, "TCP connect failed, host=%s:%u",
              BSP_W5500_OTA_HOST, (unsigned)BSP_W5500_OTA_PORT);
        return 0U;
    }

    BSP_Watchdog_SetLongOp(0U);
    w5500_set_state(BSP_W5500_STATE_TCP_CONNECTED);
    LOG_I(W5500_APP_TAG, "OTA link up %s:%u", BSP_W5500_OTA_HOST, (unsigned)BSP_W5500_OTA_PORT);
    return 1U;
}

/**
 * @brief  W5500 后台任务：启动链路并在空闲时维护重连
 */
static void W5500Task(void *argument)
{
    (void)argument;

    bsp_w5500_tcp_rx_notify_init();

    if (w5500_try_net() != 0U) {
        (void)w5500_try_tcp();
    }

    for (;;) {
        BSP_Watchdog_Kick(BSP_WDG_W5500);

        /* OTA 会话进行中：不重连、不长阻塞，把带宽留给 OTA 任务 poll */
        if (BSP_Ota_IsActive() != 0U) {
            osDelay(50);
            continue;
        }

        if (s_state == BSP_W5500_STATE_TCP_CONNECTED) {
            if (bsp_w5500_tcp_is_connected() == 0U) {
                LOG_W(W5500_APP_TAG, "TCP lost, reconnecting");
                w5500_set_state(BSP_W5500_STATE_TCP_ERROR);
            } else {
                osDelay(1000);
                continue;
            }
        }

        if (s_state == BSP_W5500_STATE_NET_ERROR) {
            osDelay(BSP_W5500_TCP_RETRY_MS);
            if (w5500_try_net() != 0U) {
                (void)w5500_try_tcp();
            }
        } else if (s_state == BSP_W5500_STATE_TCP_ERROR) {
            osDelay(BSP_W5500_TCP_RETRY_MS);
            if (bsp_w5500_net_phy_link_up() == 0U) {
                /* 线缆/PHY 掉了，先回到网络恢复流程 */
                w5500_set_state(BSP_W5500_STATE_NET_ERROR);
            } else {
                (void)w5500_try_tcp();
            }
        } else {
            osDelay(1000);
        }
    }
}

/**
 * @brief  创建 W5500 后台任务
 */
void bsp_w5500_app_start(void)
{
    const osThreadAttr_t attr = {
        .name = "w5500",
        .stack_size = BSP_W5500_TASK_STACK_WORDS * 4U,
        .priority = (osPriority_t)osPriorityNormal,
    };

    (void)osThreadNew(W5500Task, NULL, &attr);
}

/**
 * @brief  获取当前 W5500 应用状态
 */
bsp_w5500_state_t bsp_w5500_get_state(void)
{
    return s_state;
}

/**
 * @brief  读取本机 IPv4 字符串
 */
int bsp_w5500_get_ip(char *buf, size_t len)
{
    if ((buf == NULL) || (len == 0U)) {
        return -1;
    }

    if (s_ip[0] == '\0') {
        buf[0] = '\0';
        return -1;
    }

    (void)snprintf(buf, len, "%s", s_ip);
    return 0;
}

/**
 * @brief  查询应用层是否认为 OTA TCP 已连通
 */
uint8_t bsp_w5500_is_tcp_connected(void)
{
    return (s_state == BSP_W5500_STATE_TCP_CONNECTED) ? 1U : 0U;
}

/**
 * @brief  底层 TCP 状态异常通知（由 tcp 层在 poll/发送路径调用）
 */
void bsp_w5500_notify_tcp_closed(void)
{
    if (s_state == BSP_W5500_STATE_TCP_CONNECTED) {
        if (BSP_Ota_IsActive() != 0U) {
            /* OTA 中不改状态、不重连：误报 CLOSED 会导致停等 ACK 全丢 */
            LOG_W(W5500_APP_TAG, "TCP status not ESTABLISHED during OTA (ignored)");
            return;
        }
        LOG_W(W5500_APP_TAG, "TCP closed (peer or local timeout)");
        w5500_set_state(BSP_W5500_STATE_TCP_ERROR);
    }
}
