/**
 * @file    bsp_modbus_th.h
 * @brief   华控 SHT30 RS485 温湿度模块 Modbus RTU 读取
 */
#ifndef BSP_MODBUS_TH_H
#define BSP_MODBUS_TH_H

#include <stdint.h>

typedef struct {
    int16_t temperature_x10;
    uint16_t humidity_x10;
    uint8_t valid;
} bsp_th_sample_t;

/**
 * @brief  初始化温湿度 Modbus 通信（RS485）
 * @retval 0 成功，非 0 失败
 */
int BSP_Modbus_TH_Init(void);

/**
 * @brief  读取一次温湿度采样
 * @param  sample  输出采样结果
 * @retval 0 成功，非 0 失败
 */
int BSP_Modbus_TH_Read(bsp_th_sample_t *sample);

/**
 * @brief  启动温湿度周期采集任务（RTOS）
 */
void BSP_Modbus_TH_Start(void);

#endif /* BSP_MODBUS_TH_H */
