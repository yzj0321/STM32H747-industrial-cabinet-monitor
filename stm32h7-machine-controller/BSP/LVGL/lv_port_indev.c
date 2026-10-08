/**
 * @file    lv_port_indev.c
 * @brief   LVGL 指针输入移植层（GT9xxx / FT5206 轮询触摸）
 */
#include "lv_port_indev.h"
#include "lvgl.h"
#include "touch.h"
#include <stdbool.h>
#include <stdint.h>

static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data);

/**
 * @brief  检测触摸是否按下
 * @retval true 按下，false 未按下或未就绪
 */
static bool touchpad_is_pressed(void);

/**
 * @brief  获取当前触摸坐标
 * @param  x 输出 X 坐标
 * @param  y 输出 Y 坐标
 */
static void touchpad_get_xy(int32_t *x, int32_t *y);

static bool touch_ready;
static bool touch_init_attempted;

/**
 * @brief  注册 LVGL 指针类输入设备（不立即初始化触摸硬件）
 */
void lv_port_indev_init(void)
{
    touch_ready = false;
    touch_init_attempted = false;

    lv_indev_t *indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
}

/**
 * @brief  尝试初始化触摸控制器（GT9xxx / FT5206）
 * @retval true  触摸就绪；false 初始化失败
 */
bool lv_port_indev_touch_init(void)
{
    if (touch_init_attempted)
    {
        return touch_ready;
    }

    touch_init_attempted = true;
    touch_ready = (tp_dev.init() == 0U);
    return touch_ready;
}

/**
 * @brief  查询触摸是否已成功初始化
 * @retval true  就绪；false 未就绪
 */
bool lv_port_indev_touch_ready(void)
{
    return touch_ready;
}

/**
 * @brief  LVGL 输入设备读回调
 * @param  indev  输入设备指针
 * @param  data   输出触摸状态与坐标
 */
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;

    (void)indev;

    if (touchpad_is_pressed())
    {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = last_x;
    data->point.y = last_y;
}

/**
 * @brief  检测触摸是否按下
 * @retval true  按下；false 未按下或未就绪
 */
static bool touchpad_is_pressed(void)
{
    if (!touch_ready || (tp_dev.scan == 0))
    {
        return false;
    }

    tp_dev.scan(0);

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        return true;
    }

    return false;
}

/**
 * @brief  获取当前触摸坐标
 * @param  x  输出 X 坐标
 * @param  y  输出 Y 坐标
 */
static void touchpad_get_xy(int32_t *x, int32_t *y)
{
    *x = (int32_t)tp_dev.x[0];
    *y = (int32_t)tp_dev.y[0];
}
