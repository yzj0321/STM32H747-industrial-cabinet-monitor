/**
 * @file bsp_sys.c
 * @brief BSP 系统级辅助实现
 */
#include "bsp_sys.h"

/**
 * @brief  使能 I-Cache、D-Cache 并配置强制透写
 * @retval 无
 */
void bsp_cache_enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
    SCB->CACR |= 1U << 2;
}
