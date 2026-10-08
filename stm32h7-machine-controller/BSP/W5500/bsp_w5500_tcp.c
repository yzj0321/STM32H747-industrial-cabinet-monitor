/**
 * @file    bsp_w5500_tcp.c
 * @brief   W5500 TCP 客户端：连接 OTA Server 并收发 SOIP 字节流
 *
 * 发送不走 ioLibrary send()（其 sock_is_sending/SENDOK 状态机易与停等 ACK 死锁），
 * 改用 wiz_send_data + Sn_CR_SEND + 等待 Sn_IR_SENDOK（WIZnet 官方推荐流程）。
 */
#include "bsp_w5500_tcp.h"

#include "bsp_w5500_app.h"
#include "bsp_w5500_conf.h"
#include "bsp_log.h"
#include "bsp_watchdog.h"
#include "main.h"
#include "socket.h"
#include "w5500.h"

#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"

#define W5500_TCP_TAG   "W5500-TCP"

/** RX 数据到达通知：poll_rx 写入 pending 后释放，wait_rx 阻塞等待 */
static osSemaphoreId_t s_tcp_rx_sem;
/** 保护 socket 寄存器访问与 pending 缓冲，避免 OTA 任务与发送路径并发冲突 */
static osMutexId_t s_tcp_mutex;

/** 从 W5500 RX FIFO 搬出的软件 pending 缓冲（SOIP 按字节消费） */
static uint8_t s_pending[BSP_W5500_TCP_RX_CHUNK];
static uint16_t s_pending_len;
static uint16_t s_pending_pos;

/** 应用认为连接生命周期有效（connect 成功后置 1，close 清 0） */
static uint8_t s_tcp_active;
/** CLOSED/CLOSE_WAIT 连续观测计数，防 SPI 毛刺误报断连 */
static uint8_t s_close_debounce;

/**
 * @brief  获取 TCP 会话锁
 */
static void tcp_lock(void)
{
    if (s_tcp_mutex != NULL) {
        (void)osMutexAcquire(s_tcp_mutex, osWaitForever);
    }
}

/**
 * @brief  释放 TCP 会话锁
 */
static void tcp_unlock(void)
{
    if (s_tcp_mutex != NULL) {
        (void)osMutexRelease(s_tcp_mutex);
    }
}

/**
 * @brief  尝试获取 TCP 会话锁
 */
static uint8_t tcp_try_lock(void)
{
    if (s_tcp_mutex == NULL) {
        return 1U;
    }
    return (osMutexAcquire(s_tcp_mutex, 0U) == osOK) ? 1U : 0U;
}

/**
 * @brief  通知有 TCP 接收数据
 */
static void tcp_signal_rx(void)
{
    if (s_tcp_rx_sem != NULL) {
        (void)osSemaphoreRelease(s_tcp_rx_sem);
    }
}

/**
 * @brief  排空 TCP RX 信号量
 */
static void tcp_drain_rx_sem(void)
{
    if (s_tcp_rx_sem == NULL) {
        return;
    }

    while (osSemaphoreAcquire(s_tcp_rx_sem, 0U) == osOK) {
    }
}

/**
 * @brief  复位 pending 接收状态
 */
static void tcp_reset_pending(void)
{
    s_pending_len = 0U;
    s_pending_pos = 0U;
}

/**
 * @brief  查询 pending 是否有数据
 */
static uint8_t tcp_pending_has_data(void)
{
    return (s_pending_pos < s_pending_len) ? 1U : 0U;
}

/**
 * @brief  解析 IPv4 字符串
 */
static uint8_t tcp_parse_ipv4(const char *host, uint8_t out[4])
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;

    if ((host == NULL) || (out == NULL)) {
        return 0U;
    }

    if (sscanf(host, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return 0U;
    }

    if ((a > 255U) || (b > 255U) || (c > 255U) || (d > 255U)) {
        return 0U;
    }

    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
    return 1U;
}

/**
 * @brief  连续读三次 Sn_SR，取多数一致值，降低 SPI 偶发误读
 */
static uint8_t tcp_read_socket_status(uint8_t sn)
{
    uint8_t s1 = getSn_SR(sn);
    uint8_t s2 = getSn_SR(sn);
    uint8_t s3 = getSn_SR(sn);

    if ((s1 == s2) || (s1 == s3)) {
        return s1;
    }
    if (s2 == s3) {
        return s2;
    }

    return s1;
}

/**
 * @brief  等待 socket 进入 ESTABLISHED，超时或 CLOSED/CLOSE_WAIT 则失败
 */
static uint8_t tcp_wait_established(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t sn = BSP_W5500_OTA_SOCKET;

    for (;;) {
        uint8_t status = tcp_read_socket_status(sn);

        if (status == SOCK_ESTABLISHED) {
            return 1U;
        }

        if ((status == SOCK_CLOSED) || (status == SOCK_CLOSE_WAIT)) {
            return 0U;
        }

        if ((HAL_GetTick() - start) >= timeout_ms) {
            return 0U;
        }

        bsp_w5500_tcp_poll_rx();
        BSP_Watchdog_Kick(BSP_WDG_W5500);
        osDelay(10);
    }
}

/**
 * @brief  WIZnet 官方发送流程（绕过 socket.c send/sock_is_sending）
 *
 * 步骤：清 SENDOK/TIMEOUT IR -> 等 TX_FSR -> wiz_send_data -> Sn_CR_SEND。
 * 不阻塞等待 SENDOK：SOIP 停等 ACK 已保证可靠，避免 SPI 误读导致假超时。
 */
static uint8_t tcp_send_raw(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint8_t sn = BSP_W5500_OTA_SOCKET;
    uint32_t start = HAL_GetTick();
    uint8_t ir;

    if ((data == NULL) || (len == 0U)) {
        return 0U;
    }

    tcp_lock();

    if (tcp_read_socket_status(sn) != SOCK_ESTABLISHED) {
        tcp_unlock();
        return 0U;
    }

    /* 清残留中断 */
    ir = getSn_IR(sn);
    if (ir & (Sn_IR_SENDOK | Sn_IR_TIMEOUT)) {
        setSn_IR(sn, (uint8_t)(ir & (Sn_IR_SENDOK | Sn_IR_TIMEOUT)));
    }

    /* 等 TX 空闲空间 */
    while (getSn_TX_FSR(sn) < len) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            tcp_unlock();
            LOG_E(W5500_TCP_TAG, "TX full timeout len=%u", (unsigned)len);
            return 0U;
        }
        if (tcp_read_socket_status(sn) != SOCK_ESTABLISHED) {
            tcp_unlock();
            return 0U;
        }
        tcp_unlock();
        osDelay(1);
        tcp_lock();
    }

    wiz_send_data(sn, (uint8_t *)data, len);
    setSn_CR(sn, Sn_CR_SEND);
    while (getSn_CR(sn) != 0U) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            tcp_unlock();
            LOG_E(W5500_TCP_TAG, "SEND cmd timeout");
            return 0U;
        }
    }

    /*
     * CR 清零即已交给 W5500 硬件发送。应用层停等保证可靠，
     * 不阻塞等 SENDOK（SPI 误读 IR / delayed-ACK 会造成假超时丢 ACK）。
     */
    ir = getSn_IR(sn);
    if (ir & Sn_IR_TIMEOUT) {
        setSn_IR(sn, Sn_IR_TIMEOUT);
        tcp_unlock();
        LOG_E(W5500_TCP_TAG, "SEND TIMEOUT IR len=%u", (unsigned)len);
        return 0U;
    }
    if (ir & Sn_IR_SENDOK) {
        setSn_IR(sn, Sn_IR_SENDOK);
    }

    tcp_unlock();
    return 1U;
}

/**
 * @brief  初始化 TCP RX 信号量与互斥锁（幂等，可重复调用）
 */
void bsp_w5500_tcp_rx_notify_init(void)
{
    if (s_tcp_rx_sem != NULL) {
        return;
    }

    if (osKernelGetState() == osKernelInactive) {
        return;
    }

    s_tcp_rx_sem = osSemaphoreNew(BSP_W5500_TCP_RX_SEM_MAX, 0U, NULL);
    if (s_tcp_mutex == NULL) {
        s_tcp_mutex = osMutexNew(NULL);
    }
}

/**
 * @brief  轮询 socket RX，搬入 pending；连续 3 次观测到关闭则通知应用层
 */
void bsp_w5500_tcp_poll_rx(void)
{
    uint8_t sn = BSP_W5500_OTA_SOCKET;
    uint8_t status;
    int32_t avail;
    uint16_t chunk;
    int32_t got;

    if (s_tcp_active == 0U) {
        return;
    }

    /* 发送路径持锁时跳过，避免阻塞 OTA 任务过久 */
    if (tcp_try_lock() == 0U) {
        return;
    }

    status = tcp_read_socket_status(sn);
    if (status != SOCK_ESTABLISHED) {
        if ((status == SOCK_CLOSED) || (status == SOCK_CLOSE_WAIT)) {
            if (s_close_debounce < 255U) {
                s_close_debounce++;
            }
            tcp_unlock();
            if (s_close_debounce >= 3U) {
                s_close_debounce = 0U;
                bsp_w5500_notify_tcp_closed();
            }
            return;
        }
        s_close_debounce = 0U;
        tcp_unlock();
        return;
    }

    s_close_debounce = 0U;
    avail = getSn_RX_RSR(sn);
    if (avail <= 0) {
        tcp_unlock();
        return;
    }

    while (avail > 0) {
        chunk = (uint16_t)((avail > 256) ? 256 : avail);
        if ((s_pending_len + chunk) > BSP_W5500_TCP_RX_CHUNK) {
            LOG_E(W5500_TCP_TAG, "RX pending full (%u)", (unsigned)BSP_W5500_TCP_RX_CHUNK);
            break;
        }

        got = recv(sn, &s_pending[s_pending_len], chunk);
        if (got <= 0) {
            break;
        }

        s_pending_len = (uint16_t)(s_pending_len + (uint16_t)got);
        avail = getSn_RX_RSR(sn);
        tcp_signal_rx();
    }

    tcp_unlock();
}

/**
 * @brief  先 poll，若 pending 为空则阻塞等待 RX 信号量
 */
int bsp_w5500_tcp_wait_rx(uint32_t timeout_ms)
{
    bsp_w5500_tcp_poll_rx();

    if (tcp_pending_has_data() != 0U) {
        return 0;
    }

    if (s_tcp_rx_sem == NULL) {
        osDelay(timeout_ms);
        bsp_w5500_tcp_poll_rx();
        return -1;
    }

    if (osSemaphoreAcquire(s_tcp_rx_sem, timeout_ms) == osOK) {
        bsp_w5500_tcp_poll_rx();
        return 0;
    }

    return -1;
}

/**
 * @brief  非阻塞从 pending 取 1 字节；取空后复位读写指针
 */
int bsp_w5500_tcp_try_recv_byte(uint8_t *byte)
{
    if (byte == NULL) {
        return -1;
    }

    bsp_w5500_tcp_poll_rx();

    if (s_pending_pos < s_pending_len) {
        *byte = s_pending[s_pending_pos++];
        if (s_pending_pos >= s_pending_len) {
            s_pending_len = 0U;
            s_pending_pos = 0U;
        }
        return 0;
    }

    return -1;
}

/**
 * @brief  在 timeout_ms 内循环 try_recv / wait_rx，直到读到 1 字节
 */
int bsp_w5500_tcp_recv_byte(uint8_t *byte, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    if (byte == NULL) {
        return -1;
    }

    for (;;) {
        if (bsp_w5500_tcp_try_recv_byte(byte) == 0) {
            return 0;
        }

        if ((HAL_GetTick() - start) >= timeout_ms) {
            return -1;
        }

        BSP_Watchdog_Kick(BSP_WDG_W5500);
        if (bsp_w5500_tcp_wait_rx(timeout_ms - (HAL_GetTick() - start)) != 0) {
            if (bsp_w5500_tcp_try_recv_byte(byte) == 0) {
                return 0;
            }
            return -1;
        }
    }
}

/**
 * @brief  同时检查应用活跃标志与硬件 ESTABLISHED
 */
uint8_t bsp_w5500_tcp_is_connected(void)
{
    if (s_tcp_active == 0U) {
        return 0U;
    }

    return (tcp_read_socket_status(BSP_W5500_OTA_SOCKET) == SOCK_ESTABLISHED) ? 1U : 0U;
}

/**
 * @brief  断开并关闭 socket，清空 pending 与 RX 信号量计数
 */
uint8_t bsp_w5500_tcp_close(void)
{
    uint8_t sn = BSP_W5500_OTA_SOCKET;

    s_tcp_active = 0U;
    tcp_reset_pending();
    tcp_drain_rx_sem();

    tcp_lock();
    (void)disconnect(sn);
    (void)close(sn);
    tcp_unlock();

    return 1U;
}

/**
 * @brief  解析 IPv4 后打开 TCP socket 并 connect，成功后置 s_tcp_active
 */
uint8_t bsp_w5500_tcp_connect(const char *host, uint16_t port)
{
    uint8_t sn = BSP_W5500_OTA_SOCKET;
    uint8_t dest_ip[4];
    int8_t ret;

    if ((host == NULL) || (host[0] == '\0') || (port == 0U)) {
        return 0U;
    }

    if (tcp_parse_ipv4(host, dest_ip) == 0U) {
        LOG_E(W5500_TCP_TAG, "invalid host %s", host);
        return 0U;
    }

    if (bsp_w5500_tcp_is_connected() != 0U) {
        LOG_I(W5500_TCP_TAG, "TCP already connected");
        return 1U;
    }

    (void)bsp_w5500_tcp_close();

    tcp_lock();

    /* 非阻塞 connect：避免 WIZnet 阻塞式空转饿死 logSd 等任务 */
    ret = socket(sn, Sn_MR_TCP, 0U, (uint8_t)(SF_TCP_NODELAY | SF_IO_NONBLOCK));
    if (ret != (int8_t)sn) {
        LOG_E(W5500_TCP_TAG, "socket open failed ret=%d", (int)ret);
        tcp_unlock();
        return 0U;
    }

    ret = connect(sn, dest_ip, port);
    tcp_unlock();

    if ((ret != SOCK_OK) && (ret != SOCK_BUSY)) {
        LOG_E(W5500_TCP_TAG, "connect failed ret=%d", (int)ret);
        (void)bsp_w5500_tcp_close();
        return 0U;
    }

    if (tcp_wait_established(BSP_W5500_TCP_CONNECT_TIMEOUT_MS) == 0U) {
        LOG_E(W5500_TCP_TAG, "connect timeout -> %s:%u", host, (unsigned)port);
        (void)bsp_w5500_tcp_close();
        return 0U;
    }

    s_tcp_active = 1U;
    s_close_debounce = 0U;
    tcp_reset_pending();
    tcp_drain_rx_sem();
    LOG_I(W5500_TCP_TAG, "connected to OTA host %s:%u", host, (unsigned)port);
    return 1U;
}

/**
 * @brief  已连接前提下单次 tcp_send_raw
 */
uint8_t bsp_w5500_tcp_send_once(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (bsp_w5500_tcp_is_connected() == 0U) {
        return 0U;
    }
    return tcp_send_raw(data, len, timeout_ms);
}

/**
 * @brief  带重试的发送；短帧用更短超时以适配 ACK 场景
 */
uint8_t bsp_w5500_tcp_send(const uint8_t *data, uint16_t len)
{
    uint8_t attempt;
    const uint32_t timeout_ms =
        (len <= 128U) ? BSP_W5500_OTA_ACK_TRY_MS : BSP_W5500_OTA_SEND_TIMEOUT_MS;

    if ((data == NULL) || (len == 0U)) {
        return 0U;
    }

    for (attempt = 0U; attempt < BSP_W5500_SEND_RETRIES; attempt++) {
        if (attempt > 0U) {
            bsp_w5500_tcp_poll_rx();
            osDelay(2U);
        }

        if (tcp_send_raw(data, len, timeout_ms) != 0U) {
            return 1U;
        }
    }

    return 0U;
}

/**
 * @brief  OTA 链路恢复入口（当前禁用主动重连，避免会话中断）
 * @retval 恒为 0（未恢复）
 */
uint8_t bsp_w5500_tcp_recover_ota_link(void)
{
    LOG_W(W5500_TCP_TAG, "OTA TCP recover disabled (keep link)");
    return 0U;
}
