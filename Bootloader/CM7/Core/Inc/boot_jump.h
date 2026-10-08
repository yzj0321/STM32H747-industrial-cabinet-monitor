/**
 * @file    boot_jump.h
 * @brief   Bootloader 跳转应用程序接口
 */
#ifndef BOOT_JUMP_H
#define BOOT_JUMP_H

#include <stdint.h>
#include <stdbool.h>
#include "boot_config.h"

/**
 * @brief  校验默认 Slot A 应用程序是否有效
 * @retval true 有效，false 无效
 */
bool Boot_IsAppValid(void);

/**
 * @brief  校验指定基地址处应用程序是否有效
 * @param  app_base 应用程序向量表基地址
 * @retval true 有效，false 无效
 */
bool Boot_IsAppValidAt(uint32_t app_base);

/**
 * @brief  跳转到默认 Slot A 应用程序
 * @retval 无（成功则不返回）
 */
void Boot_JumpToApp(void);

/**
 * @brief  跳转到指定基地址处应用程序
 * @param  app_base 应用程序向量表基地址
 * @retval 无（成功则不返回）
 */
void Boot_JumpToAppAt(uint32_t app_base);

#endif
