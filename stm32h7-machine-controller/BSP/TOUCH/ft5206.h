/**
 * @file    ft5206.h
 * @brief   FT5206/FT5426 电容触摸控制器驱动
 */
#ifndef __FT5206_H
#define __FT5206_H

#include "main.h"
#include <stdint.h>

#define FT5206_RST_GPIO_PORT            GPIOB
#define FT5206_RST_GPIO_PIN             GPIO_PIN_12
#define FT5206_RST_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define FT5206_INT_GPIO_PORT            GPIOB
#define FT5206_INT_GPIO_PIN             GPIO_PIN_5
#define FT5206_INT_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define FT5206_RST(x) do{ \
    if ((x) != 0) HAL_GPIO_WritePin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN, GPIO_PIN_SET); \
    else           HAL_GPIO_WritePin(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN, GPIO_PIN_RESET); \
}while(0)

#define FT5206_INT()  HAL_GPIO_ReadPin(FT5206_INT_GPIO_PORT, FT5206_INT_GPIO_PIN)

/* I2C command bytes */
#define FT5206_CMD_WR       0x70
#define FT5206_CMD_RD       0x71

/* FT5206 registers (16-bit address in this software I2C protocol) */
#define FT5206_DEVIDE_MODE          0x00
#define FT5206_REG_NUM_FINGER       0x02

#define FT5206_TP1_REG              0x03
#define FT5206_TP2_REG              0x09
#define FT5206_TP3_REG              0x0F
#define FT5206_TP4_REG              0x15
#define FT5206_TP5_REG              0x1B

#define FT5206_ID_G_LIB_VERSION     0xA1
#define FT5206_ID_G_MODE            0xA4
#define FT5206_ID_G_THGROUP         0x80
#define FT5206_ID_G_PERIODACTIVE    0x88

/**
 * @brief  初始化 FT5206 触摸芯片
 * @retval 0 成功；1 芯片 ID/版本校验失败
 */
uint8_t ft5206_init(void);

/**
 * @brief  轮询扫描触摸点并更新 tp_dev
 * @param  mode  扫描模式（本驱动内部使用）
 * @retval 非 0 检测到触摸；0 无触摸
 */
uint8_t ft5206_scan(uint8_t mode);

/**
 * @brief  向 FT5206 寄存器写入数据
 * @param  reg  寄存器地址
 * @param  buf  写入数据缓冲区
 * @param  len  写入长度
 * @retval 0 成功；非 0 I2C 错误
 */
uint8_t ft5206_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len);

/**
 * @brief  从 FT5206 寄存器读取数据
 * @param  reg  寄存器地址
 * @param  buf  输出缓冲区
 * @param  len  读取长度
 */
void ft5206_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len);

#endif /* __FT5206_H */
