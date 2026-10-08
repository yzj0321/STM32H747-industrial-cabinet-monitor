/**
 * @file    bsp_flash_ram.h
 * @brief   在 DTCM (.RamFunc) 中执行的 Flash 擦写接口（Bank1 / Bank2）
 */
#ifndef BSP_FLASH_RAM_H
#define BSP_FLASH_RAM_H

#include <stdint.h>

/**
 * @brief  擦除 Bank1 指定扇区范围（在 RAM 中执行）
 * @param  first_sector  起始扇区号
 * @param  last_sector   结束扇区号（含）
 * @retval 0 成功，非 0 失败
 */
int bsp_flash_ram_erase_sectors(uint32_t first_sector, uint32_t last_sector);

/**
 * @brief  擦除 Bank2 指定扇区范围（在 RAM 中执行）
 * @param  first_sector  起始扇区号
 * @param  last_sector   结束扇区号（含）
 * @retval 0 成功，非 0 失败
 */
int bsp_flash_ram_erase_bank2_sectors(uint32_t first_sector, uint32_t last_sector);

/**
 * @brief  按 FlashWord 对齐向 Flash 编程缓冲区
 * @param  address  目标地址
 * @param  data     数据指针
 * @param  length   字节长度
 * @retval 0 成功，非 0 失败
 */
int bsp_flash_ram_program_buffer(uint32_t address, const uint8_t *data, uint32_t length);

#endif /* BSP_FLASH_RAM_H */
