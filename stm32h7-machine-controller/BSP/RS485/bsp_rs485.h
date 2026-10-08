/**
 * @file    bsp_rs485.h
 * @brief   MAX485 RS485 半双工驱动
 */
#ifndef BSP_RS485_H
#define BSP_RS485_H

#include <stdint.h>

/**
 * @brief  初始化 RS485 方向控制引脚并进入接收模式
 */
void BSP_Rs485_Init(void);

/**
 * @brief  切换为接收模式（RE=0, DE=0）
 */
void BSP_Rs485_SetRxMode(void);

/**
 * @brief  切换为发送模式（RE=1, DE=1）
 */
void BSP_Rs485_SetTxMode(void);

/**
 * @brief  发送数据（内部自动切换收发方向）
 * @param  data     数据指针
 * @param  len      数据长度
 * @param  timeout  超时（毫秒）
 * @retval HAL 状态
 */
int BSP_Rs485_Send(const uint8_t *data, uint16_t len, uint32_t timeout);

/**
 * @brief  发送以 '\\0' 结尾的字符串
 * @param  str  字符串指针
 * @retval HAL 状态
 */
int BSP_Rs485_SendString(const char *str);

/**
 * @brief  设置 RS485 UART 波特率
 * @param  baud  波特率
 * @retval HAL 状态
 */
int BSP_Rs485_SetBaud(uint32_t baud);

/**
 * @brief  接收单字节
 * @param  data         输出字节指针
 * @param  timeout_ms   超时（毫秒）
 * @retval 0 成功；-1 失败
 */
int BSP_Rs485_RecvByte(uint8_t *data, uint32_t timeout_ms);

/**
 * @brief  清空接收缓冲
 * @retval 丢弃的字节数
 */
int BSP_Rs485_Flush(void);

#endif /* BSP_RS485_H */
