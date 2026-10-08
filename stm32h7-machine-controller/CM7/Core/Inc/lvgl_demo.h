/**
 * @file    lvgl_demo.h
 * @brief   LVGL CabinetGuard 四页 HMI 应用入口
 */
#ifndef LVGL_DEMO_H
#define LVGL_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化 LVGL 内核与显示/输入移植层
 */
void lvgl_app_init(void);

/**
 * @brief  创建并启动 LVGL 主任务
 */
void lvgl_start_task(void);

#ifdef __cplusplus
}
#endif

#endif /* LVGL_DEMO_H */
