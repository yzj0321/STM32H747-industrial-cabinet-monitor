/**
 * @file    bsp_modbus_app.h
 * @brief   Modbus 从站应用层接口（线圈/保持寄存器映射与任务启动）
 */
#ifndef BSP_MODBUS_APP_H
#define BSP_MODBUS_APP_H

#include <stdint.h>

#define MODBUS_SLAVE_ID 1U

/* Coil map (FC01/FC05/FC0F, Poll address 00000..00009 = index 0..9) */
#define MODBUS_COIL_COUNT       10U
#define MODBUS_COIL_INDEX_MAX   9U
#define MODBUS_COIL_LED_PA3     0U   /* Write single coil 00000 -> PA3 LED */

/* Holding register map (FC03, address = index) */
#define MODBUS_REG_HUMIDITY     0U
#define MODBUS_REG_TEMPERATURE  1U
#define MODBUS_REG_ADC_MV       2U   /* ADC voltage, unit: mV, Vref=3300 */
#define MODBUS_REG_WET          3U   /* CM4 hysteresis result: 1=wet/alarm */

/**
 * @brief  初始化 Modbus 数据映射与线圈持久化后台任务（不依赖 USB）
 */
void BSP_Modbus_Data_Init(void);

/**
 * @brief  启动 USB Modbus RTU 从站服务任务（需在 USB 栈就绪后调用）
 */
void BSP_Modbus_App_Start(void);

/**
 * @brief  设置保持寄存器值并同步到应用数据
 * @param  index  寄存器索引
 * @param  value  寄存器值
 */
void BSP_Modbus_SetHoldingRegister(uint16_t index, uint16_t value);

/**
 * @brief  读取保持寄存器值
 * @param  index  寄存器索引
 * @retval 寄存器值，无效索引返回 0
 */
uint16_t BSP_Modbus_GetHoldingRegister(uint16_t index);

/**
 * @brief  设置线圈状态
 * @param  index  线圈索引
 * @param  value  非 0 为 ON，0 为 OFF
 */
void BSP_Modbus_SetCoil(uint16_t index, uint8_t value);

/**
 * @brief  读取线圈状态
 * @param  index  线圈索引
 * @retval 线圈值（0 或 1），无效索引返回 0
 */
uint8_t BSP_Modbus_GetCoil(uint16_t index);

#endif
