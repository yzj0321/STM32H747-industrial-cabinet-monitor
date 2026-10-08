/**
 * @file    mpu.h
 * @brief   Cortex-M7 MPU 内存保护配置接口
 */
#ifndef BSP_MPU_H
#define BSP_MPU_H

#include "main.h"

/**
 * @brief  配置单个 MPU 保护区域
 * @param  baseaddr  区域基地址
 * @param  size      区域大小（MPU_REGION_SIZE_x 枚举）
 * @param  rnum      区域编号
 * @param  de        是否禁止指令执行
 * @param  ap        访问权限
 * @param  sen       是否共享
 * @param  cen       是否可缓存
 * @param  ben       是否可缓冲
 * @retval 0 成功
 */
uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum,
                           uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben);

/**
 * @brief  按工程内存布局配置全部 MPU 区域
 */
void mpu_memory_protection(void);

#endif /* BSP_MPU_H */
