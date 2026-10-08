/**
 * @file    app_ipc_cm4.h
 * @brief   CM4 OpenAMP Remote：向 CM7 发送温湿度 / 水浸 ADC 数据
 */
#ifndef APP_IPC_CM4_H
#define APP_IPC_CM4_H

#include <stdint.h>

/**
 * @brief  初始化 CM4 侧 OpenAMP Remote
 * @retval 0 成功，非 0 失败
 */
int app_ipc_cm4_init(void);

/**
 * @brief  轮询处理 RPMsg（含 PING/ACK）
 */
void app_ipc_cm4_poll(void);

/**
 * @brief  向 CM7 发送温湿度传感器数据
 * @param  temperature  温度（约定单位，通常为 ℃×10）
 * @param  humidity     湿度（约定单位，通常为 %RH×10）
 * @retval 0 成功，非 0 失败
 */
int app_ipc_cm4_send_sensor(uint16_t temperature, uint16_t humidity);

/**
 * @brief  向 CM7 发送临时 Modbus 诊断状态
 * @param  status 状态码，见 APP_IPC_SENSOR_STATUS_*
 * @param  detail 底层返回码/errno
 * @param  raw_temperature 原始温度寄存器
 * @param  raw_humidity 原始湿度寄存器
 */
int app_ipc_cm4_send_sensor_status(uint32_t status,
                                   uint32_t detail,
                                   uint16_t raw_temperature,
                                   uint16_t raw_humidity);

/**
 * @brief  向 CM7 发送水浸 ADC 数据
 * @param  adc_mv  电压（mV）
 * @param  wet     浸水标志（非 0 表示浸水）
 * @retval 0 成功，非 0 失败
 */
int app_ipc_cm4_send_adc(uint16_t adc_mv, uint8_t wet);

#endif /* APP_IPC_CM4_H */
