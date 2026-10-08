/**
 * @file    bsp_modbus_coil_nv.c
 * @brief   Modbus 线圈 NV：写入 BL24C16F EEPROM 用户区双备份
 *
 * 布局见 bsp_nvm_layout.h：
 *   Copy0 @ 0x0200, Copy1 @ 0x0240（各 64B）
 * 不与 Boot meta（0x0000~0x01FF）冲突；seq 轮换写入，CRC32 校验。
 */
#include "bsp_modbus_coil_nv.h"

#include <stdio.h>
#include <string.h>

#include "bsp_eeprom.h"
#include "bsp_nvm_layout.h"

#define MODBUS_COIL_NV_MAGIC        (0x4D42434CUL) /* 'MBCL' */
#define MODBUS_COIL_NV_STRUCT_VER   (1U)
#define MODBUS_COIL_NV_SLOTS        (16U)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t struct_ver;
    uint16_t coil_count;
    uint8_t coils[MODBUS_COIL_NV_SLOTS];
    uint32_t seq;
    uint32_t crc32;
} modbus_coil_nv_t;

_Static_assert(sizeof(modbus_coil_nv_t) <= BSP_NVM_MODBUS_COIL_COPY_SIZE,
               "modbus_coil_nv_t exceeds reserved EEPROM copy size");

static uint32_t s_last_seq;

/**
 * @brief  计算线圈 NV 记录 CRC32
 */
static uint32_t coil_nv_crc32(const modbus_coil_nv_t *rec)
{
    const uint8_t *data = (const uint8_t *)rec;
    const uint32_t length = (uint32_t)(sizeof(modbus_coil_nv_t) - sizeof(uint32_t));
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
 * @brief  校验线圈 NV 记录有效性
 */
static int coil_nv_valid(const modbus_coil_nv_t *rec)
{
    if ((rec == NULL) ||
        (rec->magic != MODBUS_COIL_NV_MAGIC) ||
        (rec->struct_ver != MODBUS_COIL_NV_STRUCT_VER) ||
        (rec->coil_count == 0U) ||
        (rec->coil_count > MODBUS_COIL_NV_SLOTS)) {
        return 0;
    }

    return (rec->crc32 == coil_nv_crc32(rec)) ? 1 : 0;
}

/**
 * @brief  读取一份线圈 NV 备份
 */
static int coil_nv_read_copy(uint16_t addr, modbus_coil_nv_t *out)
{
    uint8_t raw[BSP_NVM_MODBUS_COIL_COPY_SIZE];

    memset(out, 0, sizeof(*out));
    memset(raw, 0xFF, sizeof(raw));

    if (!BSP_Eeprom_Read(addr, raw, sizeof(raw))) {
        return 0;
    }

    memcpy(out, raw, sizeof(*out));
    return coil_nv_valid(out);
}

/**
 * @brief  查询线圈 NV 介质是否就绪
 */
bool BSP_Modbus_CoilNv_MediaReady(void)
{
    return BSP_Eeprom_IsReady();
}

/**
 * @brief  初始化线圈 NV
 */
bool BSP_Modbus_CoilNv_Init(void)
{
    return BSP_Eeprom_IsReady();
}

/**
 * @brief  加载线圈 NV
 */
bool BSP_Modbus_CoilNv_Load(uint8_t *coils, uint16_t count)
{
    modbus_coil_nv_t c0;
    modbus_coil_nv_t c1;
    const modbus_coil_nv_t *best = NULL;
    int c0_ok;
    int c1_ok;
    uint16_t n;
    uint16_t i;

    if ((coils == NULL) || (count == 0U) || !BSP_Eeprom_IsReady()) {
        return false;
    }

    c0_ok = coil_nv_read_copy(BSP_NVM_MODBUS_COIL_COPY0_OFFSET, &c0);
    c1_ok = coil_nv_read_copy(BSP_NVM_MODBUS_COIL_COPY1_OFFSET, &c1);

    if ((c0_ok != 0) && (c1_ok != 0)) {
        best = (c0.seq >= c1.seq) ? &c0 : &c1;
    } else if (c0_ok != 0) {
        best = &c0;
    } else if (c1_ok != 0) {
        best = &c1;
    } else {
        printf("[MB-NV] coil EEPROM empty/invalid, keep defaults\r\n");
        return false;
    }

    n = (count < best->coil_count) ? count : best->coil_count;
    if (n > MODBUS_COIL_NV_SLOTS) {
        n = MODBUS_COIL_NV_SLOTS;
    }

    for (i = 0U; i < n; i++) {
        coils[i] = (best->coils[i] != 0U) ? 1U : 0U;
    }
    for (; i < count; i++) {
        coils[i] = 0U;
    }

    s_last_seq = best->seq;
    printf("[MB-NV] coil load OK seq=%lu count=%u\r\n",
           (unsigned long)best->seq, (unsigned int)n);
    return true;
}

/**
 * @brief  保存线圈 NV
 */
bool BSP_Modbus_CoilNv_Save(const uint8_t *coils, uint16_t count)
{
    modbus_coil_nv_t rec;
    modbus_coil_nv_t c0;
    modbus_coil_nv_t c1;
    uint8_t page[BSP_NVM_MODBUS_COIL_COPY_SIZE];
    uint16_t target;
    uint16_t n;
    uint16_t i;
    int c0_ok;
    int c1_ok;

    if ((coils == NULL) || (count == 0U) || !BSP_Eeprom_IsReady()) {
        return false;
    }

    n = (count < MODBUS_COIL_NV_SLOTS) ? count : (uint16_t)MODBUS_COIL_NV_SLOTS;

    memset(&rec, 0, sizeof(rec));
    rec.magic = MODBUS_COIL_NV_MAGIC;
    rec.struct_ver = MODBUS_COIL_NV_STRUCT_VER;
    rec.coil_count = n;
    for (i = 0U; i < n; i++) {
        rec.coils[i] = (coils[i] != 0U) ? 1U : 0U;
    }

    c0_ok = coil_nv_read_copy(BSP_NVM_MODBUS_COIL_COPY0_OFFSET, &c0);
    c1_ok = coil_nv_read_copy(BSP_NVM_MODBUS_COIL_COPY1_OFFSET, &c1);

    if ((c0_ok != 0) && (c1_ok != 0)) {
        rec.seq = ((c0.seq >= c1.seq) ? c0.seq : c1.seq) + 1U;
        target = (c0.seq <= c1.seq) ? BSP_NVM_MODBUS_COIL_COPY0_OFFSET
                                    : BSP_NVM_MODBUS_COIL_COPY1_OFFSET;
    } else if (c0_ok != 0) {
        rec.seq = c0.seq + 1U;
        target = BSP_NVM_MODBUS_COIL_COPY1_OFFSET;
    } else if (c1_ok != 0) {
        rec.seq = c1.seq + 1U;
        target = BSP_NVM_MODBUS_COIL_COPY0_OFFSET;
    } else {
        rec.seq = (s_last_seq != 0U) ? (s_last_seq + 1U) : 1U;
        target = BSP_NVM_MODBUS_COIL_COPY0_OFFSET;
    }

    rec.crc32 = coil_nv_crc32(&rec);

    memset(page, 0xFF, sizeof(page));
    memcpy(page, &rec, sizeof(rec));

    if (!BSP_Eeprom_Write(target, page, sizeof(page))) {
        printf("[MB-NV] coil save fail @ 0x%04X seq=%lu\r\n",
               (unsigned int)target, (unsigned long)rec.seq);
        return false;
    }

    s_last_seq = rec.seq;
    printf("[MB-NV] coil save OK @ 0x%04X seq=%lu count=%u\r\n",
           (unsigned int)target, (unsigned long)rec.seq, (unsigned int)n);
    return true;
}
