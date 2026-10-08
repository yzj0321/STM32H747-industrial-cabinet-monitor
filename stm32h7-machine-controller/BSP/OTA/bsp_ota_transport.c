/**
 * @file    bsp_ota_transport.c
 * @brief   OTA 传输层实现，封装 W5500 TCP 收发
 *
 * 本层将 OTA 状态机与具体网卡解耦：上层只依赖本文件 API，
 * 底层当前映射到 bsp_w5500_tcp / bsp_w5500_app。
 * 写路径在未建立 TCP 时直接失败；读路径由 OTA 任务配合 poll + wait_rx 使用。
 */
#include "bsp_ota_transport.h"

#include "bsp_w5500_app.h"
#include "bsp_w5500_tcp.h"

/**
 * @brief  初始化 OTA 传输层
 *
 * 当前无额外状态；TCP RX 信号量由 bsp_w5500_tcp_rx_notify_init
 * 在 RTOS 任务启动后创建。
 */
void bsp_ota_transport_init(void)
{
}

/**
 * @brief  轮询 TCP 接收，将 socket RX 数据写入 pending 缓冲
 *
 * 应在 OTA 任务循环中定期调用，尤其是 DATA 停等阶段需密集 poll，
 * 避免 W5500 RX 积压。
 */
void bsp_ota_transport_poll(void)
{
    bsp_w5500_tcp_poll_rx();
}

/**
 * @brief  阻塞等待 TCP RX 信号量
 * @param  timeout_ms 超时时间（毫秒）
 * @retval 0 有数据可读
 * @retval -1 超时或无连接
 */
int bsp_ota_transport_wait_rx(uint32_t timeout_ms)
{
    return bsp_w5500_tcp_wait_rx(timeout_ms);
}

/**
 * @brief  非阻塞读取 1 字节 SOIP 载荷
 * @param  byte 输出字节指针
 * @retval 0 成功
 * @retval -1 无数据或参数无效
 */
int bsp_ota_transport_try_read(uint8_t *byte)
{
    return bsp_w5500_tcp_try_recv_byte(byte);
}

/**
 * @brief  查询 TCP 连接是否就绪（以应用层状态为准）
 * @retval 1 已连接
 * @retval 0 未连接
 */
uint8_t bsp_ota_transport_ready(void)
{
    return bsp_w5500_is_tcp_connected();
}

/**
 * @brief  阻塞读取 1 字节
 * @param  byte       输出字节指针
 * @param  timeout_ms 超时时间（毫秒）
 * @retval 0 成功
 * @retval -1 失败（空指针、未连接或超时）
 */
int bsp_ota_transport_read(uint8_t *byte, uint32_t timeout_ms)
{
    if (byte == NULL) {
        return -1;
    }

    if (!bsp_w5500_is_tcp_connected()) {
        return -1;
    }

    return bsp_w5500_tcp_recv_byte(byte, timeout_ms);
}

/**
 * @brief  发送数据（带内部重试）
 * @param  data 待发送缓冲区
 * @param  len  字节数
 * @retval 0 成功
 * @retval -1 失败（参数无效、未连接或发送失败）
 */
int bsp_ota_transport_write(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    if (!bsp_w5500_is_tcp_connected()) {
        return -1;
    }

    return bsp_w5500_tcp_send(data, len) ? 0 : -1;
}

/**
 * @brief  单次发送数据（指定超时，不在本层重试）
 * @param  data       待发送缓冲区
 * @param  len        字节数
 * @param  timeout_ms 单次发送超时（毫秒）
 * @retval 0 成功
 * @retval -1 失败
 */
int bsp_ota_transport_write_once(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if ((data == NULL) || (len == 0U)) {
        return -1;
    }

    if (!bsp_w5500_is_tcp_connected()) {
        return -1;
    }

    return bsp_w5500_tcp_send_once(data, len, timeout_ms) ? 0 : -1;
}

/**
 * @brief  TCP 连续发送失败时重连（尽量保持 OTA 会话上下文）
 * @retval 0 重连成功
 * @retval -1 重连失败
 */
int bsp_ota_transport_recover_link(void)
{
    return bsp_w5500_tcp_recover_ota_link() ? 0 : -1;
}
