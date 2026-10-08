/**
 * @file    cm4_boot_handoff.c
 * @brief   CM4 Stub 选槽握手与跳转
 */
#include "cm4_boot_handoff.h"

#include "boot_slot.h"
#include "stm32h7xx.h"

/**
 * @brief  写入 CM4 启动握手基址到 D3
 */
void cm4_boot_handoff_write(uint32_t app_base)
{
    volatile uint32_t *magic = (volatile uint32_t *)CM4_BOOT_HANDOFF_MAGIC_ADDR;
    volatile uint32_t *base = (volatile uint32_t *)CM4_BOOT_HANDOFF_BASE_ADDR;

    *base = app_base;
    __DSB();
    *magic = CM4_BOOT_HANDOFF_MAGIC;
    __DSB();
}

/**
 * @brief  从 D3 读取 CM4 启动基址
 */
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

/**
 * @brief  校验 CM4 应用向量表合法性
 */
static int cm4_boot_vector_ok(uint32_t app_base)
{
    uint32_t stack;
    uint32_t reset;
    uint32_t slot_end;
    uint8_t slot;

    if ((app_base != CM4_APP_SLOT_A_BASE) && (app_base != CM4_APP_SLOT_B_BASE)) {
        return 0;
    }

    slot = cm4_boot_slot_index_from_address(app_base);
    if (cm4_boot_slot_base(slot) != app_base) {
        return 0;
    }
    if (cm4_boot_slot_is_valid(slot) == 0) {
        return 0;
    }

    slot_end = app_base + CM4_APP_SLOT_SIZE;
    stack = *(__IO uint32_t *)app_base;
    reset = *(__IO uint32_t *)(app_base + 4U);
    (void)stack;
    (void)slot_end;
    (void)reset;
    return 1;
}

/**
 * @brief  跳转到指定 CM4 应用
 */
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

/**
 * @brief  从 D3 握手区读取并跳转
 */
int cm4_boot_handoff_jump_from_d3(void)
{
    uint32_t app_base = 0U;

    if (cm4_boot_handoff_read(&app_base) == 0) {
        app_base = CM4_APP_SLOT_A_BASE;
    }

    return cm4_boot_handoff_jump(app_base);
}

/**
 * @brief  清除 CM4 alive 标志
 */
void cm4_boot_alive_clear(void)
{
    volatile uint32_t *magic = (volatile uint32_t *)CM4_BOOT_ALIVE_MAGIC_ADDR;
    volatile uint32_t *slot = (volatile uint32_t *)CM4_BOOT_ALIVE_SLOT_ADDR;

    *slot = 0U;
    __DSB();
    *magic = 0U;
    __DSB();
}

void cm4_ipc_diag_set(uint32_t stage)
{
    volatile uint32_t *diag = (volatile uint32_t *)CM4_IPC_DIAG_ADDR;
    *diag = stage;
    __DSB();
}

uint32_t cm4_ipc_diag_get(void)
{
    volatile uint32_t *diag = (volatile uint32_t *)CM4_IPC_DIAG_ADDR;
    return *diag;
}

#if defined(CORE_CM4)
/**
 * @brief  CM4 上报 alive 标志
 */
void cm4_boot_alive_signal(uint8_t slot_index)
{
    volatile uint32_t *magic = (volatile uint32_t *)CM4_BOOT_ALIVE_MAGIC_ADDR;
    volatile uint32_t *slot = (volatile uint32_t *)CM4_BOOT_ALIVE_SLOT_ADDR;

    *slot = (uint32_t)slot_index;
    __DSB();
    *magic = CM4_BOOT_ALIVE_MAGIC;
    __DSB();
}
#endif

#if defined(CORE_CM7)
/**
 * @brief  CM7 消费 CM4 alive 标志
 */
int cm4_boot_alive_consume(uint8_t *slot_index)
{
    volatile uint32_t *magic = (volatile uint32_t *)CM4_BOOT_ALIVE_MAGIC_ADDR;
    volatile uint32_t *slot = (volatile uint32_t *)CM4_BOOT_ALIVE_SLOT_ADDR;

    if ((slot_index == NULL) || (*magic != CM4_BOOT_ALIVE_MAGIC)) {
        return 0;
    }

    *slot_index = (uint8_t)(*slot & 0xFFU);
    cm4_boot_alive_clear();
    return 1;
}
#endif
