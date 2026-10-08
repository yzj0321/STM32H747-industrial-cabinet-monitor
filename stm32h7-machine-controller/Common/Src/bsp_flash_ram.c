/**
 * @file    bsp_flash_ram.c
 * @brief   Bank1 Flash erase/program implementation running from DTCM
 *
 * Both CM7 application slots are in Bank1. While Bank1 is busy, every
 * instruction in the critical path must come from RAM and interrupts must not
 * vector into Flash. I-Cache and branch-target allocation are also disabled to
 * prevent speculative Bank1 accesses during the operation.
 */
#include "bsp_flash_ram.h"

#include "stm32h7xx_hal.h"

#define BSP_FLASH_RAM_WORD_BYTES \
    (FLASH_NB_32BITWORD_IN_FLASHWORD * sizeof(uint32_t))
#define BSP_FLASH_RAM_TIMEOUT_WRAPS  (6U)
#define BSP_FLASH_RAM_ACTLR_BTAC_BITS \
    (SCnSCB_ACTLR_DISBTACALLOC_Msk | SCnSCB_ACTLR_DISBTACREAD_Msk)
#define BSP_FLASH_RAM_IWDG_SR_BUSY \
    (IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU)

#if !defined(FLASH_BANK2_BASE)
#define FLASH_BANK2_BASE            (0x08100000UL)
#endif

static __RAM_FUNC void bsp_flash_ram_wdg_kick(void)
{
#if defined(IWDG1)
    IWDG1->KR = 0x0000AAAAU;
#endif
}

typedef struct {
    uint32_t last;
    uint32_t wraps;
} bsp_flash_ram_timeout_t;

static __RAM_FUNC void bsp_flash_ram_timeout_arm(bsp_flash_ram_timeout_t *timeout)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    timeout->last = DWT->CYCCNT;
    timeout->wraps = 0U;
}

static __RAM_FUNC int bsp_flash_ram_timeout_expired(bsp_flash_ram_timeout_t *timeout)
{
    uint32_t now = DWT->CYCCNT;

    if (now < timeout->last) {
        timeout->wraps++;
    }
    timeout->last = now;
    return (timeout->wraps >= BSP_FLASH_RAM_TIMEOUT_WRAPS) ? 1 : 0;
}

static __RAM_FUNC void bsp_flash_ram_wait_iwdg_update(void)
{
#if defined(IWDG1)
    bsp_flash_ram_timeout_t timeout;

    bsp_flash_ram_timeout_arm(&timeout);
    while ((IWDG1->SR & BSP_FLASH_RAM_IWDG_SR_BUSY) != 0U) {
        if (bsp_flash_ram_timeout_expired(&timeout) != 0) {
            break;
        }
    }
#endif
}

static __RAM_FUNC void bsp_flash_ram_iwdg_stretch(void)
{
#if defined(IWDG1)
    IWDG1->KR = 0x00005555U;
    bsp_flash_ram_wait_iwdg_update();
    IWDG1->PR = 6U;       /* /256 */
    IWDG1->RLR = 0x0FFFU; /* About 32 seconds at 32-kHz LSI. */
    bsp_flash_ram_wait_iwdg_update();
    bsp_flash_ram_wdg_kick();
#endif
}

static __RAM_FUNC void bsp_flash_ram_iwdg_restore(void)
{
#if defined(IWDG1)
    IWDG1->KR = 0x00005555U;
    bsp_flash_ram_wait_iwdg_update();
    IWDG1->PR = 5U;       /* /128, matches MX_IWDG1_Init. */
    IWDG1->RLR = 0x0FFFU;
    bsp_flash_ram_wait_iwdg_update();
    bsp_flash_ram_wdg_kick();
#endif
}

static __RAM_FUNC void bsp_flash_ram_spec_off(uint32_t *saved_ccr,
                                              uint32_t *saved_actlr)
{
    *saved_actlr = SCnSCB->ACTLR;
    SCnSCB->ACTLR = *saved_actlr | BSP_FLASH_RAM_ACTLR_BTAC_BITS;

    *saved_ccr = SCB->CCR & SCB_CCR_IC_Msk;
    __DSB();
    __ISB();
    if (*saved_ccr != 0U) {
        SCB->CCR &= ~SCB_CCR_IC_Msk;
        SCB->ICIALLU = 0UL;
    }
    __DSB();
    __ISB();
}

static __RAM_FUNC void bsp_flash_ram_spec_restore(uint32_t saved_ccr,
                                                  uint32_t saved_actlr)
{
    __DSB();
    __ISB();
    SCB->ICIALLU = 0UL;
    __DSB();
    __ISB();

    if (saved_ccr != 0U) {
        SCB->CCR |= SCB_CCR_IC_Msk;
        __DSB();
        __ISB();
    }

    SCnSCB->ACTLR = saved_actlr;
    __DSB();
    __ISB();
}

static __RAM_FUNC int bsp_flash_ram_wait_bank1(void)
{
    bsp_flash_ram_timeout_t timeout;
    uint32_t error;

    bsp_flash_ram_timeout_arm(&timeout);
    while ((FLASH->SR1 & FLASH_FLAG_QW_BANK1) != 0U) {
        bsp_flash_ram_wdg_kick();
        if (bsp_flash_ram_timeout_expired(&timeout) != 0) {
            return -1;
        }
    }

    error = FLASH->SR1 & FLASH_FLAG_ALL_ERRORS_BANK1;
    if (error != 0U) {
        FLASH->SR1 = error;
        return -1;
    }

    if ((FLASH->SR1 & FLASH_FLAG_EOP_BANK1) != 0U) {
        FLASH->SR1 = FLASH_FLAG_EOP_BANK1;
    }
    return 0;
}

static __RAM_FUNC int bsp_flash_ram_unlock_bank1(void)
{
    if ((FLASH->CR1 & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR1 = FLASH_KEY1;
        FLASH->KEYR1 = FLASH_KEY2;
        if ((FLASH->CR1 & FLASH_CR_LOCK) != 0U) {
            return -1;
        }
    }
    return 0;
}

static __RAM_FUNC void bsp_flash_ram_lock_bank1(void)
{
    SET_BIT(FLASH->CR1, FLASH_CR_LOCK);
}

static __RAM_FUNC int bsp_flash_ram_erase_sector(uint32_t sector)
{
    if (bsp_flash_ram_wait_bank1() != 0) {
        return -1;
    }

#if defined(FLASH_CR_PSIZE)
    FLASH->CR1 &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB);
    FLASH->CR1 |= (FLASH_CR_SER | FLASH_VOLTAGE_RANGE_3 |
                   (sector << FLASH_CR_SNB_Pos) | FLASH_CR_START);
#else
    FLASH->CR1 &= ~FLASH_CR_SNB;
    FLASH->CR1 |= (FLASH_CR_SER | (sector << FLASH_CR_SNB_Pos) |
                   FLASH_CR_START);
#endif

    if (bsp_flash_ram_wait_bank1() != 0) {
        FLASH->CR1 &= ~(FLASH_CR_SER | FLASH_CR_SNB);
        return -1;
    }

    FLASH->CR1 &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    return 0;
}

static __RAM_FUNC int bsp_flash_ram_program_word(uint32_t address,
                                                 const uint32_t *data)
{
    uint32_t i;

    if (bsp_flash_ram_wait_bank1() != 0) {
        return -1;
    }

    SET_BIT(FLASH->CR1, FLASH_CR_PG);
    __ISB();
    __DSB();

    for (i = 0U; i < FLASH_NB_32BITWORD_IN_FLASHWORD; i++) {
        *(__IO uint32_t *)(address + (i * sizeof(uint32_t))) = data[i];
    }

    __ISB();
    __DSB();
    if (bsp_flash_ram_wait_bank1() != 0) {
        CLEAR_BIT(FLASH->CR1, FLASH_CR_PG);
        return -1;
    }

    CLEAR_BIT(FLASH->CR1, FLASH_CR_PG);
    return 0;
}

static __RAM_FUNC int bsp_flash_ram_wait_bank2(void)
{
    bsp_flash_ram_timeout_t timeout;
    uint32_t error;

    bsp_flash_ram_timeout_arm(&timeout);
    while ((FLASH->SR2 & FLASH_SR_QW) != 0U) {
        bsp_flash_ram_wdg_kick();
        if (bsp_flash_ram_timeout_expired(&timeout) != 0) {
            return -1;
        }
    }

    error = FLASH->SR2 & (FLASH_FLAG_ALL_ERRORS_BANK2 & 0x7FFFFFFFU);
    if (error != 0U) {
        FLASH->CCR2 = error;
        return -1;
    }

    if ((FLASH->SR2 & FLASH_SR_EOP) != 0U) {
        FLASH->CCR2 = FLASH_SR_EOP;
    }
    return 0;
}

static __RAM_FUNC int bsp_flash_ram_unlock_bank2(void)
{
    if ((FLASH->CR2 & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR2 = FLASH_KEY1;
        FLASH->KEYR2 = FLASH_KEY2;
        if ((FLASH->CR2 & FLASH_CR_LOCK) != 0U) {
            return -1;
        }
    }
    return 0;
}

static __RAM_FUNC void bsp_flash_ram_lock_bank2(void)
{
    SET_BIT(FLASH->CR2, FLASH_CR_LOCK);
}

static __RAM_FUNC int bsp_flash_ram_erase_sector_bank2(uint32_t sector)
{
    if (bsp_flash_ram_wait_bank2() != 0) {
        return -1;
    }

#if defined(FLASH_CR_PSIZE)
    FLASH->CR2 &= ~(FLASH_CR_PSIZE | FLASH_CR_SNB);
    FLASH->CR2 |= (FLASH_CR_SER | FLASH_VOLTAGE_RANGE_3 |
                   (sector << FLASH_CR_SNB_Pos) | FLASH_CR_START);
#else
    FLASH->CR2 &= ~FLASH_CR_SNB;
    FLASH->CR2 |= (FLASH_CR_SER | (sector << FLASH_CR_SNB_Pos) | FLASH_CR_START);
#endif

    if (bsp_flash_ram_wait_bank2() != 0) {
        FLASH->CR2 &= ~(FLASH_CR_SER | FLASH_CR_SNB);
        return -1;
    }

    FLASH->CR2 &= ~(FLASH_CR_SER | FLASH_CR_SNB);
    return 0;
}

static __RAM_FUNC int bsp_flash_ram_program_word_bank2(uint32_t address,
                                                      const uint32_t *data)
{
    uint32_t i;

    if (bsp_flash_ram_wait_bank2() != 0) {
        return -1;
    }

    SET_BIT(FLASH->CR2, FLASH_CR_PG);
    __ISB();
    __DSB();

    for (i = 0U; i < FLASH_NB_32BITWORD_IN_FLASHWORD; i++) {
        *(__IO uint32_t *)(address + (i * sizeof(uint32_t))) = data[i];
    }

    __ISB();
    __DSB();
    if (bsp_flash_ram_wait_bank2() != 0) {
        CLEAR_BIT(FLASH->CR2, FLASH_CR_PG);
        return -1;
    }

    CLEAR_BIT(FLASH->CR2, FLASH_CR_PG);
    return 0;
}

int __RAM_FUNC bsp_flash_ram_erase_sectors(uint32_t first_sector,
                                           uint32_t last_sector)
{
    uint32_t sector;
    uint32_t saved_ccr;
    uint32_t saved_actlr;
    uint32_t primask;
    int result = 0;

    if (first_sector > last_sector) {
        return -1;
    }

    bsp_flash_ram_iwdg_stretch();
    bsp_flash_ram_spec_off(&saved_ccr, &saved_actlr);
    if (bsp_flash_ram_unlock_bank1() != 0) {
        bsp_flash_ram_spec_restore(saved_ccr, saved_actlr);
        bsp_flash_ram_iwdg_restore();
        return -1;
    }

    FLASH->SR1 = FLASH_FLAG_ALL_ERRORS_BANK1;
    for (sector = first_sector; sector <= last_sector; sector++) {
        primask = __get_PRIMASK();
        __disable_irq();
        bsp_flash_ram_wdg_kick();
        result = bsp_flash_ram_erase_sector(sector);
        __set_PRIMASK(primask);
        if (result != 0) {
            break;
        }
        bsp_flash_ram_wdg_kick();
    }

    bsp_flash_ram_lock_bank1();
    bsp_flash_ram_spec_restore(saved_ccr, saved_actlr);
    bsp_flash_ram_iwdg_restore();
    bsp_flash_ram_wdg_kick();
    return result;
}

int __RAM_FUNC bsp_flash_ram_erase_bank2_sectors(uint32_t first_sector,
                                                  uint32_t last_sector)
{
    uint32_t sector;
    int result = 0;

    if (first_sector > last_sector) {
        return -1;
    }

    if (first_sector == 0U) {
        return -1;
    }

    bsp_flash_ram_iwdg_stretch();
    if (bsp_flash_ram_unlock_bank2() != 0) {
        bsp_flash_ram_iwdg_restore();
        return -1;
    }

    FLASH->CCR2 = (FLASH_FLAG_ALL_ERRORS_BANK2 & 0x7FFFFFFFU);
    for (sector = first_sector; sector <= last_sector; sector++) {
        bsp_flash_ram_wdg_kick();
        result = bsp_flash_ram_erase_sector_bank2(sector);
        if (result != 0) {
            break;
        }
    }

    bsp_flash_ram_lock_bank2();
    bsp_flash_ram_iwdg_restore();
    bsp_flash_ram_wdg_kick();
    return result;
}

int __RAM_FUNC bsp_flash_ram_program_buffer(uint32_t address,
                                            const uint8_t *data,
                                            uint32_t length)
{
    volatile uint32_t flashword[FLASH_NB_32BITWORD_IN_FLASHWORD];
    uint32_t offset = 0U;
    uint32_t chunk;
    uint32_t i;
    uint32_t saved_ccr;
    uint32_t saved_actlr;
    uint32_t primask;
    int result = 0;
    int is_bank2;

    if ((data == NULL) || (length == 0U) ||
        ((address % BSP_FLASH_RAM_WORD_BYTES) != 0U)) {
        return -1;
    }

    is_bank2 = (address >= FLASH_BANK2_BASE) ? 1 : 0;

    bsp_flash_ram_iwdg_stretch();
    if (is_bank2 != 0) {
        if (bsp_flash_ram_unlock_bank2() != 0) {
            bsp_flash_ram_iwdg_restore();
            return -1;
        }
    } else {
        bsp_flash_ram_spec_off(&saved_ccr, &saved_actlr);
        if (bsp_flash_ram_unlock_bank1() != 0) {
            bsp_flash_ram_spec_restore(saved_ccr, saved_actlr);
            bsp_flash_ram_iwdg_restore();
            return -1;
        }
    }

    while (offset < length) {
        chunk = length - offset;
        if (chunk > BSP_FLASH_RAM_WORD_BYTES) {
            chunk = BSP_FLASH_RAM_WORD_BYTES;
        }

        for (i = 0U; i < FLASH_NB_32BITWORD_IN_FLASHWORD; i++) {
            flashword[i] = 0xFFFFFFFFU;
        }
        for (i = 0U; i < chunk; i++) {
            ((uint8_t *)flashword)[i] = data[offset + i];
        }

        primask = __get_PRIMASK();
        __disable_irq();
        bsp_flash_ram_wdg_kick();
        if (is_bank2 != 0) {
            result = bsp_flash_ram_program_word_bank2(address + offset,
                                                      (const uint32_t *)flashword);
        } else {
            result = bsp_flash_ram_program_word(address + offset,
                                                (const uint32_t *)flashword);
        }
        __set_PRIMASK(primask);
        if (result != 0) {
            break;
        }
        offset += BSP_FLASH_RAM_WORD_BYTES;
    }

    if (is_bank2 != 0) {
        bsp_flash_ram_lock_bank2();
    } else {
        bsp_flash_ram_lock_bank1();
        bsp_flash_ram_spec_restore(saved_ccr, saved_actlr);
    }
    bsp_flash_ram_iwdg_restore();
    bsp_flash_ram_wdg_kick();
    return result;
}
