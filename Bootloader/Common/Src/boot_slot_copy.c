/**
 * @file    boot_slot_copy.c
 * @brief   OTA 槽镜像复制到执行分区（Slot A）实现（legacy，元数据来自 EEPROM）
 */
#include "boot_slot_copy.h"

#include <string.h>

#include "boot_eeprom.h"
#include "boot_slot.h"
#include "bsp_flash_ram.h"
#include "stm32h7xx_hal.h"

#define BOOT_COPY_CHUNK           (256U)

/**
 * @brief  擦除指定范围的 Flash 扇区（封装 bsp_flash_ram）
 */
static int flash_erase_sectors(uint32_t first_sector, uint32_t last_sector)
{
    return bsp_flash_ram_erase_sectors(first_sector, last_sector);
}

/**
 * @brief  向 Flash 写入数据缓冲区（封装 bsp_flash_ram）
 */
static int flash_program_buffer(uint32_t address, const uint8_t *data, uint32_t length)
{
    return bsp_flash_ram_program_buffer(address, data, length);
}

/**
 * @brief  分块将源地址镜像复制到目标 Flash 地址
 */
static int copy_image(uint32_t src_base, uint32_t dst_base, uint32_t image_size)
{
    uint8_t chunk[BOOT_COPY_CHUNK];
    uint32_t offset = 0U;

    while (offset < image_size) {
        uint32_t n = image_size - offset;

        if (n > BOOT_COPY_CHUNK) {
            n = BOOT_COPY_CHUNK;
        }

        memcpy(chunk, (const void *)(src_base + offset), n);

        if (flash_program_buffer(dst_base + offset, chunk, n) != 0) {
            return -1;
        }

        offset += n;
    }

    return 0;
}

/**
 * @brief  将源槽镜像复制到 Slot A（执行分区）
 * @param  src_slot 源槽索引（通常为 Slot B）
 * @retval 0 成功，-1 失败
 */
int boot_slot_copy_to_run_slot(uint8_t src_slot)
{
    boot_eeprom_meta_t meta;
    uint32_t src_base;
    uint32_t image_size;

    if (src_slot >= CM7_APP_SLOT_COUNT) {
        return -1;
    }

    if (boot_eeprom_is_ready() == 0) {
        return -1;
    }

    (void)boot_eeprom_load(&meta);
    if (boot_eeprom_slot_image_valid(&meta, src_slot) == 0) {
        return -1;
    }

    image_size = meta.app_size[src_slot];
    src_base = boot_slot_base(src_slot);

    /* Slot A 占 S1~S3；S4 为 Reserve，与 Slot B 无重叠 */
    if (flash_erase_sectors(FLASH_SECTOR_1, FLASH_SECTOR_3) != 0) {
        return -1;
    }

    return copy_image(src_base, CM7_APP_SLOT_A_BASE, image_size);
}
