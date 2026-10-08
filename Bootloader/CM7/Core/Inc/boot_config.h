/**
 * @file    boot_config.h
 * @brief   Bootloader 跳转标志与固件标识配置
 */
#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include "boot_slot.h"

/* Boot 跳入 APP 时在 DTCM 尾留魔数；APP 的 SystemInit 据此识别 IAP 并仍执行 RCC 复位 */
#define BOOT_JUMP_FLAG_ADDR     (0x2001FFF0UL)
#define BOOT_JUMP_MAGIC         (0xB00710ADUL)

#define BOOT_FW_TAG             "Bootloader-20260829-1810"

#endif
