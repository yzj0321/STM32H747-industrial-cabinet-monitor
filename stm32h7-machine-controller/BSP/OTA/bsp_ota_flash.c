/**
 * @file    bsp_ota_flash.c
 * @brief   OTA 目标槽位 Flash 擦写与校验实现
 */
#include "bsp_ota_flash.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bsp_flash_ram.h"
#include "bsp_ota_conf.h"
#include "bsp_ota_crc.h"
#include "bsp_watchdog.h"
#include "iwdg.h"
#include "stm32h7xx_hal.h"
#include "usart.h"

/** Flash 调试日志 UART 发送超时（毫秒） */
#define OTA_FLASH_TRACE_TIMEOUT_MS  (100U)
/** 单个 FlashWord 字节数 */
#define OTA_FLASH_WORD_SIZE         (FLASH_NB_32BITWORD_IN_FLASHWORD * sizeof(uint32_t))

/** 当前 OTA 写入目标槽位索引 */
static uint8_t s_target_slot = CM7_APP_SLOT_B_INDEX;
/** 当前写入槽位基地址 */
static uint32_t s_write_base;
/** 已写入字节数 */
static uint32_t s_write_size;
/** 当前目标核 */
static bsp_ota_flash_core_t s_target_core = BSP_OTA_FLASH_CORE_CM7;

/**
 * @brief  获取 Flash 槽基址
 */
static uint32_t ota_flash_slot_base(uint8_t slot_index)
{
    if (s_target_core == BSP_OTA_FLASH_CORE_CM4) {
        return cm4_boot_slot_base(slot_index);
    }

    return boot_slot_base(slot_index);
}

/**
 * @brief  获取 Flash 槽大小
 */
static uint32_t ota_flash_slot_size(void)
{
    return (s_target_core == BSP_OTA_FLASH_CORE_CM4) ? CM4_APP_SLOT_SIZE
                                                     : CM7_APP_SLOT_SIZE;
}

/**
 * @brief  设置 OTA 目标核
 */
void bsp_ota_flash_set_target_core(bsp_ota_flash_core_t core)
{
    s_target_core = core;
}

/**
 * @brief  查询 OTA 目标核
 */
bsp_ota_flash_core_t bsp_ota_flash_target_core(void)
{
    return s_target_core;
}

/**
 * @brief  通过 UART1 输出 Flash 操作调试日志
 * @param  fmt  printf 格式字符串
 * @param  ... 可变参数
 */
static void flash_trace(const char *fmt, ...)
{
    char line[160];
    va_list args;
    int len;

    len = snprintf(line, sizeof(line), "[I][%06lu][OTA-FLASH] ",
                   (unsigned long)HAL_GetTick());
    if ((len < 0) || ((size_t)len >= sizeof(line))) {
        return;
    }

    va_start(args, fmt);
    len += vsnprintf(&line[len], sizeof(line) - (size_t)len, fmt, args);
    va_end(args);

    if (len < 0) {
        return;
    }

    if ((size_t)len > (sizeof(line) - 3U)) {
        len = (int)(sizeof(line) - 3U);
    }

    line[len++] = '\r';
    line[len++] = '\n';
    line[len] = '\0';

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)line, (uint16_t)len,
                            OTA_FLASH_TRACE_TIMEOUT_MS);
}

/**
 * @brief  刷新软件看门狗与独立看门狗，防止 Flash 长操作复位
 */
static void flash_refresh_watchdog(void)
{
    BSP_Watchdog_Kick(BSP_WDG_DEFAULT);
    (void)HAL_IWDG_Refresh(&hiwdg1);
}

/**
 * @brief  擦除指定范围的 Flash 扇区
 * @param  first_sector 起始扇区编号
 * @param  last_sector  结束扇区编号
 * @retval 0 成功，-1 失败
 */
static int flash_erase_sectors(uint32_t first_sector, uint32_t last_sector)
{
    flash_trace("erase range begin sector=%lu..%lu",
                (unsigned long)first_sector, (unsigned long)last_sector);
    flash_refresh_watchdog();

    if (bsp_flash_ram_erase_sectors(first_sector, last_sector) != 0) {
        flash_trace("erase range failed sector=%lu..%lu",
                    (unsigned long)first_sector, (unsigned long)last_sector);
        return -1;
    }

    flash_trace("erase range done sector=%lu..%lu",
                (unsigned long)first_sector, (unsigned long)last_sector);
    return 0;
}

/**
 * @brief  向 Flash 写入数据缓冲区
 * @param  address 目标 Flash 地址
 * @param  data    待写入数据指针
 * @param  length  待写入字节长度
 * @retval 0 成功，-1 失败
 */
static int flash_program_buffer(uint32_t address, const uint8_t *data, uint32_t length)
{
#if (BSP_OTA_FLASH_VERBOSE != 0U)
    flash_trace("program begin addr=0x%08lX len=%lu",
                (unsigned long)address, (unsigned long)length);
#endif

    if ((address % OTA_FLASH_WORD_SIZE) != 0U) {
        flash_trace("program addr not aligned addr=0x%08lX align=%lu",
                    (unsigned long)address,
                    (unsigned long)OTA_FLASH_WORD_SIZE);
        return -1;
    }

    /* H7 Flash 编程必须以 FlashWord(32B) 为单位，底层 bsp_flash_ram 负责补齐 */
    flash_refresh_watchdog();
    if (bsp_flash_ram_program_buffer(address, data, length) != 0) {
        flash_trace("program failed addr=0x%08lX len=%lu",
                    (unsigned long)address, (unsigned long)length);
        return -1;
    }

    flash_refresh_watchdog();
#if (BSP_OTA_FLASH_VERBOSE != 0U)
    flash_trace("program done addr=0x%08lX len=%lu",
                (unsigned long)address, (unsigned long)length);
#endif
    return 0;
}

/**
 * @brief  设置当前 Flash 写入目标槽位
 * @param  slot_index 槽位索引
 */
void bsp_ota_flash_set_target_slot(uint8_t slot_index)
{
    s_target_slot = slot_index;
    s_write_base = ota_flash_slot_base(slot_index);
    s_write_size = 0U;
}

/**
 * @brief  获取当前目标槽位基地址
 * @retval Flash 基地址
 */
uint32_t bsp_ota_flash_target_base(void)
{
    return s_write_base;
}

/**
 * @brief  擦除指定 CM7 应用槽位（S4 Reserve 不参与擦除）
 * @param  slot_index 槽位索引
 * @retval 0 成功，-1 失败
 */
int bsp_ota_flash_erase_slot(uint8_t slot_index)
{
    uint8_t max_slots = (s_target_core == BSP_OTA_FLASH_CORE_CM4) ? CM4_APP_SLOT_COUNT
                                                                    : CM7_APP_SLOT_COUNT;

    if (slot_index >= max_slots) {
        flash_trace("erase slot invalid index=%u core=%u",
                    (unsigned int)slot_index,
                    (unsigned int)s_target_core);
        return -1;
    }

    s_target_slot = slot_index;
    s_write_base = ota_flash_slot_base(slot_index);
    s_write_size = 0U;

    flash_trace("erase slot begin core=%u slot=%u base=0x%08lX",
                (unsigned int)s_target_core,
                (unsigned int)slot_index,
                (unsigned long)s_write_base);

    if (s_target_core == BSP_OTA_FLASH_CORE_CM4) {
        if (slot_index == CM4_APP_SLOT_A_INDEX) {
            if (bsp_flash_ram_erase_bank2_sectors(FLASH_SECTOR_1, FLASH_SECTOR_3) != 0) {
                flash_trace("erase CM4 slot A failed");
                return -1;
            }
            flash_trace("erase CM4 slot A done");
            return 0;
        }

        if (bsp_flash_ram_erase_bank2_sectors(FLASH_SECTOR_5, FLASH_SECTOR_7) != 0) {
            flash_trace("erase CM4 slot B failed");
            return -1;
        }
        flash_trace("erase CM4 slot B done");
        return 0;
    }

    if (slot_index == CM7_APP_SLOT_A_INDEX) {
        if (flash_erase_sectors(FLASH_SECTOR_1, FLASH_SECTOR_3) != 0) {
            flash_trace("erase slot A failed");
            return -1;
        }
        flash_trace("erase slot A done");
        return 0;
    }

    if (flash_erase_sectors(FLASH_SECTOR_5, FLASH_SECTOR_7) != 0) {
        flash_trace("erase slot B failed");
        return -1;
    }
    flash_trace("erase slot B done");
    return 0;
}

/**
 * @brief  向当前目标槽位写入固件数据
 * @param  offset 槽内偏移（字节）
 * @param  data   数据源
 * @param  length 长度（字节）
 * @retval 0 成功，-1 失败
 */
int bsp_ota_flash_write(uint32_t offset, const uint8_t *data, uint32_t length)
{
    uint32_t address;

    if ((data == NULL) || (length == 0U)) {
        return -1;
    }

    if ((offset + length) > ota_flash_slot_size()) {
        return -1;
    }

    address = s_write_base + offset;
    if (flash_program_buffer(address, data, length) != 0) {
        return -1;
    }

    if ((offset + length) > s_write_size) {
        s_write_size = offset + length;
    }

    return 0;
}

/**
 * @brief  校验槽位固件 CRC32
 * @param  slot_index     槽位索引
 * @param  image_size     镜像大小（字节）
 * @param  expected_crc32 期望 CRC32
 * @retval 0 校验通过，-1 校验失败或参数无效
 */
int bsp_ota_flash_verify(uint8_t slot_index, uint32_t image_size, uint32_t expected_crc32)
{
    uint32_t base;
    uint32_t crc;

    if ((slot_index >= CM7_APP_SLOT_COUNT) || (image_size == 0U)) {
        return -1;
    }

    base = ota_flash_slot_base(slot_index);
    if (image_size > ota_flash_slot_size()) {
        return -1;
    }

    /* 直接对 Flash 映射地址做 CRC32，与上位机计算方式一致（初值 0xFFFFFFFF） */
    crc = bsp_ota_crc32((const uint8_t *)base, image_size, 0xFFFFFFFFU);
    return (crc == expected_crc32) ? 0 : -1;
}
