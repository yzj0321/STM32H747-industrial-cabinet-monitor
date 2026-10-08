/**
 * @file    bsp_w5500_app.h
 * @brief   W5500 FreeRTOS 任务与应用状态
 *
 * 负责：上电初始化以太网、连接 OTA TCP Server，并在链路异常时自动重试。
 * OTA 会话活动期间本任务降低巡检频率，且忽略误报的 TCP CLOSED，避免打断停等 ACK。
 */
#ifndef BSP_W5500_APP_H
#define BSP_W5500_APP_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief  W5500 应用层链路状态
 */
typedef enum {
    BSP_W5500_STATE_OFF = 0,           /**< 任务未启动或尚未进入启动流程 */
    BSP_W5500_STATE_BOOT,              /**< 正在初始化网卡 / DHCP */
    BSP_W5500_STATE_NET_READY,         /**< 以太网已就绪，尚未建立 OTA TCP */
    BSP_W5500_STATE_NET_ERROR,         /**< 网卡或 PHY 失败，等待重试 */
    BSP_W5500_STATE_TCP_CONNECTING,    /**< 正在连接 OTA 主机 */
    BSP_W5500_STATE_TCP_CONNECTED,     /**< OTA TCP 已连通，可收发 SOIP */
    BSP_W5500_STATE_TCP_ERROR,         /**< TCP 断开或连接失败，等待重连 */
} bsp_w5500_state_t;

/**
 * @brief  创建 W5500 后台任务（网络初始化 + OTA TCP 维护）
 *
 * 应在 RTOS 调度开始后、OTA 任务启动前或同期调用一次。
 */
void bsp_w5500_app_start(void);

/**
 * @brief  获取当前 W5500 应用状态
 * @retval 当前 bsp_w5500_state_t
 */
bsp_w5500_state_t bsp_w5500_get_state(void);

/**
 * @brief  读取本机 IPv4 字符串（点分十进制）
 * @param  buf 输出缓冲区
 * @param  len 缓冲区容量
 * @retval 0 成功
 * @retval -1 参数无效或尚未获得 IP
 */
int bsp_w5500_get_ip(char *buf, size_t len);

/**
 * @brief  查询 OTA TCP 是否处于已连接应用状态
 * @retval 1 已连接（STATE_TCP_CONNECTED）
 * @retval 0 未连接
 */
uint8_t bsp_w5500_is_tcp_connected(void);

/**
 * @brief  底层检测到 TCP 非 ESTABLISHED 时的通知入口
 *
 * OTA 会话活动中会忽略误报，防止停等 ACK 路径被重连打断；
 * 空闲时将状态切到 TCP_ERROR 以触发自动重连。
 */
void bsp_w5500_notify_tcp_closed(void);

#endif /* BSP_W5500_APP_H */
