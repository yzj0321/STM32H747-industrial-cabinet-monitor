/**
 * @file    boot_slot_copy.h
 * @brief   OTA 槽镜像复制到执行分区（Slot A）接口
 */
#ifndef BOOT_SLOT_COPY_H
#define BOOT_SLOT_COPY_H

#include <stdint.h>

/**
 * @brief  将源槽镜像复制到 Slot A（执行分区）
 * @param  src_slot 源槽索引（通常为 Slot B）
 * @retval 0 成功，-1 失败
 */
int boot_slot_copy_to_run_slot(uint8_t src_slot);

#endif /* BOOT_SLOT_COPY_H */
