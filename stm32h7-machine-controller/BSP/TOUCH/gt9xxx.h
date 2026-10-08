/**
 * @file    gt9xxx.h
 * @brief   GT9xxx 系列电容触摸控制器驱动（软件 I2C）
 */
#ifndef __GT9XXX_H
#define __GT9XXX_H

#include "main.h"
#include <stdint.h>

/* RST / INT pins for GT9xxx series.
 * Note: these are GPIO-only pins, I2C pins are handled by ctiic.c.
 */
#define GT9XXX_RST_GPIO_PORT            GPIOB
#define GT9XXX_RST_GPIO_PIN             GPIO_PIN_12
#define GT9XXX_RST_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define GT9XXX_INT_GPIO_PORT            GPIOB
#define GT9XXX_INT_GPIO_PIN             GPIO_PIN_5
#define GT9XXX_INT_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define GT9XXX_RST(x) do{ \
    if ((x) != 0) HAL_GPIO_WritePin(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, GPIO_PIN_SET); \
    else           HAL_GPIO_WritePin(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, GPIO_PIN_RESET); \
}while(0)

#define GT9XXX_INT()  HAL_GPIO_ReadPin(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN)

/* I2C command bytes for GT9xxx */
#define GT9XXX_CMD_WR       0x28
#define GT9XXX_CMD_RD       0x29

/* GT9xxx registers (16-bit address) */
#define GT9XXX_CTRL_REG     0x8040
#define GT9XXX_CFGS_REG     0x8047
#define GT9XXX_CHECK_REG    0x80FF
#define GT9XXX_PID_REG      0x8140
#define GT9XXX_GSTID_REG    0x814E

#define GT9XXX_TP1_REG      0x8150
#define GT9XXX_TP2_REG      0x8158
#define GT9XXX_TP3_REG      0x8160
#define GT9XXX_TP4_REG      0x8168
#define GT9XXX_TP5_REG      0x8170
#define GT9XXX_TP6_REG      0x8178
#define GT9XXX_TP7_REG      0x8180
#define GT9XXX_TP8_REG      0x8188
#define GT9XXX_TP9_REG      0x8190
#define GT9XXX_TP10_REG     0x8198

/**
 * @brief  初始化 GT9xxx 触摸芯片
 * @retval 0 成功；1 PID 不匹配
 */
uint8_t gt9xxx_init(void);

/**
 * @brief  轮询扫描触摸点并更新 tp_dev
 * @param  mode  扫描模式（驱动内部读写状态寄存器）
 * @retval 非 0 检测到触摸；0 无触摸
 */
uint8_t gt9xxx_scan(uint8_t mode);

/**
 * @brief  向 GT9xxx 寄存器写入数据
 * @param  reg  16 位寄存器地址
 * @param  buf  写入数据缓冲区
 * @param  len  写入长度
 * @retval 0 成功；非 0 I2C 错误
 */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len);

/**
 * @brief  从 GT9xxx 寄存器读取数据
 * @param  reg  16 位寄存器地址
 * @param  buf  输出缓冲区
 * @param  len  读取长度
 */
void gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len);

#endif /* __GT9XXX_H */
