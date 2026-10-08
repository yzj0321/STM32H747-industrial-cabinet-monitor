/**
 * @file    lv_port_disp.h
 * @brief   LVGL 显示移植层（LTDC RGB LCD）
 */
#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  初始化 LVGL 显示驱动与帧缓冲
 */
void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LV_PORT_DISP_H */
