/**
 * @file bsp_delay.c
 * @brief BSP 简单延时实现
 */
#include "bsp_delay.h"

/**
 * @brief  毫秒级阻塞延时
 * @param  ms  延时毫秒数
 * @retval 无
 */
void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief  微秒级忙等待延时
 * @param  us  延时微秒数
 * @retval 无
 */
void delay_us(uint32_t us)
{
    uint32_t ticks = (SystemCoreClock / 1000000U) * us / 5U;

    while (ticks-- > 0U)
    {
        __NOP();
    }
}
