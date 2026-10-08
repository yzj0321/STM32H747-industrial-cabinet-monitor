/**
 * @file    cm4_boot_handoff.h
 * @brief   CM7 → CM4 Stub 选槽握手（D3 SRAM）
 *
 * CM7 在 EnableBootCore 之前写入 magic + app_base；
 * Bootloader/CM4 Stub 读取后跳转到 Slot A/B。
 */
#ifndef CM4_BOOT_HANDOFF_H
#define CM4_BOOT_HANDOFF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 小端 'CM4J' */
#define CM4_BOOT_HANDOFF_MAGIC      (0x4A344D43UL)

/** OpenAMP SHM 止于 0x3800F000；状态字之后空闲 */
#define CM4_BOOT_HANDOFF_MAGIC_ADDR (0x3800F010UL)
#define CM4_BOOT_HANDOFF_BASE_ADDR  (0x3800F014UL)

/** CM4 应用启动后写入，供 CM7 确认 trial boot */
#define CM4_BOOT_ALIVE_MAGIC_ADDR   (0x3800F008UL)
#define CM4_BOOT_ALIVE_SLOT_ADDR    (0x3800F00CUL)
#define CM4_BOOT_ALIVE_MAGIC        (0x414C5645UL) /* 'ALVE' */

/** CM4 启动/IPC 诊断状态（临时调试用） */
#define CM4_IPC_DIAG_ADDR           (0x3800F020UL)
#define CM4_IPC_DIAG_MAIN_READY     (1U)
#define CM4_IPC_DIAG_IPC_START      (2U)
#define CM4_IPC_DIAG_IPC_READY      (3U)
#define CM4_IPC_DIAG_IPC_FAIL       (4U)
#define CM4_IPC_DIAG_DEFAULT_READY  (5U)

/**
 * @brief  CM7 写入 CM4 应用入口基址到 D3 握手区
 * @param  app_base  CM4 Slot 基址
 */
void cm4_boot_handoff_write(uint32_t app_base);

/**
 * @brief  读取 D3 中的 CM4 应用基址
 * @param  app_base  输出基址
 * @retval 0 成功，非 0 magic 无效
 */
int cm4_boot_handoff_read(uint32_t *app_base);

/**
 * @brief  校验向量表后跳转到指定 CM4 应用
 * @param  app_base  应用基址
 * @retval 正常不返回；失败返回非 0
 */
int cm4_boot_handoff_jump(uint32_t app_base);

/**
 * @brief  从 D3 读取基址并跳转（Stub 入口使用）
 * @retval 正常不返回；失败返回非 0
 */
int cm4_boot_handoff_jump_from_d3(void);

#if defined(CORE_CM4)
/**
 * @brief  CM4 应用启动后写 alive 标志（供 CM7 trial 确认）
 * @param  slot_index  当前运行槽
 */
void cm4_boot_alive_signal(uint8_t slot_index);
#endif
#if defined(CORE_CM7)
/**
 * @brief  CM7 消费 CM4 alive 标志
 * @param  slot_index  输出槽索引（可为 NULL）
 * @retval 0 已消费到有效标志，非 0 无标志
 */
int cm4_boot_alive_consume(uint8_t *slot_index);
#endif

/**
 * @brief  清除 CM4 alive 标志区
 */
void cm4_boot_alive_clear(void);

void cm4_ipc_diag_set(uint32_t stage);
uint32_t cm4_ipc_diag_get(void);

#ifdef __cplusplus
}
#endif

#endif /* CM4_BOOT_HANDOFF_H */
