/**
 * @file    bsp_flash_ram.h
 * @brief   在 DTCM 中执行的 Bank1 Flash 擦写接口
 */
#ifndef BSP_FLASH_RAM_H
#define BSP_FLASH_RAM_H

#include <stdint.h>

/**
 * @brief  擦除 Bank1 指定范围内的 Flash 扇区
 * @param  first_sector 起始扇区编号
 * @param  last_sector  结束扇区编号
 * @retval 0 成功，-1 失败
 */
int bsp_flash_ram_erase_sectors(uint32_t first_sector, uint32_t last_sector);

/**
 * @brief  向 Bank1 Flash 按 FlashWord 对齐写入数据缓冲区
 * @param  address 目标 Flash 地址（须 FlashWord 对齐）
 * @param  data    待写入数据指针
 * @param  length  待写入字节长度
 * @retval 0 成功，-1 失败
 */
int bsp_flash_ram_program_buffer(uint32_t address, const uint8_t *data, uint32_t length);

#endif /* BSP_FLASH_RAM_H */
