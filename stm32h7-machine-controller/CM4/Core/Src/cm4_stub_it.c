/**
 * @file    cm4_stub_it.c
 * @brief   CM4 Stub 最小中断处理（无 USART/DMA）
 */
#include "stm32h7xx_it.h"
#include "main.h"

extern TIM_HandleTypeDef htim7;

/** @brief 不可屏蔽中断 */
void NMI_Handler(void)
{
}

/** @brief 硬件故障：死循环 */
void HardFault_Handler(void)
{
    while (1) {
    }
}

/** @brief 存储器管理故障：死循环 */
void MemManage_Handler(void)
{
    while (1) {
    }
}

/** @brief 总线故障：死循环 */
void BusFault_Handler(void)
{
    while (1) {
    }
}

/** @brief 用法故障：死循环 */
void UsageFault_Handler(void)
{
    while (1) {
    }
}

/** @brief SVC 调用 */
void SVC_Handler(void)
{
}

/** @brief 调试监视器 */
void DebugMon_Handler(void)
{
}

/** @brief PendSV（Stub 无 RTOS） */
void PendSV_Handler(void)
{
}

/** @brief SysTick：推进 HAL tick */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/** @brief TIM7：HAL 时基中断 */
void TIM7_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim7);
}
