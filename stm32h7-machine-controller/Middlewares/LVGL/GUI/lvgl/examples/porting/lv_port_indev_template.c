/**
 * @file lv_port_indev_templ.c
 *
 */

/*复制此文件为"lv_port_indev.c"并将此值设为"1"以启用内容*/
#if 1

/*********************
 *      包含文件
 *********************/
#include "lv_port_indev_template.h"
#include "lvgl.h"
#include "./BSP/TOUCH/touch.h"
#include <stdint.h>
#include <stdbool.h>

/*********************
 *      宏定义
 *********************/

/**********************
 *      类型定义
 **********************/

/**********************
 *  静态函数原型
 **********************/

/* 触摸屏 */
static void touchpad_init(void);
static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(int32_t * x, int32_t * y);

/* 鼠标 */
//static void mouse_init(void);
//static void mouse_read(lv_indev_t * indev, lv_indev_data_t * data);
//static bool mouse_is_pressed(void);
//static void mouse_get_xy(int32_t * x, int32_t * y);

/* 键盘 */
//static void keypad_init(void);
//static void keypad_read(lv_indev_t * indev, lv_indev_data_t * data);
//static uint32_t keypad_get_key(void);

/* 编码器 */
//static void encoder_init(void);
//static void encoder_read(lv_indev_t * indev, lv_indev_data_t * data);
//static void encoder_handler(void);

/* 按钮 */
//static void button_init(void);
//static void button_read(lv_indev_t * indev, lv_indev_data_t * data);
//static int8_t button_get_pressed_id(void);
//static bool button_is_pressed(uint8_t id);

/**********************
 *  静态变量
 **********************/
lv_indev_t * indev_touchpad;
//lv_indev_t * indev_mouse;
//lv_indev_t * indev_keypad;
//lv_indev_t * indev_encoder;
//lv_indev_t * indev_button;

static int32_t encoder_diff;
static lv_indev_state_t encoder_state;

/**********************
 *      宏定义
 **********************/

/**********************
 *  全局函数
 **********************/

/**
 * @brief       初始化并注册输入设备
 * @param       无
 * @retval      无
 */
void lv_port_indev_init(void)
{
    /**
     * 这里提供了LittlevGL支持的输入设备示例实现：
     *  - 触摸屏
     *  - 鼠标（支持光标）
     *  - 键盘（仅支持按键操作的GUI使用）
     *  - 编码器（仅支持：左旋、右旋、按压操作）
     *  - 按钮（外部按钮，用于按压屏幕上的点）
     *
     *  `..._read()` 函数仅为示例。
     *  您应根据硬件情况调整它们
     */

    /*------------------
     * 触摸屏
     * -----------------*/

    /*初始化触摸屏（如果有）*/
    touchpad_init();

    /*注册触摸屏输入设备*/
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);

//    /*------------------
//     * 鼠标
//     * -----------------*/

//    /*初始化鼠标（如果有）*/
//    mouse_init();

//    /*注册鼠标输入设备*/
//    indev_mouse = lv_indev_create();
//    lv_indev_set_type(indev_mouse, LV_INDEV_TYPE_POINTER);
//    lv_indev_set_read_cb(indev_mouse, mouse_read);

//    /*设置光标。为简单起见，现在设置一个HOME符号*/
//    lv_obj_t * mouse_cursor = lv_image_create(lv_screen_active());
//    lv_image_set_src(mouse_cursor, LV_SYMBOL_HOME);
//    lv_indev_set_cursor(indev_mouse, mouse_cursor);

//    /*------------------
//     * 键盘
//     * -----------------*/

//    /*初始化键盘或按键（如果有）*/
//    keypad_init();

//    /*注册键盘输入设备*/
//    indev_keypad = lv_indev_create();
//    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
//    lv_indev_set_read_cb(indev_keypad, keypad_read);

//    /*之后您应该创建分组：`lv_group_t * group = lv_group_create()`，
//     * 将对象添加到分组：`lv_group_add_obj(group, obj)`
//     * 并将此输入设备分配给分组以实现导航：
//     * `lv_indev_set_group(indev_keypad, group);`*/

//    /*------------------
//     * 编码器
//     * -----------------*/

//    /*初始化编码器（如果有）*/
//    encoder_init();

//    /*注册编码器输入设备*/
//    indev_encoder = lv_indev_create();
//    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
//    lv_indev_set_read_cb(indev_encoder, encoder_read);

//    /*之后您应该创建分组：`lv_group_t * group = lv_group_create()`，
//     * 将对象添加到分组：`lv_group_add_obj(group, obj)`
//     * 并将此输入设备分配给分组以实现导航：
//     * `lv_indev_set_group(indev_encoder, group);`*/

//    /*------------------
//     * 按钮
//     * -----------------*/

//    /*初始化按钮（如果有）*/
//    button_init();

//    /*注册按钮输入设备*/
//    indev_button = lv_indev_create();
//    lv_indev_set_type(indev_button, LV_INDEV_TYPE_BUTTON);
//    lv_indev_set_read_cb(indev_button, button_read);

//    /*将按钮分配到屏幕上的点*/
//    static const lv_point_t btn_points[2] = {
//        {10, 10},   /*按钮 0 -> x:10; y:10*/
//        {40, 100},  /*按钮 1 -> x:40; y:100*/
//    };
//    lv_indev_set_button_points(indev_button, btn_points);
}

/**********************
 *  静态函数
 **********************/

/*------------------
 * 触摸屏
 * -----------------*/

/**
 * @brief       初始化触摸屏
 * @param       无
 * @retval      无
 */
static void touchpad_init(void)
{
    /*在此添加您的代码*/
    tp_dev.init();
}

/**
 * @brief       图形库的触摸屏读取回调函数
 * @param       indev_drv   : 触摸屏设备
 *   @arg       data        : 输入设备数据结构体
 * @retval      无
 */
static void touchpad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;

	  /* 保存按下的坐标和状态 */
    if(touchpad_is_pressed()) 
		{
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PRESSED;
    } 
		else 
		{
        data->state = LV_INDEV_STATE_RELEASED;
    }

		/* 设置最后按下的坐标 */
    data->point.x = last_x;
    data->point.y = last_y;
}

/**
 * @brief       获取触摸屏设备的状态
 * @param       无
 * @retval      返回触摸屏设备是否被按下
 */
static bool touchpad_is_pressed(void)
{
    /*在此添加您的代码*/
	  tp_dev.scan(0);

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        return true;
    }
		
    return false;
}

/**
 * @brief       在触摸屏被按下的时候读取 x、y 坐标
 * @param       x   : x坐标的指针
 *   @arg       y   : y坐标的指针
 * @retval      无
 */
static void touchpad_get_xy(int32_t * x, int32_t * y)
{
    /*在此添加您的代码*/
    (*x) = tp_dev.x[0];
    (*y) = tp_dev.y[0];
}

///*------------------
// * 鼠标
// * -----------------*/

/**
 * @brief       初始化鼠标
 * @param       无
 * @retval      无
 */
//static void mouse_init(void)
//{
//    /*在此添加您的代码*/
//}

/**
 * @brief       图形库的鼠标读取回调函数
 * @param       indev_drv   : 鼠标设备
 *   @arg       data        : 输入设备数据结构体
 * @retval      无
 */
//static void mouse_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
//{
//    /*获取当前x和y坐标*/
//    mouse_get_xy(&data->point.x, &data->point.y);

//    /*获取鼠标按键状态（按下或释放）*/
//    if(mouse_is_pressed()) {
//        data->state = LV_INDEV_STATE_PRESSED;
//    }
//    else {
//        data->state = LV_INDEV_STATE_RELEASED;
//    }
//}

/**
 * @brief       获取鼠标设备是否被按下
 * @param       无
 * @retval      返回鼠标设备是否被按下
 */
//static bool mouse_is_pressed(void)
//{
//    /*在此添加您的代码*/
//    return false;
//}

/**
 * @brief       当鼠标被按下时，获取鼠标的 x、y 坐标
 * @param       x   : x坐标的指针
 *   @arg       y   : y坐标的指针
 * @retval      无
 */
//static void mouse_get_xy(int32_t * x, int32_t * y)
//{
//    /*在此添加您的代码*/
//    (*x) = 0;
//    (*y) = 0;
//}

///*------------------
// * 键盘
// * -----------------*/

///**
// * @brief       初始化键盘
// * @param       无
// * @retval      无
// */
//static void keypad_init(void)
//{
//    /*在此添加您的代码*/
//}

///**
// * @brief       图形库的键盘读取回调函数
// * @param       indev_drv : 键盘设备
// *   @arg       data      : 输入设备数据结构体
// * @retval      无
// */
//static void keypad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
//{
//    static uint32_t last_key = 0;

//    /*获取当前x和y坐标*/
//    mouse_get_xy(&data->point.x, &data->point.y);

//    /*获取按键状态并保存被按下的键*/
//    uint32_t act_key = keypad_get_key();
//    if(act_key != 0) {
//        data->state = LV_INDEV_STATE_PRESSED;

//        /*根据您的按键定义将按键转换为LVGL控制字符*/
//        switch(act_key) {
//            case 1:
//                act_key = LV_KEY_NEXT;
//                break;
//            case 2:
//                act_key = LV_KEY_PREV;
//                break;
//            case 3:
//                act_key = LV_KEY_LEFT;
//                break;
//            case 4:
//                act_key = LV_KEY_RIGHT;
//                break;
//            case 5:
//                act_key = LV_KEY_ENTER;
//                break;
//        }

//        last_key = act_key;
//    }
//    else {
//        data->state = LV_INDEV_STATE_RELEASED;
//    }

//    data->key = last_key;
//}

///**
// * @brief       获取当前正在按下的按键
// * @param       无
// * @retval      0 : 按键没有被按下
// */
//static uint32_t keypad_get_key(void)
//{
//    /*在此添加您的代码*/
//    return 0;
//}

///*------------------
// * 编码器
// * -----------------*/

///**
// * @brief       初始化编码器
// * @param       无
// * @retval      无
// */
//static void encoder_init(void)
//{
//    /*在此添加您的代码*/
//}

///**
// * @brief       图形库的编码器读取回调函数
// * @param       indev_drv : 编码器设备
// *   @arg       data      : 输入设备数据结构体
// * @retval      无
// */
//static void encoder_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
//{
//    data->enc_diff = encoder_diff;
//    data->state = encoder_state;
//}

///**
// * @brief       在中断中调用此函数以处理编码器事件(旋转、按下)
// * @param       无
// * @retval      无
// */
//static void encoder_handler(void)
//{
//    /*在此添加您的代码*/
//    encoder_diff += 0;
//    encoder_state = LV_INDEV_STATE_RELEASED;
//}

///*------------------
// * 按钮
// * -----------------*/

///**
// * @brief       初始化按钮
// * @param       无
// * @retval      无
// */
//static void button_init(void)
//{
//    /*在此添加您的代码*/
//}

///**
// * @brief       图形库的按钮读取回调函数
// * @param       indev_drv : 按钮设备
// *   @arg       data      : 输入设备数据结构体
// * @retval      无
// */
//static void button_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
//{
//    static uint8_t last_btn = 0;

//    /*获取被按下的按钮ID*/
//    int8_t btn_act = button_get_pressed_id();

//    if(btn_act >= 0) {
//        data->state = LV_INDEV_STATE_PRESSED;
//        last_btn = btn_act;
//    }
//    else {
//        data->state = LV_INDEV_STATE_RELEASED;
//    }

//    /*保存最后按下的按钮ID*/
//    data->btn_id = last_btn;
//}

///**
// * @brief       获取被按下按钮的ID
// * @param       无
// * @retval      被按下按钮的ID
// */
//static int8_t button_get_pressed_id(void)
//{
//    uint8_t i;

//    /*检查按钮状态（假设有2个按钮）*/
//    for(i = 0; i < 2; i++) {
//        /*返回被按下的按钮ID*/
//        if(button_is_pressed(i)) {
//            return i;
//        }
//    }

//    /*无按钮按下*/
//    return -1;
//}

///**
// * @brief       检查指定ID的按钮是否被按下
// * @param       无
// * @retval      按钮是否被按下
// */
//static bool button_is_pressed(uint8_t id)
//{
//    /*在此添加您的代码*/
//    return false;
//}

#else /*在文件顶部启用此功能*/

/*此虚拟类型定义仅用于消除-pedantic警告*/
typedef int keep_pedantic_happy;
#endif
