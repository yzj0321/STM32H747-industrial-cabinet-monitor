/**
 * @file    cm4_boot_handoff.h
 * @brief   CM7 → CM4 Stub 选槽握手（D3 SRAM）
 */
#ifndef CM4_BOOT_HANDOFF_H
#define CM4_BOOT_HANDOFF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CM4_BOOT_HANDOFF_MAGIC      (0x4A344D43UL)
#define CM4_BOOT_HANDOFF_MAGIC_ADDR (0x3800F010UL)
#define CM4_BOOT_HANDOFF_BASE_ADDR  (0x3800F014UL)

void cm4_boot_handoff_write(uint32_t app_base);
int cm4_boot_handoff_read(uint32_t *app_base);
int cm4_boot_handoff_jump(uint32_t app_base);
int cm4_boot_handoff_jump_from_d3(void);

#ifdef __cplusplus
}
#endif

#endif /* CM4_BOOT_HANDOFF_H */
