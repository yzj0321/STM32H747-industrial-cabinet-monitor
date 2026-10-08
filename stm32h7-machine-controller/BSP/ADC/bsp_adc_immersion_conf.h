/**
 * @file    bsp_adc_immersion_conf.h
 * @brief   水浸传感器（AO→ADC1_INP4 / PC4）采样与算法参数
 */
#ifndef BSP_ADC_IMMERSION_CONF_H
#define BSP_ADC_IMMERSION_CONF_H

/** ADC 参考电压（mV），与板级 3.3V 一致 */
#define BSP_ADC_IMM_VREF_MV           (3300U)

/** 16-bit 满量程码值 */
#define BSP_ADC_IMM_FULL_SCALE        (65535U)

/** 单次上报前连续采样次数（突发平均） */
#define BSP_ADC_IMM_BURST_SAMPLES     (8U)

/** 简单滑动平均窗口长度 */
#define BSP_ADC_IMM_SMA_WINDOW        (8U)

/** 任务轮询周期（ms） */
#define BSP_ADC_IMM_POLL_MS           (200U)

/** OpenAMP 上报最短间隔（ms） */
#define BSP_ADC_IMM_REPORT_MS         (500U)

/**
 * 极性：1 = 电压升高判定为浸水（常见导电探头 AO）；
 *       0 = 电压降低判定为浸水
 */
#define BSP_ADC_IMM_WET_WHEN_HIGH     (0U)

/**
 * 滞回阈值（mV）。
 * WET_WHEN_HIGH=1：mv >= ENTER → 浸水；mv <= EXIT → 干燥
 * WET_WHEN_HIGH=0：mv <= ENTER → 浸水；mv >= EXIT → 干燥
 */
#define BSP_ADC_IMM_WET_ENTER_MV      (1200U)
#define BSP_ADC_IMM_WET_EXIT_MV       (1600U)

/** 单次转换超时（ms） */
#define BSP_ADC_IMM_CONV_TIMEOUT_MS   (10U)

#endif /* BSP_ADC_IMMERSION_CONF_H */
