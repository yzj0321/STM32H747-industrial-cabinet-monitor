/**
 * @file    uart_device_rs485.c
 * @brief   RS485 UART 设备后端（libmodbus RTU）
 */
#include "uart_device.h"

#include "bsp_rs485.h"

/**
 * @brief  RS485 串口设备初始化（uart_device 适配）
 */
static int RS485Serial_Init(struct UART_Device *pDev, int baud, char parity, int data_bit, int stop_bit)
{
    (void)pDev;
    (void)parity;
    (void)data_bit;
    (void)stop_bit;
    return BSP_Rs485_SetBaud((uint32_t)baud);
}

/**
 * @brief  RS485 串口设备发送
 */
static int RS485Serial_Send(struct UART_Device *pDev, uint8_t *datas, uint32_t len, int timeout)
{
    (void)pDev;
    return (BSP_Rs485_Send(datas, (uint16_t)len, (uint32_t)timeout) == 0) ? 0 : -1;
}

/**
 * @brief  RS485 串口设备收字节
 */
static int RS485Serial_RecvByte(struct UART_Device *pDev, uint8_t *pData, int timeout)
{
    (void)pDev;
    return BSP_Rs485_RecvByte(pData, (uint32_t)timeout);
}

/**
 * @brief  RS485 串口设备清缓冲
 */
static int RS485Serial_Flush(struct UART_Device *pDev)
{
    (void)pDev;
    return BSP_Rs485_Flush();
}

struct UART_Device g_rs485_dev = {
    "rs485",
    RS485Serial_Init,
    RS485Serial_Send,
    RS485Serial_RecvByte,
    RS485Serial_Flush,
};
