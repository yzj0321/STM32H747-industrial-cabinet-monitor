/**
 * @file    cm4_boot_handoff.c
 * @brief   CM4 Stub 选槽握手与跳转
 */
#include "cm4_boot_handoff.h"

#include "boot_slot.h"
#include "stm32h7xx.h"

void cm4_boot_handoff_write(uint32_t app_base)
{
    volatile uint32_t *magic = (volatile uint32_t *)CM4_BOOT_HANDOFF_MAGIC_ADDR;
    volatile uint32_t *base = (volatile uint32_t *)CM4_BOOT_HANDOFF_BASE_ADDR;

    *base = app_base;
    __DSB();
    *magic = CM4_BOOT_HANDOFF_MAGIC;
    __DSB();
}

int cm4_boot_handoff_read(uint32_t *app_base)
{
    volatile uint32_t *magic = (volatile uint32_t *)CM4_BOOT_HANDOFF_MAGIC_ADDR;
    volatile uint32_t *base = (volatile uint32_t *)CM4_BOOT_HANDOFF_BASE_ADDR;

    if (app_base == NULL) {
        return 0;
    }
    if (*magic != CM4_BOOT_HANDOFF_MAGIC) {
        return 0;
    }

    *app_base = *base;
    return 1;
}

static int cm4_boot_vector_ok(uint32_t app_base)
{
    uint8_t slot;

    if ((app_base != CM4_APP_SLOT_A_BASE) && (app_base != CM4_APP_SLOT_B_BASE)) {
        return 0;
    }

    slot = cm4_boot_slot_index_from_address(app_base);
    if (cm4_boot_slot_base(slot) != app_base) {
        return 0;
    }

    return (cm4_boot_slot_is_valid(slot) != 0) ? 1 : 0;
}

int cm4_boot_handoff_jump(uint32_t app_base)
{
    uint32_t app_stack;
    uint32_t app_entry;
    void (*app_reset)(void);

    if (cm4_boot_vector_ok(app_base) == 0) {
        return -1;
    }

    app_stack = *(__IO uint32_t *)app_base;
    app_entry = *(__IO uint32_t *)(app_base + 4U);
    app_reset = (void (*)(void))app_entry;

    __disable_irq();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    SCB->VTOR = app_base;
    __set_MSP(app_stack);
    __DSB();
    __ISB();
    app_reset();

    return -1;
}

int cm4_boot_handoff_jump_from_d3(void)
{
    uint32_t app_base = 0U;

    if (cm4_boot_handoff_read(&app_base) == 0) {
        app_base = CM4_APP_SLOT_A_BASE;
    }

    return cm4_boot_handoff_jump(app_base);
}
