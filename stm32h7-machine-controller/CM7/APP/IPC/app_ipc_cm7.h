/**
 * @file    app_ipc_cm7.h
 * @brief   CM7 OpenAMP Master：接收 CM4 温湿度数据
 */
#ifndef APP_IPC_CM7_H
#define APP_IPC_CM7_H

#include "app_ipc_proto.h"

/**
 * @brief  清空 D3 SRAM 中的 OpenAMP 共享区（启动前调用）
 */
void app_ipc_d3_clear(void);

/**
 * @brief  初始化 CM7 侧 OpenAMP Master 与 RPMsg 通道
 * @retval 0 成功，非 0 失败
 */
int app_ipc_cm7_init_master(void);

/**
 * @brief  等待 CM4 Remote 端点就绪
 * @retval 0 成功，非 0 超时/失败
 */
int app_ipc_cm7_wait_endpoint(void);

/**
 * @brief  轮询接收 CM4 上报的传感器 / ADC 帧并写入 AppData
 */
void app_ipc_cm7_poll(void);

#endif /* APP_IPC_CM7_H */
