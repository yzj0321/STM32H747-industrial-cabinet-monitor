/**
 * @file    boot_eeprom.h
 * @brief   EEPROM 双备份启动元数据（BL24C16F）
 *
 * struct_ver=2：在 CM7 槽信息之外增加 CM4 A/B 槽元数据。
 * 读到 ver=1 旧记录时自动升级为 ver=2（CM4 默认 Slot A）。
 */
#ifndef BOOT_EEPROM_H
#define BOOT_EEPROM_H

#include <stdint.h>
#include "boot_slot.h"

#define BOOT_EE_MAGIC           (0x45454D41UL) /* "EEMA" */
#define BOOT_EE_STRUCT_VER      (2U)
#define BOOT_EE_STRUCT_VER_V1   (1U)
#define BOOT_EE_FLAG_PENDING    (1U << 0)
#define BOOT_EE_FLAG_CM4_PENDING (1U << 0)
#define BOOT_EE_TRIAL_MAX       (3U)

#define BOOT_EE_COPY0_ADDR      (0x0000U)
#define BOOT_EE_COPY1_ADDR      (0x0100U)
#define BOOT_EE_COPY_SIZE       (256U)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t struct_ver;
    uint16_t length;
    uint8_t active_slot;
    uint8_t pending_slot;
    uint8_t boot_trial;
    uint8_t flags;
    uint32_t app_version[CM7_APP_SLOT_COUNT];
    uint32_t app_size[CM7_APP_SLOT_COUNT];
    uint32_t app_crc[CM7_APP_SLOT_COUNT];
    uint8_t cm4_active_slot;
    uint8_t cm4_pending_slot;
    uint8_t cm4_boot_trial;
    uint8_t cm4_flags;
    uint32_t cm4_app_version[CM4_APP_SLOT_COUNT];
    uint32_t cm4_app_size[CM4_APP_SLOT_COUNT];
    uint32_t cm4_app_crc[CM4_APP_SLOT_COUNT];
    uint32_t seq;
    uint32_t crc32;
} boot_eeprom_meta_t;

/**
 * @brief  初始化 EEPROM 并加载/迁移启动元数据
 * @retval 0 成功，非 0 失败
 */
int boot_eeprom_init(void);

/**
 * @brief  查询 EEPROM 启动元数据子系统是否就绪
 * @retval 非 0 就绪，0 未就绪
 */
int boot_eeprom_is_ready(void);

/**
 * @brief  从双备份中加载有效的启动元数据
 * @param  meta  输出缓冲区
 * @retval 0 成功，非 0 失败
 */
int boot_eeprom_load(boot_eeprom_meta_t *meta);

/**
 * @brief  保存启动元数据到双备份区（含 CRC）
 * @param  meta  待写入元数据（会更新 seq/crc）
 * @retval 0 成功，非 0 失败
 */
int boot_eeprom_save(boot_eeprom_meta_t *meta);

/**
 * @brief  根据 meta 选择本次 CM7 启动槽
 * @param  meta  启动元数据
 * @retval 槽索引（A/B）
 */
uint8_t boot_eeprom_select_boot_slot(const boot_eeprom_meta_t *meta);

/**
 * @brief  准备 CM7 启动：处理 pending/trial，必要时回写 meta
 * @param  meta  可修改的启动元数据
 * @retval 最终启动槽索引
 */
uint8_t boot_eeprom_prepare_boot(boot_eeprom_meta_t *meta);

/**
 * @brief  试用启动成功后提交 pending 槽为 active
 * @param  meta  启动元数据
 */
void boot_eeprom_commit_pending(boot_eeprom_meta_t *meta);

/**
 * @brief  启动失败时回退 / 增加 trial 计数
 * @param  meta  启动元数据
 */
void boot_eeprom_on_boot_failure(boot_eeprom_meta_t *meta);

/**
 * @brief  判断指定 CM7 槽镜像元数据是否有效
 * @param  meta        启动元数据
 * @param  slot_index  槽索引
 * @retval 非 0 有效，0 无效
 */
int boot_eeprom_slot_image_valid(const boot_eeprom_meta_t *meta, uint8_t slot_index);

/**
 * @brief  清除指定 CM7 槽的镜像元数据
 * @param  meta        启动元数据
 * @param  slot_index  槽索引
 * @retval 0 成功，非 0 失败
 */
int boot_eeprom_clear_slot_image(boot_eeprom_meta_t *meta, uint8_t slot_index);

/**
 * @brief  设置 CM7 pending 槽及镜像信息（OTA 写入完成后）
 * @param  meta         启动元数据
 * @param  slot_index   pending 槽索引
 * @param  image_size   镜像大小
 * @param  image_crc32  镜像 CRC32
 * @param  version      应用版本号
 * @retval 0 成功，非 0 失败
 */
int boot_eeprom_set_pending(boot_eeprom_meta_t *meta, uint8_t slot_index,
                            uint32_t image_size, uint32_t image_crc32,
                            uint32_t version);

/**
 * @brief  根据 meta 选择本次 CM4 启动槽
 * @param  meta  启动元数据
 * @retval 槽索引（A/B）
 */
uint8_t boot_eeprom_select_cm4_boot_slot(const boot_eeprom_meta_t *meta);

/**
 * @brief  准备 CM4 启动：处理 CM4 pending/trial
 * @param  meta  可修改的启动元数据
 * @retval 最终 CM4 启动槽索引
 */
uint8_t boot_eeprom_prepare_cm4_boot(boot_eeprom_meta_t *meta);

/**
 * @brief  CM4 试用启动成功后提交 pending 槽
 * @param  meta  启动元数据
 */
void boot_eeprom_commit_cm4_pending(boot_eeprom_meta_t *meta);

/**
 * @brief  设置 CM4 pending 槽及镜像信息
 * @param  meta         启动元数据
 * @param  slot_index   pending 槽索引
 * @param  image_size   镜像大小
 * @param  image_crc32  镜像 CRC32
 * @param  version      应用版本号
 * @retval 0 成功，非 0 失败
 */
int boot_eeprom_set_cm4_pending(boot_eeprom_meta_t *meta, uint8_t slot_index,
                                uint32_t image_size, uint32_t image_crc32,
                                uint32_t version);

#endif /* BOOT_EEPROM_H */
