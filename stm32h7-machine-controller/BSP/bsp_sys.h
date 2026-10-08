/**
 * @file bsp_sys.h
 * @brief BSP 系统级辅助接口
 */
#ifndef BSP_SYS_H
#define BSP_SYS_H

#include "main.h"

/**
 * @brief  使能 I-Cache、D-Cache 并配置强制透写
 * @retval 无
 */
void bsp_cache_enable(void);

#endif /* BSP_SYS_H */
