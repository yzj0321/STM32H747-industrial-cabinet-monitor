/**
 * @file    boot_slot.h
 * @brief   CM7 / CM4 Flash 双槽布局
 */
#ifndef BOOT_SLOT_H
#define BOOT_SLOT_H

#include <stdint.h>

#define BOOTLOADER_FLASH_BASE   (0x08000000UL)
#define BOOTLOADER_FLASH_SIZE   (128U * 1024U)

#define CM7_APP_SLOT_COUNT      (2U)
#define CM7_APP_SLOT_SIZE       (384U * 1024U)
#define CM7_APP_SLOT_A_BASE     (0x08020000UL)
#define CM7_APP_SLOT_B_BASE     (0x080A0000UL)
#define CM7_APP_SLOT_A_INDEX    (0U)
#define CM7_APP_SLOT_B_INDEX    (1U)
#define CM7_APP_FLASH_BASE      CM7_APP_SLOT_A_BASE

#define CM4_BOOT_STUB_BASE      (0x08100000UL)
#define CM4_BOOT_STUB_SIZE      (128U * 1024U)
#define CM4_APP_SLOT_COUNT      (2U)
#define CM4_APP_SLOT_SIZE       (384U * 1024U)
#define CM4_APP_SLOT_A_BASE     (0x08120000UL)
#define CM4_APP_SLOT_B_BASE     (0x081A0000UL)
#define CM4_APP_SLOT_A_INDEX    (0U)
#define CM4_APP_SLOT_B_INDEX    (1U)
#define CM4_APP_FLASH_BASE      CM4_APP_SLOT_A_BASE

uint8_t boot_slot_index_from_address(uint32_t address);
uint32_t boot_slot_base(uint8_t slot_index);
uint8_t boot_slot_detect_running(void);
uint8_t boot_slot_inactive_index(uint8_t running_index);
int boot_slot_is_valid(uint8_t slot_index);

uint8_t cm4_boot_slot_index_from_address(uint32_t address);
uint32_t cm4_boot_slot_base(uint8_t slot_index);
uint8_t cm4_boot_slot_inactive_index(uint8_t running_index);
int cm4_boot_slot_is_valid(uint8_t slot_index);

#endif /* BOOT_SLOT_H */
