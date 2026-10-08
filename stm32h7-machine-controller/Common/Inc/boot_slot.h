/**
 * @file    boot_slot.h
 * @brief   CM7 / CM4 Flash 双槽布局（镜像元数据在 EEPROM）
 *
 * Bank1 (1MB) — CM7：
 *   Boot        0x08000000  128KB   S0
 *   Slot A      0x08020000  384KB   至 0x0807FFFF（S1~S3）
 *   Reserve     0x08080000  128KB   至 0x0809FFFF（S4 预留，不参与 OTA）
 *   Slot B      0x080A0000  384KB   至 0x080FFFFF（S5~S7）
 *
 * Bank2 (1MB) — CM4：
 *   Stub        0x08100000  128KB   S0（固定入口，OTA 不擦）
 *   Slot A      0x08120000  384KB   至 0x0817FFFF（S1~S3）
 *   Reserve     0x08180000  128KB   至 0x0819FFFF（S4 预留）
 *   Slot B      0x081A0000  384KB   至 0x081FFFFF（S5~S7）
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

/** 出厂默认烧录/链接分区（Slot A） */
#define CM7_APP_FLASH_BASE      CM7_APP_SLOT_A_BASE

#define CM4_BOOT_STUB_BASE      (0x08100000UL)
#define CM4_BOOT_STUB_SIZE      (128U * 1024U)

#define CM4_APP_SLOT_COUNT      (2U)
#define CM4_APP_SLOT_SIZE       (384U * 1024U)

#define CM4_APP_SLOT_A_BASE     (0x08120000UL)
#define CM4_APP_SLOT_B_BASE     (0x081A0000UL)

#define CM4_APP_SLOT_A_INDEX    (0U)
#define CM4_APP_SLOT_B_INDEX    (1U)

/** 出厂默认 CM4 应用分区（Slot A） */
#define CM4_APP_FLASH_BASE      CM4_APP_SLOT_A_BASE

/**
 * @brief  由 Flash 地址推断 CM7 槽索引
 * @param  address  Flash 地址
 * @retval 槽索引；非法地址返回无效索引
 */
uint8_t boot_slot_index_from_address(uint32_t address);

/**
 * @brief  获取 CM7 指定槽的基址
 * @param  slot_index  槽索引
 * @retval 槽基址
 */
uint32_t boot_slot_base(uint8_t slot_index);

/**
 * @brief  检测当前正在运行的 CM7 应用槽
 * @retval 运行中槽索引
 */
uint8_t boot_slot_detect_running(void);

/**
 * @brief  由运行槽得到对端（非活动）槽索引
 * @param  running_index  当前运行槽
 * @retval 对端槽索引
 */
uint8_t boot_slot_inactive_index(uint8_t running_index);

/**
 * @brief  判断 CM7 槽索引是否合法
 * @param  slot_index  槽索引
 * @retval 非 0 合法，0 非法
 */
int boot_slot_is_valid(uint8_t slot_index);

/**
 * @brief  由 Flash 地址推断 CM4 槽索引
 * @param  address  Flash 地址
 * @retval 槽索引
 */
uint8_t cm4_boot_slot_index_from_address(uint32_t address);

/**
 * @brief  获取 CM4 指定槽的基址
 * @param  slot_index  槽索引
 * @retval 槽基址
 */
uint32_t cm4_boot_slot_base(uint8_t slot_index);

/**
 * @brief  由运行槽得到 CM4 对端槽索引
 * @param  running_index  当前运行槽
 * @retval 对端槽索引
 */
uint8_t cm4_boot_slot_inactive_index(uint8_t running_index);

/**
 * @brief  判断 CM4 槽索引是否合法
 * @param  slot_index  槽索引
 * @retval 非 0 合法，0 非法
 */
int cm4_boot_slot_is_valid(uint8_t slot_index);

/**
 * @brief  检测当前正在运行的 CM4 应用槽
 * @retval 运行中槽索引
 */
uint8_t cm4_boot_slot_detect_running(void);

#endif /* BOOT_SLOT_H */
