/**
 * @file bsp_delay.h
 * @brief BSP 简单延时接口
 */
#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include "main.h"

/**
 * @brief  毫秒级阻塞延时
 * @param  ms  延时毫秒数
 * @retval 无
 */
void delay_ms(uint32_t ms);

/**
 * @brief  微秒级忙等待延时
 * @param  us  延时微秒数
 * @retval 无
 */
void delay_us(uint32_t us);

#endif /* BSP_DELAY_H */
