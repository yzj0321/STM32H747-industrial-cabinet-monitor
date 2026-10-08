/**
 * @file    bsp_ota_flash.h
 * @brief   OTA 目标槽位 Flash 擦写与校验
 */
#ifndef BSP_OTA_FLASH_H
#define BSP_OTA_FLASH_H

#include <stdint.h>

typedef enum {
    BSP_OTA_FLASH_CORE_CM7 = 0,
    BSP_OTA_FLASH_CORE_CM4 = 1,
} bsp_ota_flash_core_t;

/**
 * @brief  设置 OTA 目标核（CM7 / CM4）
 * @param  core  目标核
 */
void bsp_ota_flash_set_target_core(bsp_ota_flash_core_t core);

/**
 * @brief  查询当前 OTA 目标核
 * @retval 目标核枚举
 */
bsp_ota_flash_core_t bsp_ota_flash_target_core(void);

/**
 * @brief  擦除指定 OTA 槽
 * @param  slot_index  槽索引
 * @retval 0 成功，非 0 失败
 */
int bsp_ota_flash_erase_slot(uint8_t slot_index);

/**
 * @brief  向当前目标槽写入镜像数据
 * @param  offset  槽内偏移
 * @param  data    数据指针
 * @param  length  字节长度
 * @retval 0 成功，非 0 失败
 */
int bsp_ota_flash_write(uint32_t offset, const uint8_t *data, uint32_t length);

/**
 * @brief  校验槽内镜像 CRC
 * @param  slot_index       槽索引
 * @param  image_size       镜像大小
 * @param  expected_crc32   期望 CRC32
 * @retval 0 校验通过，非 0 失败
 */
int bsp_ota_flash_verify(uint8_t slot_index, uint32_t image_size, uint32_t expected_crc32);

/**
 * @brief  设置当前写入目标槽
 * @param  slot_index  槽索引
 */
void bsp_ota_flash_set_target_slot(uint8_t slot_index);

/**
 * @brief  获取当前目标槽基址
 * @retval Flash 基址
 */
uint32_t bsp_ota_flash_target_base(void);

#endif /* BSP_OTA_FLASH_H */
