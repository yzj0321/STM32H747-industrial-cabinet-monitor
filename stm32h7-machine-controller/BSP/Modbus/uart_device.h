/**
 * @file    uart_device.h
 * @brief   抽象 UART 设备接口（供 libmodbus 后端使用）
 */
#ifndef UART_DEVICE_H
#define UART_DEVICE_H

#include <stdint.h>

/** @brief UART 设备操作函数表 */
struct UART_Device {
  char *name;
  int (*Init)(struct UART_Device *pDev, int baud, char parity, int data_bit, int stop_bit);
  int (*Send)(struct UART_Device *pDev, uint8_t *datas, uint32_t len, int timeout);
  int (*RecvByte)(struct UART_Device *pDev, uint8_t *data, int timeout);
  int (*Flush)(struct UART_Device *pDev);
};

/**
 * @brief  按名称查找 UART 设备
 * @param  name  设备名称字符串
 * @retval 设备指针，未找到返回 NULL
 */
struct UART_Device *GetUARTDevice(char *name);

#endif
