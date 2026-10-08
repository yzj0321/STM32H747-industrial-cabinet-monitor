/**
 * @file    boot_slot.c
 * @brief   CM7 / CM4 双槽 Flash 分区与可执行校验
 */
#include "boot_slot.h"

#include "stm32h7xx.h"

extern uint32_t g_pfnVectors[];

/**
 * @brief  由地址推断 CM7 槽索引
 */
uint8_t boot_slot_index_from_address(uint32_t address)
{
    if ((address >= CM7_APP_SLOT_B_BASE) &&
        (address < (CM7_APP_SLOT_B_BASE + CM7_APP_SLOT_SIZE))) {
        return CM7_APP_SLOT_B_INDEX;
    }

    return CM7_APP_SLOT_A_INDEX;
}

/**
 * @brief  获取 CM7 槽基址
 */
uint32_t boot_slot_base(uint8_t slot_index)
{
    if (slot_index == CM7_APP_SLOT_B_INDEX) {
        return CM7_APP_SLOT_B_BASE;
    }

    return CM7_APP_SLOT_A_BASE;
}

/**
 * @brief  检测当前运行的 CM7 槽
 */
uint8_t boot_slot_detect_running(void)
{
    return boot_slot_index_from_address((uint32_t)g_pfnVectors);
}

/**
 * @brief  获取 CM7 对端槽索引
 */
uint8_t boot_slot_inactive_index(uint8_t running_index)
{
    return (running_index == CM7_APP_SLOT_A_INDEX) ? CM7_APP_SLOT_B_INDEX
                                                   : CM7_APP_SLOT_A_INDEX;
}

/**
 * @brief  判断 CM7 槽索引是否合法
 */
int boot_slot_is_valid(uint8_t slot_index)
{
    uint32_t base;
    uint32_t stack;
    uint32_t reset;
    uint32_t slot_end;

    if (slot_index >= CM7_APP_SLOT_COUNT) {
        return 0;
    }

    base = boot_slot_base(slot_index);
    slot_end = base + CM7_APP_SLOT_SIZE;
    stack = *(__IO uint32_t *)base;
    reset = *(__IO uint32_t *)(base + 4U);

    {
        uint32_t stack_region = stack & 0xFF000000U;
        if ((stack_region != 0x20000000U) &&
            (stack_region != 0x24000000U) &&
            (stack_region != 0x30000000U)) {
            return 0;
        }
    }

    if ((reset < base) || (reset >= slot_end) || ((reset & 1U) == 0U)) {
        return 0;
    }

    return 1;
}

/**
 * @brief  由地址推断 CM4 槽索引
 */
uint8_t cm4_boot_slot_index_from_address(uint32_t address)
{
    if ((address >= CM4_APP_SLOT_B_BASE) &&
        (address < (CM4_APP_SLOT_B_BASE + CM4_APP_SLOT_SIZE))) {
        return CM4_APP_SLOT_B_INDEX;
    }

    return CM4_APP_SLOT_A_INDEX;
}

/**
 * @brief  获取 CM4 槽基址
 */
uint32_t cm4_boot_slot_base(uint8_t slot_index)
{
    if (slot_index == CM4_APP_SLOT_B_INDEX) {
        return CM4_APP_SLOT_B_BASE;
    }

    return CM4_APP_SLOT_A_BASE;
}

/**
 * @brief  获取 CM4 对端槽索引
 */
uint8_t cm4_boot_slot_inactive_index(uint8_t running_index)
{
    return (running_index == CM4_APP_SLOT_A_INDEX) ? CM4_APP_SLOT_B_INDEX
                                                   : CM4_APP_SLOT_A_INDEX;
}

/**
 * @brief  判断 CM4 槽索引是否合法
 */
int cm4_boot_slot_is_valid(uint8_t slot_index)
{
    uint32_t base;
    uint32_t stack;
    uint32_t reset;
    uint32_t slot_end;

    if (slot_index >= CM4_APP_SLOT_COUNT) {
        return 0;
    }

    base = cm4_boot_slot_base(slot_index);
    slot_end = base + CM4_APP_SLOT_SIZE;
    stack = *(__IO uint32_t *)base;
    reset = *(__IO uint32_t *)(base + 4U);

    {
        uint32_t stack_region = stack & 0xFF000000U;
        if ((stack_region != 0x10000000U) &&
            (stack_region != 0x30000000U) &&
            (stack_region != 0x20000000U)) {
            return 0;
        }
    }

    if ((reset < base) || (reset >= slot_end) || ((reset & 1U) == 0U)) {
        return 0;
    }

    return 1;
}

/**
 * @brief  检测当前运行的 CM4 槽
 */
uint8_t cm4_boot_slot_detect_running(void)
{
#if defined(CORE_CM4)
    return cm4_boot_slot_index_from_address((uint32_t)g_pfnVectors);
#else
    return CM4_APP_SLOT_A_INDEX;
#endif
}
