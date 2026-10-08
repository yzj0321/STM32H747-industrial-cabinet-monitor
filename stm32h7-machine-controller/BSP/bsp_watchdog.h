/**
 * @file bsp_watchdog.h
 * @brief 独立看门狗（IWDG）任务健康监控接口
 */
#ifndef BSP_WATCHDOG_H
#define BSP_WATCHDOG_H

#include <stdint.h>

typedef enum {
    BSP_WDG_LVGL = 0,
    BSP_WDG_DEFAULT,
    BSP_WDG_MODBUS,
    BSP_WDG_W5500,
    BSP_WDG_TASK_COUNT
} bsp_wdg_task_t;

/**
 * @brief  记录并打印 MCU 复位原因
 * @retval 无
 */
void BSP_Watchdog_LogResetCause(void);

/**
 * @brief  创建看门狗喂狗任务
 * @retval 无
 */
void BSP_Watchdog_Start(void);

/**
 * @brief  任务喂狗（递增心跳计数）
 * @param  task  任务槽位编号
 * @retval 无
 */
void BSP_Watchdog_Kick(bsp_wdg_task_t task);

/**
 * @brief  标记 W5500 等长时间阻塞网络操作进行中
 * @param  active  非 0 表示长操作进行中，仅监控 W5500 槽位
 * @retval 无
 */
void BSP_Watchdog_SetLongOp(uint8_t active);

#endif /* BSP_WATCHDOG_H */
