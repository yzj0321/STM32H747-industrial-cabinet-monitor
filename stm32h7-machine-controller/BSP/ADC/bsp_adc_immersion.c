/**
 * @file    bsp_adc_immersion.c
 * @brief   水浸传感器：ADC1 CH4 采样 + SMA 滤波 + 滞回判决 + OpenAMP 上报
 *
 * 算法说明：
 *  1) 突发平均 —— 连续采 N 次削弱瞬时噪声（线长约 1m）
 *  2) SMA —— 滑动平均平滑慢变抖动
 *  3) 滞回 —— 进入/退出阈值分离，避免临界区反复翻转
 */
#include "bsp_adc_immersion.h"
#include "bsp_adc_immersion_conf.h"

#include "adc.h"

#include "cmsis_os2.h"

#if defined(CORE_CM4)
#include "app_ipc_cm4.h"
#endif

#define ADC_IMM_TASK_STACK_WORDS  512U

static uint16_t s_sma_buf[BSP_ADC_IMM_SMA_WINDOW];
static uint32_t s_sma_sum;
static uint8_t s_sma_idx;
static uint8_t s_sma_count;
static uint8_t s_wet;
static uint8_t s_calibrated;

/**
 * @brief  ADC 原始值换算为毫伏
 */
static uint16_t adc_raw_to_mv(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * BSP_ADC_IMM_VREF_MV) / BSP_ADC_IMM_FULL_SCALE);
}

/**
 * @brief  单次读取 ADC 原始值
 */
static int adc_read_raw_once(uint16_t *raw)
{
    if (raw == NULL) {
        return -1;
    }

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return -1;
    }

    if (HAL_ADC_PollForConversion(&hadc1, BSP_ADC_IMM_CONV_TIMEOUT_MS) != HAL_OK) {
        (void)HAL_ADC_Stop(&hadc1);
        return -1;
    }

    *raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);
    return 0;
}

/**
 * @brief  突发采样并求平均
 */
static int adc_read_burst_avg(uint16_t *avg_raw)
{
    uint32_t sum = 0U;
    uint32_t i;

    if (avg_raw == NULL) {
        return -1;
    }

    for (i = 0U; i < BSP_ADC_IMM_BURST_SAMPLES; i++) {
        uint16_t raw;

        if (adc_read_raw_once(&raw) != 0) {
            return -1;
        }
        sum += raw;
    }

    *avg_raw = (uint16_t)(sum / BSP_ADC_IMM_BURST_SAMPLES);
    return 0;
}

/**
 * @brief  更新滑动平均滤波
 */
static uint16_t sma_update(uint16_t sample)
{
    if (s_sma_count < BSP_ADC_IMM_SMA_WINDOW) {
        s_sma_buf[s_sma_idx] = sample;
        s_sma_sum += sample;
        s_sma_idx = (uint8_t)((s_sma_idx + 1U) % BSP_ADC_IMM_SMA_WINDOW);
        s_sma_count++;
        return (uint16_t)(s_sma_sum / s_sma_count);
    }

    s_sma_sum -= s_sma_buf[s_sma_idx];
    s_sma_buf[s_sma_idx] = sample;
    s_sma_sum += sample;
    s_sma_idx = (uint8_t)((s_sma_idx + 1U) % BSP_ADC_IMM_SMA_WINDOW);
    return (uint16_t)(s_sma_sum / BSP_ADC_IMM_SMA_WINDOW);
}

/**
 * @brief  更新浸水滞回判断
 */
static uint8_t hysteresis_update(uint16_t mv)
{
#if (BSP_ADC_IMM_WET_WHEN_HIGH != 0U)
    if (s_wet == 0U) {
        if (mv >= BSP_ADC_IMM_WET_ENTER_MV) {
            s_wet = 1U;
        }
    } else {
        if (mv <= BSP_ADC_IMM_WET_EXIT_MV) {
            s_wet = 0U;
        }
    }
#else
    if (s_wet == 0U) {
        if (mv <= BSP_ADC_IMM_WET_ENTER_MV) {
            s_wet = 1U;
        }
    } else {
        if (mv >= BSP_ADC_IMM_WET_EXIT_MV) {
            s_wet = 0U;
        }
    }
#endif
    return s_wet;
}

/**
 * @brief  初始化水浸 ADC
 */
int BSP_AdcImmersion_Init(void)
{
    s_sma_sum = 0U;
    s_sma_idx = 0U;
    s_sma_count = 0U;
    s_wet = 0U;
    s_calibrated = 0U;

    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET, ADC_SINGLE_ENDED) != HAL_OK) {
        return -1;
    }

    s_calibrated = 1U;
    return 0;
}

/**
 * @brief  读取水浸 ADC 结果
 */
int BSP_AdcImmersion_Read(bsp_adc_imm_sample_t *sample)
{
    uint16_t burst;
    uint16_t filtered;

    if (sample == NULL) {
        return -1;
    }

    sample->valid = 0U;

    if (s_calibrated == 0U) {
        return -1;
    }

    if (adc_read_burst_avg(&burst) != 0) {
        return -1;
    }

    filtered = sma_update(burst);
    sample->raw = filtered;
    sample->mv = adc_raw_to_mv(filtered);
    sample->wet = hysteresis_update(sample->mv);
    sample->valid = 1U;
    return 0;
}

#if defined(CORE_CM4)
/**
 * @brief  水浸 ADC 周期采集任务
 */
static void AdcImmersionTask(void *argument)
{
    uint32_t last_report_ms = 0U;

    (void)argument;

    if (BSP_AdcImmersion_Init() != 0) {
        for (;;) {
            osDelay(1000);
        }
    }

    for (;;) {
        bsp_adc_imm_sample_t sample;
        uint32_t now = osKernelGetTickCount();

        if (BSP_AdcImmersion_Read(&sample) == 0) {
            if ((now - last_report_ms) >= BSP_ADC_IMM_REPORT_MS) {
                last_report_ms = now;
                (void)app_ipc_cm4_send_adc(sample.mv, sample.wet);
            }
        }

        osDelay(BSP_ADC_IMM_POLL_MS);
    }
}
#endif

/**
 * @brief  启动水浸 ADC 任务
 */
void BSP_AdcImmersion_Start(void)
{
#if defined(CORE_CM4)
    const osThreadAttr_t attr = {
        .name = "adc_imm",
        .stack_size = ADC_IMM_TASK_STACK_WORDS * sizeof(uint32_t),
        .priority = osPriorityBelowNormal,
    };

    (void)osThreadNew(AdcImmersionTask, NULL, &attr);
#else
    (void)0;
#endif
}
