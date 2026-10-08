/**
 * @file    bsp_w5500_tcp.h
 * @brief   W5500 TCP 客户端（连接 OTA 主机 PC）
 *
 * 为 SOIP OTA 提供字节流收发：发送走 wiz_send_data + SENDOK；
 * 接收经 pending 小缓冲 + 信号量通知，供 OTA 任务 poll/wait 使用。
 */
#ifndef BSP_W5500_TCP_H
#define BSP_W5500_TCP_H

#include <stdint.h>

/**
 * @brief  初始化 TCP 接收通知信号量与互斥锁
 *
 * 可在多个任务启动路径重复调用；内部只创建一次。
 */
void bsp_w5500_tcp_rx_notify_init(void);

/**
 * @brief  建立 TCP 连接到指定主机与端口
 * @param  host  目标 IPv4 字符串（如 "192.168.1.10"）
 * @param  port  目标端口
 * @retval 1 成功进入 ESTABLISHED
 * @retval 0 失败（参数、socket 或超时）
 */
uint8_t bsp_w5500_tcp_connect(const char *host, uint16_t port);

/**
 * @brief  关闭当前 TCP 连接并清空 pending 接收缓冲
 * @retval 1 成功
 * @retval 0 失败
 */
uint8_t bsp_w5500_tcp_close(void);

/**
 * @brief  判断 TCP socket 是否仍处于已连接状态
 * @retval 1 已连接
 * @retval 0 未连接
 */
uint8_t bsp_w5500_tcp_is_connected(void);

/**
 * @brief  通过 W5500 socket 发送 TCP 数据（含有限次重试）
 * @param  data 待发送缓冲区
 * @param  len  字节数
 * @retval 1 成功
 * @retval 0 失败
 */
uint8_t bsp_w5500_tcp_send(const uint8_t *data, uint16_t len);

/**
 * @brief  单次发送（指定超时，不做本函数内重试循环）
 * @param  data       待发送缓冲区
 * @param  len        字节数
 * @param  timeout_ms 等待 SENDOK 的超时（毫秒）
 * @retval 1 成功
 * @retval 0 失败
 */
uint8_t bsp_w5500_tcp_send_once(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief  OTA 期间连续发送失败时关闭并重连 PC Server
 * @retval 1 重连成功
 * @retval 0 重连失败
 */
uint8_t bsp_w5500_tcp_recover_ota_link(void);

/**
 * @brief  阻塞接收 1 字节 TCP payload
 * @param  byte       输出字节
 * @param  timeout_ms 超时（毫秒）
 * @retval 0 成功
 * @retval -1 失败或超时
 */
int bsp_w5500_tcp_recv_byte(uint8_t *byte, uint32_t timeout_ms);

/**
 * @brief  非阻塞取 1 字节 TCP payload（来自 pending 缓冲）
 * @param  byte 输出字节
 * @retval 0 成功
 * @retval -1 无数据或参数无效
 */
int bsp_w5500_tcp_try_recv_byte(uint8_t *byte);

/**
 * @brief  阻塞等待 TCP 接收数据或超时
 * @param  timeout_ms 超时（毫秒）
 * @retval 0 有数据（或 pending 非空）
 * @retval -1 超时
 */
int bsp_w5500_tcp_wait_rx(uint32_t timeout_ms);

/**
 * @brief  轮询 socket RX 缓冲并写入 pending 队列，有数据时释放 RX 信号量
 *
 * 由 OTA 任务在循环中调用；DATA 阶段应提高调用频率。
 */
void bsp_w5500_tcp_poll_rx(void);

#endif /* BSP_W5500_TCP_H */
