/**
 * @file    bsp_adc_immersion.h
 * @brief   水浸传感器 ADC 采样（SMA + 滞回）并通过 OpenAMP 上报
 */
#ifndef BSP_ADC_IMMERSION_H
#define BSP_ADC_IMMERSION_H

#include <stdint.h>

typedef struct {
    uint16_t raw;       /**< 滤波后 ADC 原始码值 */
    uint16_t mv;        /**< 换算电压（mV） */
    uint8_t wet;        /**< 1=浸水，0=干燥 */
    uint8_t valid;      /**< 本次采样是否有效 */
} bsp_adc_imm_sample_t;

/**
 * @brief  校准 ADC 并初始化滤波状态
 * @retval 0 成功，负值失败
 */
int BSP_AdcImmersion_Init(void);

/**
 * @brief  采样一次（突发平均 + SMA + 滞回）
 * @param  sample  输出样本
 * @retval 0 成功，负值失败
 */
int BSP_AdcImmersion_Read(bsp_adc_imm_sample_t *sample);

/**
 * @brief  启动 CM4 FreeRTOS 采集任务（OpenAMP 上报）
 */
void BSP_AdcImmersion_Start(void);

#endif /* BSP_ADC_IMMERSION_H */
