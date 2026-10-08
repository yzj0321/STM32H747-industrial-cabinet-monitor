/**
 * @file    bsp_rs485_conf.h
 * @brief   RS485 (MAX485) 硬件引脚与 UART 配置
 */
#ifndef BSP_RS485_CONF_H
#define BSP_RS485_CONF_H

#include "usart.h"

/** USART6: PG14=TX, PG9=RX */
#define BSP_RS485_UART_HANDLE       huart6

/** DE: 发送使能，高电平有效 */
#define BSP_RS485_DE_GPIO_PORT      GPIOG
#define BSP_RS485_DE_GPIO_PIN       GPIO_PIN_6

/** RE: 接收使能，低电平有效 */
#define BSP_RS485_RE_GPIO_PORT      GPIOA
#define BSP_RS485_RE_GPIO_PIN       GPIO_PIN_15

/** Modbus 温湿度轮询间隔（毫秒） */
#define BSP_RS485_POLL_INTERVAL_MS  1000U

/** 华控 SHT30 RS485 温湿度模块默认参数 */
#define BSP_RS485_MODBUS_BAUD       9600
#define BSP_RS485_MODBUS_PARITY     'N'
#define BSP_RS485_MODBUS_DATA_BITS  8
#define BSP_RS485_MODBUS_STOP_BITS  1
#define BSP_RS485_MODBUS_SLAVE_ID   1U
#define BSP_RS485_MODBUS_REG_TEMP   0U
#define BSP_RS485_MODBUS_REG_HUMI   1U

#endif /* BSP_RS485_CONF_H */
