/**
 * @file    touch.h
 * @brief   统一触摸屏接口（GT9xxx / FT5206 自动探测）
 */
#ifndef __TOUCH_H
#define __TOUCH_H

#include "main.h"
#include "lcd.h"

#define CT_MAX_TOUCH  10

/* Touch status bits (compatible with your example code style) */
#define TP_PRES_DOWN    0x8000U   /* pressed */
#define TP_CATH_PRES    0x4000U   /* key/valid point pressed flag (capacitive) */

/** @brief 触摸屏设备状态与操作函数表 */
typedef struct
{
    uint8_t (*init)(void);
    uint8_t (*scan)(uint8_t mode);
    void (*adjust)(void);

    uint16_t x[CT_MAX_TOUCH];
    uint16_t y[CT_MAX_TOUCH];
    uint16_t sta;

    /* bit0: 0=portrait mapping, 1=landscape mapping (your example uses lcddev.dir&0x01) */
    /* bit7: 1=capacitive mode */
    uint8_t touchtype;
} _m_tp_dev;

extern _m_tp_dev tp_dev;

/**
 * @brief  初始化触摸屏（自动探测 GT9xxx 或 FT5206）
 * @retval 0 成功；1 未检测到支持的触摸芯片
 */
uint8_t tp_init(void);

/**
 * @brief  扫描触摸点（委托给已绑定的 scan 函数）
 * @param  mode  扫描模式（由具体驱动解释）
 * @retval 非 0 检测到有效触摸；0 无触摸
 */
uint8_t tp_scan(uint8_t mode);

#endif /* __TOUCH_H */
