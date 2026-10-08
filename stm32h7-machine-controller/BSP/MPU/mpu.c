/**
 * @file    mpu.c
 * @brief   Cortex-M7 MPU 内存保护区域配置
 */
#include "mpu.h"

static uint8_t mpu_set_protection_ex(uint32_t baseaddr, uint32_t size, uint32_t rnum,
                                     uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen,
                                     uint8_t ben, uint32_t tex)
{
    MPU_Region_InitTypeDef mpu_region_init_handle;

    HAL_MPU_Disable();

    mpu_region_init_handle.Enable = MPU_REGION_ENABLE;
    mpu_region_init_handle.Number = rnum;
    mpu_region_init_handle.BaseAddress = baseaddr;
    mpu_region_init_handle.Size = size;
    mpu_region_init_handle.SubRegionDisable = 0x00U;
    mpu_region_init_handle.TypeExtField = tex;
    mpu_region_init_handle.AccessPermission = ap;
    mpu_region_init_handle.DisableExec = de;
    mpu_region_init_handle.IsShareable = sen;
    mpu_region_init_handle.IsCacheable = cen;
    mpu_region_init_handle.IsBufferable = ben;

    HAL_MPU_ConfigRegion(&mpu_region_init_handle);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

    return 0;
}

uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum,
                           uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben)
{
    return mpu_set_protection_ex(baseaddr, size, rnum, de, ap, sen, cen, ben,
                                 MPU_TEX_LEVEL0);
}

/**
 * @brief  配置 MPU 内存保护区域
 */
void mpu_memory_protection(void)
{
    mpu_set_protection_ex(0x20000000U, MPU_REGION_SIZE_128KB, MPU_REGION_NUMBER1, 0,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                          MPU_ACCESS_CACHEABLE, MPU_ACCESS_BUFFERABLE, MPU_TEX_LEVEL0);

    mpu_set_protection_ex(0x24000000U, MPU_REGION_SIZE_512KB, MPU_REGION_NUMBER2, 0,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                          MPU_ACCESS_CACHEABLE, MPU_ACCESS_BUFFERABLE, MPU_TEX_LEVEL0);

    mpu_set_protection_ex(0x30000000U, MPU_REGION_SIZE_512KB, MPU_REGION_NUMBER3, 0,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                          MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                          MPU_TEX_LEVEL1);

    mpu_set_protection_ex(0x38000000U, MPU_REGION_SIZE_64KB, MPU_REGION_NUMBER4, 1,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_SHAREABLE,
                          MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                          MPU_TEX_LEVEL1);

    mpu_set_protection_ex(0x60000000U, MPU_REGION_SIZE_64MB, MPU_REGION_NUMBER5, 0,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                          MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                          MPU_TEX_LEVEL0);

    /* LTDC 帧缓冲在 SDRAM：必须非 Cache，否则 DMA2D 写入后被 CPU 缓存回写污染花屏 */
    mpu_set_protection_ex(0xD0000000U, MPU_REGION_SIZE_32MB, MPU_REGION_NUMBER6, 0,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                          MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                          MPU_TEX_LEVEL1);

    mpu_set_protection_ex(0x80000000U, MPU_REGION_SIZE_256MB, MPU_REGION_NUMBER7, 1,
                          MPU_REGION_FULL_ACCESS, MPU_ACCESS_NOT_SHAREABLE,
                          MPU_ACCESS_NOT_CACHEABLE, MPU_ACCESS_NOT_BUFFERABLE,
                          MPU_TEX_LEVEL0);
}
