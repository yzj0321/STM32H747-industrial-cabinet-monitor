/**
 * @file bsp_app_data.h
 * @brief 应用层共享传感器数据（湿度、温度、ADC 电压）
 */
#ifndef BSP_APP_DATA_H
#define BSP_APP_DATA_H

#include <stdint.h>

/**
 * @brief  初始化应用数据互斥锁（RTOS 就绪后调用）
 * @retval 无
 */
void BSP_AppData_RtosInit(void);

/**
 * @brief  读取当前湿度值
 * @retval 湿度（与传感器驱动约定单位，通常为 %RH×10）
 */
uint16_t BSP_AppData_GetHumidity(void);

/**
 * @brief  读取当前温度值
 * @retval 温度（与传感器驱动约定单位，通常为 ℃×10）
 */
uint16_t BSP_AppData_GetTemperature(void);

/**
 * @brief  读取当前 ADC 电压
 * @retval 电压毫伏值（mV）
 */
uint16_t BSP_AppData_GetAdcMv(void);

/**
 * @brief  读取 CM4 已完成滞回判定的水浸状态
 * @retval 1=浸水告警，0=干燥
 */
uint8_t BSP_AppData_GetWet(void);

/**
 * @brief  更新湿度值
 * @param  value  湿度
 * @retval 无
 */
void BSP_AppData_SetHumidity(uint16_t value);

/**
 * @brief  更新温度值
 * @param  value  温度
 * @retval 无
 */
void BSP_AppData_SetTemperature(uint16_t value);

/**
 * @brief  更新 ADC 电压值
 * @param  value  电压（mV）
 * @retval 无
 */
void BSP_AppData_SetAdcMv(uint16_t value);

/**
 * @brief  更新 CM4 已完成滞回判定的水浸状态
 * @param  value  非 0 表示浸水
 */
void BSP_AppData_SetWet(uint8_t value);

#endif
