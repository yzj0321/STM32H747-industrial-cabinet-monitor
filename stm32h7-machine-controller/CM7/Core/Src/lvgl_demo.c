/**
 * @file    lvgl_demo.c
 * @brief   LVGL 应用入口：CabinetGuard 四页 HMI
 */
#include "lvgl_demo.h"

#include "cabinet_guard_ui.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "bsp_watchdog.h"
#include "bsp_ota_coop.h"

#define LVGL_TASK_PRIO   (osPriorityAboveNormal)
#define LVGL_TASK_STACK  (4096U * 4U)
#define UI_REFRESH_MS    500U

static osThreadId_t lvglTaskHandle;

static void lvgl_task(void *argument);
static void ui_refresh_timer(lv_timer_t *timer);

/**
 * @brief  初始化 LVGL 应用与端口
 */
void lvgl_app_init(void)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
}

/**
 * @brief  创建 LVGL 任务
 */
void lvgl_start_task(void)
{
    const osThreadAttr_t lvgl_task_attributes = {
        .name = "lvgl_task",
        .stack_size = LVGL_TASK_STACK,
        .priority = LVGL_TASK_PRIO,
    };

    lvglTaskHandle = osThreadNew(lvgl_task, NULL, &lvgl_task_attributes);
    bsp_ota_coop_register(lvglTaskHandle);
}

/**
 * @brief  LVGL 主循环任务
 */
static void lvgl_task(void *argument)
{
    (void)argument;

    BSP_Watchdog_Kick(BSP_WDG_LVGL);
    (void)lv_port_indev_touch_init();
    cabinet_guard_ui_create();
    lv_timer_create(ui_refresh_timer, UI_REFRESH_MS, NULL);

    for (;;) {
        BSP_Watchdog_Kick(BSP_WDG_LVGL);
        lv_timer_handler();
        osDelay(1);
    }
}

/**
 * @brief  UI 定时刷新回调
 */
static void ui_refresh_timer(lv_timer_t *timer)
{
    (void)timer;
    cabinet_guard_ui_refresh();
}

/**
 * @brief  FreeRTOS tick hook（驱动 LVGL 节拍）
 */
void vApplicationTickHook(void)
{
    lv_tick_inc(1);
}
