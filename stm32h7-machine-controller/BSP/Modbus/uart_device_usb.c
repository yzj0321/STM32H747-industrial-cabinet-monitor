/**
 * @file    uart_device_usb.c
 * @brief   USB 虚拟串口 UART 设备后端（libmodbus RTU）
 */
#include "uart_device.h"

#include "bsp_usb_modbus.h"

/**
 * @brief  USB 串口初始化（占位，参数由 USB 栈处理）
 * @param  pDev       设备指针
 * @param  baud       波特率（未使用）
 * @param  parity     校验位（未使用）
 * @param  data_bit   数据位（未使用）
 * @param  stop_bit   停止位（未使用）
 * @retval 0 成功
 */
static int USBSerial_Init(struct UART_Device *pDev, int baud, char parity, int data_bit, int stop_bit)
{
  (void)pDev;
  (void)baud;
  (void)parity;
  (void)data_bit;
  (void)stop_bit;
  return 0;
}

/**
 * @brief  通过 USB CDC 发送数据
 * @param  pDev     设备指针
 * @param  datas    待发送数据
 * @param  len      数据长度
 * @param  timeout  超时（毫秒）
 * @retval 0 成功；-1 失败
 */
static int USBSerial_Send(struct UART_Device *pDev, uint8_t *datas, uint32_t len, int timeout)
{
  (void)pDev;
  return BSP_USB_Modbus_Send(datas, len, (uint32_t)timeout);
}

/**
 * @brief  从 USB CDC 接收一字节
 * @param  pDev     设备指针
 * @param  pData    输出字节指针
 * @param  timeout  超时（毫秒）
 * @retval 0 成功；-1 失败
 */
static int USBSerial_RecvByte(struct UART_Device *pDev, uint8_t *pData, int timeout)
{
  (void)pDev;
  return BSP_USB_Modbus_RecvByte(pData, timeout);
}

/**
 * @brief  清空 USB 接收缓冲
 * @param  pDev  设备指针
 * @retval 丢弃的字节数
 */
static int USBSerial_Flush(struct UART_Device *pDev)
{
  (void)pDev;
  return BSP_USB_Modbus_Flush();
}

/** @brief 全局 USB 虚拟串口设备实例 */
struct UART_Device g_usbserial_dev = {
  "usb",
  USBSerial_Init,
  USBSerial_Send,
  USBSerial_RecvByte,
  USBSerial_Flush,
};
