/**
 * @file    lv_port_indev.h
 * @brief   LVGL 输入设备移植层（电容触摸）
 */
#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  注册 LVGL 指针类输入设备（不立即初始化触摸硬件）
 */
void lv_port_indev_init(void);

/**
 * @brief  尝试初始化触摸控制器（GT9xxx / FT5206）
 * @retval true  触摸就绪；false 初始化失败
 */
bool lv_port_indev_touch_init(void);

/**
 * @brief  查询触摸是否已成功初始化
 * @retval true  就绪；false 未就绪
 */
bool lv_port_indev_touch_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_INDEV_H */
