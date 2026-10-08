/**
 * @file    boot_eeprom.c
 * @brief   EEPROM 双备份启动元数据实现（CM7 + CM4）
 */
#include "boot_eeprom.h"

#include <stdio.h>
#include <string.h>

#include "bsp_eeprom.h"
#include "boot_slot.h"
#include "stm32h7xx_hal.h"

/** 旧 BKPSRAM 元数据，仅用于一次性迁移 */
#define BOOT_META_BKPSRAM_ADDR  (0x38800000UL)
#define BOOT_META_MAGIC         (0xB007B007UL)
#define BOOT_META_VERSION       (1U)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t version;
    uint8_t active_slot;
    uint8_t pending_slot;
    uint8_t boot_trial;
    uint8_t flags;
    uint32_t slot_crc[CM7_APP_SLOT_COUNT];
    uint32_t slot_size[CM7_APP_SLOT_COUNT];
    uint32_t crc32;
} boot_meta_legacy_t;

/** struct_ver=1 布局（升级前），仅用于迁移读取 */
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
    uint32_t seq;
    uint32_t crc32;
} boot_eeprom_meta_v1_t;

static uint8_t s_ee_ready;

/**
 * @brief  计算缓冲区 CRC32
 */
static uint32_t boot_ee_crc32_bytes(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFU;

    for (uint32_t i = 0U; i < length; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

/**
 * @brief  计算 meta v2 结构 CRC32（不含 crc 字段）
 */
static uint32_t boot_ee_crc32(const boot_eeprom_meta_t *meta)
{
    return boot_ee_crc32_bytes((const uint8_t *)meta,
                               sizeof(boot_eeprom_meta_t) - sizeof(uint32_t));
}

/**
 * @brief  计算 meta v1 结构 CRC32
 */
static uint32_t boot_ee_crc32_v1(const boot_eeprom_meta_v1_t *meta)
{
    return boot_ee_crc32_bytes((const uint8_t *)meta,
                               sizeof(boot_eeprom_meta_v1_t) - sizeof(uint32_t));
}

/**
 * @brief  兼容旧版 meta CRC 计算
 */
static uint32_t boot_meta_legacy_crc(const boot_meta_legacy_t *meta)
{
    return boot_ee_crc32_bytes((const uint8_t *)meta,
                               sizeof(boot_meta_legacy_t) - sizeof(uint32_t));
}

/**
 * @brief  填充 CM4 槽默认元数据
 */
static void boot_ee_init_cm4_defaults(boot_eeprom_meta_t *meta)
{
    meta->cm4_active_slot = CM4_APP_SLOT_A_INDEX;
    meta->cm4_pending_slot = CM4_APP_SLOT_A_INDEX;
    meta->cm4_boot_trial = 0U;
    meta->cm4_flags = 0U;
    memset(meta->cm4_app_version, 0, sizeof(meta->cm4_app_version));
    memset(meta->cm4_app_size, 0, sizeof(meta->cm4_app_size));
    memset(meta->cm4_app_crc, 0, sizeof(meta->cm4_app_crc));
}

/**
 * @brief  填充整份启动元数据默认值
 */
static void boot_ee_set_defaults(boot_eeprom_meta_t *meta)
{
    memset(meta, 0, sizeof(*meta));
    meta->magic = BOOT_EE_MAGIC;
    meta->struct_ver = BOOT_EE_STRUCT_VER;
    meta->length = (uint16_t)sizeof(boot_eeprom_meta_t);
    meta->active_slot = CM7_APP_SLOT_A_INDEX;
    meta->pending_slot = CM7_APP_SLOT_A_INDEX;
    meta->boot_trial = 0U;
    meta->flags = 0U;
    boot_ee_init_cm4_defaults(meta);
    meta->seq = 1U;
    meta->crc32 = boot_ee_crc32(meta);
}

static void boot_ee_upgrade_v1_to_v2(const boot_eeprom_meta_v1_t *v1,
                                      boot_eeprom_meta_t *out)
{
    memset(out, 0, sizeof(*out));
    out->magic = BOOT_EE_MAGIC;
    out->struct_ver = BOOT_EE_STRUCT_VER;
    out->length = (uint16_t)sizeof(boot_eeprom_meta_t);
    out->active_slot = v1->active_slot;
    out->pending_slot = v1->pending_slot;
    out->boot_trial = v1->boot_trial;
    out->flags = v1->flags;
    memcpy(out->app_version, v1->app_version, sizeof(out->app_version));
    memcpy(out->app_size, v1->app_size, sizeof(out->app_size));
    memcpy(out->app_crc, v1->app_crc, sizeof(out->app_crc));
    boot_ee_init_cm4_defaults(out);
    out->seq = v1->seq;
    out->crc32 = boot_ee_crc32(out);
}

/**
 * @brief  校验并复制 v2 备份副本
 */
static int boot_ee_copy_valid_v2(const boot_eeprom_meta_t *meta)
{
    if (meta->magic != BOOT_EE_MAGIC) {
        return 0;
    }
    if (meta->struct_ver != BOOT_EE_STRUCT_VER) {
        return 0;
    }
    if (meta->length != (uint16_t)sizeof(boot_eeprom_meta_t)) {
        return 0;
    }
    if (meta->active_slot >= CM7_APP_SLOT_COUNT) {
        return 0;
    }
    if (meta->pending_slot >= CM7_APP_SLOT_COUNT) {
        return 0;
    }
    if (meta->cm4_active_slot >= CM4_APP_SLOT_COUNT) {
        return 0;
    }
    if (meta->cm4_pending_slot >= CM4_APP_SLOT_COUNT) {
        return 0;
    }
    if (meta->crc32 != boot_ee_crc32(meta)) {
        return 0;
    }
    return 1;
}

/**
 * @brief  校验并复制 v1 备份副本
 */
static int boot_ee_copy_valid_v1(const boot_eeprom_meta_v1_t *meta)
{
    if (meta->magic != BOOT_EE_MAGIC) {
        return 0;
    }
    if (meta->struct_ver != BOOT_EE_STRUCT_VER_V1) {
        return 0;
    }
    if (meta->length != (uint16_t)sizeof(boot_eeprom_meta_v1_t)) {
        return 0;
    }
    if (meta->active_slot >= CM7_APP_SLOT_COUNT) {
        return 0;
    }
    if (meta->pending_slot >= CM7_APP_SLOT_COUNT) {
        return 0;
    }
    if (meta->crc32 != boot_ee_crc32_v1(meta)) {
        return 0;
    }
    return 1;
}

/**
 * @brief  读取指定备份并升级为 v2
 */
static int boot_ee_read_copy_as_v2(uint16_t addr, boot_eeprom_meta_t *out)
{
    uint8_t raw[BOOT_EE_COPY_SIZE];
    boot_eeprom_meta_t v2;
    boot_eeprom_meta_v1_t v1;

    if (sizeof(boot_eeprom_meta_t) > BOOT_EE_COPY_SIZE) {
        return 0;
    }

    memset(raw, 0, sizeof(raw));
    if (!BSP_Eeprom_Read(addr, raw, sizeof(raw))) {
        return 0;
    }

    memcpy(&v2, raw, sizeof(v2));
    if (boot_ee_copy_valid_v2(&v2) != 0) {
        *out = v2;
        return 1;
    }

    memcpy(&v1, raw, sizeof(v1));
    if (boot_ee_copy_valid_v1(&v1) != 0) {
        boot_ee_upgrade_v1_to_v2(&v1, out);
        return 1;
    }

    return 0;
}

/**
 * @brief  尝试从 BKPSRAM 迁移旧 meta 到 EEPROM
 */
static int boot_ee_try_migrate_bkpsram(boot_eeprom_meta_t *meta)
{
    const boot_meta_legacy_t *stored = (const boot_meta_legacy_t *)BOOT_META_BKPSRAM_ADDR;
    boot_meta_legacy_t legacy;

    legacy = *stored;
    if ((legacy.magic != BOOT_META_MAGIC) ||
        (legacy.version != BOOT_META_VERSION) ||
        (legacy.crc32 != boot_meta_legacy_crc(&legacy))) {
        return 0;
    }

    boot_ee_set_defaults(meta);
    meta->active_slot = legacy.active_slot;
    meta->pending_slot = legacy.pending_slot;
    meta->boot_trial = legacy.boot_trial;
    meta->flags = legacy.flags;
    memcpy(meta->app_crc, legacy.slot_crc, sizeof(meta->app_crc));
    memcpy(meta->app_size, legacy.slot_size, sizeof(meta->app_size));
    meta->crc32 = boot_ee_crc32(meta);

    if (boot_eeprom_save(meta) != 0) {
        return 0;
    }

    printf("[BOOT] migrated BKPSRAM meta -> EEPROM (active=%u pending=%u)\r\n",
           (unsigned int)meta->active_slot,
           (unsigned int)meta->pending_slot);
    return 1;
}

/**
 * @brief  初始化并加载 EEPROM 启动元数据
 */
int boot_eeprom_init(void)
{
    if (!BSP_Eeprom_IsReady()) {
        s_ee_ready = 0U;
        return -1;
    }

    s_ee_ready = 1U;
    return 0;
}

/**
 * @brief  查询启动元数据子系统是否就绪
 */
int boot_eeprom_is_ready(void)
{
    return (s_ee_ready != 0U) ? 1 : 0;
}

/**
 * @brief  加载有效启动元数据
 */
int boot_eeprom_load(boot_eeprom_meta_t *meta)
{
    boot_eeprom_meta_t c0;
    boot_eeprom_meta_t c1;
    int c0_ok;
    int c1_ok;

    if (meta == NULL) {
        return -1;
    }

    boot_ee_set_defaults(meta);

    if (s_ee_ready == 0U) {
        return 1;
    }

    c0_ok = boot_ee_read_copy_as_v2(BOOT_EE_COPY0_ADDR, &c0);
    c1_ok = boot_ee_read_copy_as_v2(BOOT_EE_COPY1_ADDR, &c1);

    if ((c0_ok != 0) && (c1_ok != 0)) {
        *meta = (c0.seq >= c1.seq) ? c0 : c1;
        return 0;
    }
    if (c0_ok != 0) {
        *meta = c0;
        return 0;
    }
    if (c1_ok != 0) {
        *meta = c1;
        return 0;
    }

    if (boot_ee_try_migrate_bkpsram(meta) != 0) {
        return 0;
    }

    printf("[BOOT] EEPROM meta empty/invalid, use defaults (CM7/CM4 active=A)\r\n");
    return 1;
}

/**
 * @brief  保存启动元数据到双备份区
 */
int boot_eeprom_save(boot_eeprom_meta_t *meta)
{
    boot_eeprom_meta_t c0;
    boot_eeprom_meta_t c1;
    uint16_t target;
    int c0_ok;
    int c1_ok;

    if ((meta == NULL) || (s_ee_ready == 0U)) {
        return -1;
    }

    meta->magic = BOOT_EE_MAGIC;
    meta->struct_ver = BOOT_EE_STRUCT_VER;
    meta->length = (uint16_t)sizeof(boot_eeprom_meta_t);
    meta->seq++;
    meta->crc32 = boot_ee_crc32(meta);

    c0_ok = boot_ee_read_copy_as_v2(BOOT_EE_COPY0_ADDR, &c0);
    c1_ok = boot_ee_read_copy_as_v2(BOOT_EE_COPY1_ADDR, &c1);

    if ((c0_ok != 0) && (c1_ok != 0)) {
        target = (c0.seq <= c1.seq) ? BOOT_EE_COPY0_ADDR : BOOT_EE_COPY1_ADDR;
    } else if (c0_ok == 0) {
        target = BOOT_EE_COPY0_ADDR;
    } else {
        target = BOOT_EE_COPY1_ADDR;
    }

    if (!BSP_Eeprom_Write(target, (const uint8_t *)meta, sizeof(*meta))) {
        return -1;
    }

    return 0;
}

/**
 * @brief  选择 CM7 启动槽
 */
uint8_t boot_eeprom_select_boot_slot(const boot_eeprom_meta_t *meta)
{
    if (meta == NULL) {
        return CM7_APP_SLOT_A_INDEX;
    }

    if (((meta->flags & BOOT_EE_FLAG_PENDING) != 0U) &&
        (meta->pending_slot < CM7_APP_SLOT_COUNT) &&
        (boot_eeprom_slot_image_valid(meta, meta->pending_slot) != 0) &&
        (boot_slot_is_valid(meta->pending_slot) != 0)) {
        return meta->pending_slot;
    }

    if ((meta->active_slot < CM7_APP_SLOT_COUNT) &&
        (boot_slot_is_valid(meta->active_slot) != 0)) {
        return meta->active_slot;
    }

    if (boot_slot_is_valid(CM7_APP_SLOT_A_INDEX) != 0) {
        return CM7_APP_SLOT_A_INDEX;
    }

    if (boot_slot_is_valid(CM7_APP_SLOT_B_INDEX) != 0) {
        return CM7_APP_SLOT_B_INDEX;
    }

    return CM7_APP_SLOT_A_INDEX;
}

/**
 * @brief  准备 CM7 启动（pending/trial）
 */
uint8_t boot_eeprom_prepare_boot(boot_eeprom_meta_t *meta)
{
    uint8_t slot;

    if (meta == NULL) {
        return CM7_APP_SLOT_A_INDEX;
    }

    slot = boot_eeprom_select_boot_slot(meta);

    if (((meta->flags & BOOT_EE_FLAG_PENDING) != 0U) &&
        (slot == meta->pending_slot)) {
        if (meta->boot_trial > 0U) {
            meta->boot_trial--;
        }

        if (meta->boot_trial == 0U) {
            printf("[BOOT] CM7 pending trial exhausted, fallback active=%u\r\n",
                   (unsigned int)meta->active_slot);
            meta->flags &= (uint8_t)~BOOT_EE_FLAG_PENDING;
            meta->pending_slot = meta->active_slot;
            slot = meta->active_slot;
            if (boot_slot_is_valid(slot) == 0) {
                slot = boot_eeprom_select_boot_slot(meta);
            }
        }

        (void)boot_eeprom_save(meta);
    }

    return slot;
}

/**
 * @brief  提交 CM7 pending 槽为 active
 */
void boot_eeprom_commit_pending(boot_eeprom_meta_t *meta)
{
    if ((meta == NULL) || ((meta->flags & BOOT_EE_FLAG_PENDING) == 0U)) {
        return;
    }

    meta->active_slot = meta->pending_slot;
    meta->boot_trial = 0U;
    meta->flags &= (uint8_t)~BOOT_EE_FLAG_PENDING;
    (void)boot_eeprom_save(meta);
}

/**
 * @brief  处理 CM7 启动失败回退
 */
void boot_eeprom_on_boot_failure(boot_eeprom_meta_t *meta)
{
    if (meta == NULL) {
        return;
    }

    if (meta->boot_trial > 0U) {
        meta->boot_trial--;
    }

    if (meta->boot_trial == 0U) {
        meta->flags &= (uint8_t)~BOOT_EE_FLAG_PENDING;
        meta->pending_slot = meta->active_slot;
    }

    (void)boot_eeprom_save(meta);
}

/**
 * @brief  判断 CM7 槽镜像元数据是否有效
 */
int boot_eeprom_slot_image_valid(const boot_eeprom_meta_t *meta, uint8_t slot_index)
{
    if ((meta == NULL) || (slot_index >= CM7_APP_SLOT_COUNT)) {
        return 0;
    }

    if ((meta->app_size[slot_index] == 0U) ||
        (meta->app_size[slot_index] > CM7_APP_SLOT_SIZE)) {
        return 0;
    }

    return 1;
}

/**
 * @brief  清除 CM7 槽镜像元数据
 */
int boot_eeprom_clear_slot_image(boot_eeprom_meta_t *meta, uint8_t slot_index)
{
    if ((meta == NULL) || (slot_index >= CM7_APP_SLOT_COUNT)) {
        return -1;
    }

    meta->app_size[slot_index] = 0U;
    meta->app_crc[slot_index] = 0U;
    meta->app_version[slot_index] = 0U;

    if (s_ee_ready == 0U) {
        return -1;
    }

    return boot_eeprom_save(meta);
}

int boot_eeprom_set_pending(boot_eeprom_meta_t *meta, uint8_t slot_index,
                            uint32_t image_size, uint32_t image_crc32,
                            uint32_t version)
{
    if ((meta == NULL) || (slot_index >= CM7_APP_SLOT_COUNT)) {
        return -1;
    }

    if ((image_size == 0U) || (image_size > CM7_APP_SLOT_SIZE)) {
        return -1;
    }

    meta->pending_slot = slot_index;
    meta->app_size[slot_index] = image_size;
    meta->app_crc[slot_index] = image_crc32;
    meta->app_version[slot_index] = version;
    meta->boot_trial = BOOT_EE_TRIAL_MAX;
    meta->flags |= BOOT_EE_FLAG_PENDING;

    if (s_ee_ready == 0U) {
        return -1;
    }

    return boot_eeprom_save(meta);
}

/**
 * @brief  选择 CM4 启动槽
 */
uint8_t boot_eeprom_select_cm4_boot_slot(const boot_eeprom_meta_t *meta)
{
    if (meta == NULL) {
        return CM4_APP_SLOT_A_INDEX;
    }

    if (((meta->cm4_flags & BOOT_EE_FLAG_CM4_PENDING) != 0U) &&
        (meta->cm4_pending_slot < CM4_APP_SLOT_COUNT) &&
        (cm4_boot_slot_is_valid(meta->cm4_pending_slot) != 0)) {
        return meta->cm4_pending_slot;
    }

    if ((meta->cm4_active_slot < CM4_APP_SLOT_COUNT) &&
        (cm4_boot_slot_is_valid(meta->cm4_active_slot) != 0)) {
        return meta->cm4_active_slot;
    }

    if (cm4_boot_slot_is_valid(CM4_APP_SLOT_A_INDEX) != 0) {
        return CM4_APP_SLOT_A_INDEX;
    }

    if (cm4_boot_slot_is_valid(CM4_APP_SLOT_B_INDEX) != 0) {
        return CM4_APP_SLOT_B_INDEX;
    }

    return CM4_APP_SLOT_A_INDEX;
}

/**
 * @brief  准备 CM4 启动（pending/trial）
 */
uint8_t boot_eeprom_prepare_cm4_boot(boot_eeprom_meta_t *meta)
{
    uint8_t slot;

    if (meta == NULL) {
        return CM4_APP_SLOT_A_INDEX;
    }

    slot = boot_eeprom_select_cm4_boot_slot(meta);

    if (((meta->cm4_flags & BOOT_EE_FLAG_CM4_PENDING) != 0U) &&
        (slot == meta->cm4_pending_slot)) {
        if (meta->cm4_boot_trial > 0U) {
            meta->cm4_boot_trial--;
        }

        if (meta->cm4_boot_trial == 0U) {
            printf("[BOOT] CM4 pending trial exhausted, fallback active=%u\r\n",
                   (unsigned int)meta->cm4_active_slot);
            meta->cm4_flags &= (uint8_t)~BOOT_EE_FLAG_CM4_PENDING;
            meta->cm4_pending_slot = meta->cm4_active_slot;
            slot = meta->cm4_active_slot;
            if (cm4_boot_slot_is_valid(slot) == 0) {
                slot = boot_eeprom_select_cm4_boot_slot(meta);
            }
        }

        (void)boot_eeprom_save(meta);
    }

    return slot;
}

/**
 * @brief  提交 CM4 pending 槽为 active
 */
void boot_eeprom_commit_cm4_pending(boot_eeprom_meta_t *meta)
{
    if ((meta == NULL) || ((meta->cm4_flags & BOOT_EE_FLAG_CM4_PENDING) == 0U)) {
        return;
    }

    meta->cm4_active_slot = meta->cm4_pending_slot;
    meta->cm4_boot_trial = 0U;
    meta->cm4_flags &= (uint8_t)~BOOT_EE_FLAG_CM4_PENDING;
    (void)boot_eeprom_save(meta);
}

int boot_eeprom_set_cm4_pending(boot_eeprom_meta_t *meta, uint8_t slot_index,
                                uint32_t image_size, uint32_t image_crc32,
                                uint32_t version)
{
    if ((meta == NULL) || (slot_index >= CM4_APP_SLOT_COUNT)) {
        return -1;
    }

    if ((image_size == 0U) || (image_size > CM4_APP_SLOT_SIZE)) {
        return -1;
    }

    meta->cm4_pending_slot = slot_index;
    meta->cm4_app_size[slot_index] = image_size;
    meta->cm4_app_crc[slot_index] = image_crc32;
    meta->cm4_app_version[slot_index] = version;
    meta->cm4_boot_trial = BOOT_EE_TRIAL_MAX;
    meta->cm4_flags |= BOOT_EE_FLAG_CM4_PENDING;

    if (s_ee_ready == 0U) {
        return -1;
    }

    return boot_eeprom_save(meta);
}
