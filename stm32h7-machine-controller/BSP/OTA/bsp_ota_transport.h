/**
 * @file    bsp_ota_transport.h
 * @brief   OTA 底层传输抽象（W5500 TCP）
 */
#ifndef BSP_OTA_TRANSPORT_H
#define BSP_OTA_TRANSPORT_H

#include <stdint.h>

/**
 * @brief  初始化 OTA 传输层（信号量由 RTOS 启动后创建）
 */
void bsp_ota_transport_init(void);

/**
 * @brief  轮询 TCP 接收，将 +IPD 数据写入环形缓冲
 */
void bsp_ota_transport_poll(void);

/**
 * @brief  阻塞等待 TCP RX 信号量
 * @param  timeout_ms 超时时间（毫秒）
 * @retval 0 有数据可读
 * @retval -1 超时或无连接
 */
int bsp_ota_transport_wait_rx(uint32_t timeout_ms);

/**
 * @brief  非阻塞读取 1 字节 SOIP 载荷
 * @param  byte 输出字节指针
 * @retval 0 成功
 * @retval -1 无数据或参数无效
 */
int bsp_ota_transport_try_read(uint8_t *byte);

/**
 * @brief  查询 TCP 连接是否就绪
 * @retval 1 已连接
 * @retval 0 未连接
 */
uint8_t bsp_ota_transport_ready(void);

/**
 * @brief  阻塞读取 1 字节
 * @param  byte       输出字节指针
 * @param  timeout_ms 超时时间（毫秒）
 * @retval 0 成功
 * @retval -1 失败
 */
int bsp_ota_transport_read(uint8_t *byte, uint32_t timeout_ms);

/**
 * @brief  发送数据（带内部重试）
 * @param  data 待发送缓冲区
 * @param  len  字节数
 * @retval 0 成功
 * @retval -1 失败
 */
int bsp_ota_transport_write(const uint8_t *data, uint16_t len);

/**
 * @brief  单次发送数据（指定超时）
 * @param  data       待发送缓冲区
 * @param  len        字节数
 * @param  timeout_ms 单次发送超时（毫秒）
 * @retval 0 成功
 * @retval -1 失败
 */
int bsp_ota_transport_write_once(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief  TCP 连续发送失败时重连（保持 OTA 会话）
 * @retval 0 重连成功
 * @retval -1 重连失败
 */
int bsp_ota_transport_recover_link(void);

#endif /* BSP_OTA_TRANSPORT_H */
