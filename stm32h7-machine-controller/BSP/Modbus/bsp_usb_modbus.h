/**
 * @file    bsp_usb_modbus.h
 * @brief   USB CDC Modbus 收发桥接层
 */
#ifndef BSP_USB_MODBUS_H
#define BSP_USB_MODBUS_H

#include <stdint.h>

/**
 * @brief  初始化 USB Modbus 接收队列
 */
void BSP_USB_Modbus_Init(void);

/**
 * @brief  USB CDC 接收回调（ISR 上下文入队）
 * @param  buf  接收数据指针
 * @param  len  数据长度
 */
void BSP_USB_Modbus_OnRx(uint8_t *buf, uint32_t len);

/**
 * @brief  USB CDC 发送完成回调（预留扩展）
 */
void BSP_USB_Modbus_OnTxComplete(void);

/**
 * @brief  通过 USB CDC 发送数据
 * @param  data        待发送数据
 * @param  len         数据长度
 * @param  timeout_ms  超时时间（毫秒）
 * @retval 0 成功；-1 失败
 */
int BSP_USB_Modbus_Send(uint8_t *data, uint32_t len, uint32_t timeout_ms);

/**
 * @brief  从接收队列读取一字节
 * @param  data        输出字节指针
 * @param  timeout_ms  超时时间（毫秒）
 * @retval 0 成功；-1 超时或错误
 */
int BSP_USB_Modbus_RecvByte(uint8_t *data, int timeout_ms);

/**
 * @brief  清空接收队列
 * @retval 丢弃的字节数
 */
int BSP_USB_Modbus_Flush(void);

#endif
