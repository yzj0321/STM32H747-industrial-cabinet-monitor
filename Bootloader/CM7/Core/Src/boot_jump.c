/**
 * @file    boot_jump.c
 * @brief   Bootloader 跳转应用程序实现
 */
#include "boot_jump.h"

#include "boot_slot.h"

#include "stm32h7xx.h"
#include "stm32h7xx_hal_rcc_ex.h"

/**
 * @brief  阻止 CM4 核从 Flash 启动
 * @retval 无
 */
static void Boot_HoldCm4(void)
{
  CLEAR_BIT(RCC->GCR, RCC_GCR_BOOT_C2);
}

/**
 * @brief  校验指定基地址处应用程序是否有效
 * @param  app_base 应用程序向量表基地址
 * @retval true 有效，false 无效
 */
bool Boot_IsAppValidAt(uint32_t app_base)
{
  uint8_t slot = boot_slot_index_from_address(app_base);

  if (boot_slot_base(slot) != app_base) {
    return false;
  }

  return (boot_slot_is_valid(slot) != 0) ? true : false;
}

/**
 * @brief  校验默认 Slot A 应用程序是否有效
 * @retval true 有效，false 无效
 */
bool Boot_IsAppValid(void)
{
  return Boot_IsAppValidAt(CM7_APP_SLOT_A_BASE);
}

/**
 * @brief  跳转到指定基地址处应用程序
 * @param  app_base 应用程序向量表基地址
 * @retval 无（成功则不返回）
 */
void Boot_JumpToAppAt(uint32_t app_base)
{
  uint32_t app_stack = *(__IO uint32_t *)app_base;
  uint32_t app_entry = *(__IO uint32_t *)(app_base + 4U);

  /* 向量表偏移 0：初始 MSP；偏移 4：Reset_Handler 入口（须 Thumb 态，LSB=1） */
  Boot_HoldCm4();

  __disable_irq();

  /* 跳转前关闭 SysTick 并清 NVIC，避免 Boot 残留中断影响 APP */
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;

  for (uint32_t i = 0U; i < 8U; i++)
  {
    /* 禁用并清除全部 NVIC 通道，防止 Boot 中断服务程序在 APP 中误触发 */
    NVIC->ICER[i] = 0xFFFFFFFFU;
    NVIC->ICPR[i] = 0xFFFFFFFFU;
  }

  /* 写入 DTCM 尾魔数，APP 侧 SystemInit 可区分「Boot 跳转」与「冷启动」 */
  *(__IO uint32_t *)BOOT_JUMP_FLAG_ADDR = BOOT_JUMP_MAGIC;

  SCB->VTOR = app_base;
  __DSB();
  __ISB();

  __set_MSP(app_stack);
  __set_CONTROL(0U);
  __DSB();
  __ISB();

  /* 直接 bx 到 APP Reset_Handler，此后 Bootloader 不再返回 */
  __asm volatile(
    "bx %0\n"
    :
    : "r"(app_entry)
    : "memory");
}

/**
 * @brief  跳转到默认 Slot A 应用程序
 * @retval 无（成功则不返回）
 */
void Boot_JumpToApp(void)
{
  Boot_JumpToAppAt(CM7_APP_SLOT_A_BASE);
}
