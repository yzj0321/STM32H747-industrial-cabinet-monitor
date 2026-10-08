/**
 * @file    bsp_ota_coop.h
 * @brief   OTA 期间与其他 RTOS 任务的协作挂起/恢复
 */
#ifndef BSP_OTA_COOP_H
#define BSP_OTA_COOP_H

#include "cmsis_os.h"

/**
 * @brief  注册需在 OTA 期间挂起的 RTOS 线程（OTA / w5500 / watchdog 勿注册）
 * @param  thread_id 线程句柄
 */
void bsp_ota_coop_register(osThreadId_t thread_id);

/**
 * @brief  BEGIN_SESSION 后挂起已注册线程，避免抢占 UART/CPU
 */
void bsp_ota_coop_suspend_peers(void);

/**
 * @brief  VERIFY 完成或 ABORT 后恢复已挂起线程
 */
void bsp_ota_coop_resume_peers(void);

#endif /* BSP_OTA_COOP_H */
