/**
 * @file    bsp_nvm_layout.h
 * @brief   外置 EEPROM 非易失性存储分区布局（App / Bootloader 共用，对齐 CamView）
 */
#ifndef BSP_NVM_LAYOUT_H
#define BSP_NVM_LAYOUT_H

#include <stdint.h>

#define BSP_NVM_LAYOUT_MAGIC     (0x4E564D21UL) /* 'NVM!' */
#define BSP_NVM_LAYOUT_VERSION   (1U)

/** boot_eeprom_meta_t 双备份 Copy0 */
#define BSP_NVM_OTA_META_COPY0_OFFSET     (0x0000U)
/** boot_eeprom_meta_t 双备份 Copy1 */
#define BSP_NVM_OTA_META_COPY1_OFFSET     (0x0100U)
/** 每份元数据区预留长度 */
#define BSP_NVM_OTA_META_COPY_SIZE        (256U)
/** 应用用户区（参数、自检等）起始 */
#define BSP_NVM_USER_BASE_OFFSET          (0x0200U)

/** Modbus 线圈 NV 双备份（各 64B，位于用户区，不与 Boot meta 重叠） */
#define BSP_NVM_MODBUS_COIL_COPY0_OFFSET  (0x0200U)
#define BSP_NVM_MODBUS_COIL_COPY1_OFFSET  (0x0240U)
#define BSP_NVM_MODBUS_COIL_COPY_SIZE     (64U)
/** 用户区其余空间从 0x0280 起仍可留给其他参数 */
#define BSP_NVM_USER_FREE_OFFSET          (0x0280U)

#endif /* BSP_NVM_LAYOUT_H */
