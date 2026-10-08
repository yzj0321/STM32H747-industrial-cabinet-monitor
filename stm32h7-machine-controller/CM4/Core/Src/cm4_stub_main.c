/**
 * @file    cm4_stub_main.c
 * @brief   CM4 Boot Stub @ 0x08100000：读 D3 handoff 后跳转到 Slot A/B 应用
 */
#include "main.h"
#include "cm4_boot_handoff.h"
#include "boot_slot.h"

/**
 * @brief  Stub 入口：优先按 D3 握手跳转，失败则尝试 Slot A/B
 */
int main(void)
{
    HAL_Init();

    if (cm4_boot_handoff_jump_from_d3() != 0) {
        if (cm4_boot_slot_is_valid(CM4_APP_SLOT_A_INDEX) != 0) {
            (void)cm4_boot_handoff_jump(CM4_APP_SLOT_A_BASE);
        }
        if (cm4_boot_slot_is_valid(CM4_APP_SLOT_B_INDEX) != 0) {
            (void)cm4_boot_handoff_jump(CM4_APP_SLOT_B_BASE);
        }
    }

    for (;;) {
    }
}

/**
 * @brief  致命错误停机
 */
void Error_Handler(void)
{
    __disable_irq();
    for (;;) {
    }
}

/**
 * @brief  TIM7 周期回调：推进 HAL tick
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM7) {
        HAL_IncTick();
    }
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  断言失败处理（空实现）
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
