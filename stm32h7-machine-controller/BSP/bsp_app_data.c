/**
 * @file bsp_app_data.c
 * @brief 应用层共享传感器数据（线程安全读写）
 */
#include "bsp_app_data.h"

#include "cmsis_os.h"

typedef struct {
    uint16_t humidity;
    uint16_t temperature;
    uint16_t adc_mv;
    uint8_t wet;
} bsp_app_live_t;

static bsp_app_live_t s_live;
static osMutexId_t s_app_data_mutex;
static uint8_t s_rtos_ready;

/**
 * @brief  获取应用数据互斥锁
 * @retval 无
 */
static void bsp_app_data_lock(void)
{
    if (s_rtos_ready != 0U) {
        (void)osMutexAcquire(s_app_data_mutex, osWaitForever);
    }
}

/**
 * @brief  释放应用数据互斥锁
 * @retval 无
 */
static void bsp_app_data_unlock(void)
{
    if (s_rtos_ready != 0U) {
        (void)osMutexRelease(s_app_data_mutex);
    }
}

/**
 * @brief  初始化应用数据互斥锁（RTOS 就绪后调用）
 * @retval 无
 */
void BSP_AppData_RtosInit(void)
{
    if (s_rtos_ready != 0U) {
        return;
    }

    s_app_data_mutex = osMutexNew(NULL);
    s_rtos_ready = 1U;
}

/**
 * @brief  读取当前湿度值
 * @retval 湿度
 */
uint16_t BSP_AppData_GetHumidity(void)
{
    uint16_t value;

    bsp_app_data_lock();
    value = s_live.humidity;
    bsp_app_data_unlock();

    return value;
}

/**
 * @brief  读取当前温度值
 * @retval 温度
 */
uint16_t BSP_AppData_GetTemperature(void)
{
    uint16_t value;

    bsp_app_data_lock();
    value = s_live.temperature;
    bsp_app_data_unlock();

    return value;
}

/**
 * @brief  读取当前 ADC 电压
 * @retval 电压毫伏值（mV）
 */
uint16_t BSP_AppData_GetAdcMv(void)
{
    uint16_t value;

    bsp_app_data_lock();
    value = s_live.adc_mv;
    bsp_app_data_unlock();

    return value;
}

uint8_t BSP_AppData_GetWet(void)
{
    uint8_t value;

    bsp_app_data_lock();
    value = s_live.wet;
    bsp_app_data_unlock();

    return value;
}

/**
 * @brief  更新湿度值
 * @param  value  湿度
 * @retval 无
 */
void BSP_AppData_SetHumidity(uint16_t value)
{
    bsp_app_data_lock();
    s_live.humidity = value;
    bsp_app_data_unlock();
}

/**
 * @brief  更新温度值
 * @param  value  温度
 * @retval 无
 */
void BSP_AppData_SetTemperature(uint16_t value)
{
    bsp_app_data_lock();
    s_live.temperature = value;
    bsp_app_data_unlock();
}

/**
 * @brief  更新 ADC 电压值
 * @param  value  电压（mV）
 * @retval 无
 */
void BSP_AppData_SetAdcMv(uint16_t value)
{
    bsp_app_data_lock();
    s_live.adc_mv = value;
    bsp_app_data_unlock();
}

void BSP_AppData_SetWet(uint8_t value)
{
    bsp_app_data_lock();
    s_live.wet = value ? 1U : 0U;
    bsp_app_data_unlock();
}
