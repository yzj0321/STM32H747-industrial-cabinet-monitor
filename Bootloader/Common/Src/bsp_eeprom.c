/**
 * @file    bsp_eeprom.c
 * @brief   BL24C16F I2C EEPROM 驱动实现
 */
#include "bsp_eeprom.h"

#include "i2c.h"
#include "stm32h7xx_hal.h"

#include <stdio.h>

#define BSP_EEPROM_I2C_TIMEOUT_MS   (200U)
#define BSP_EEPROM_WAIT_READY_MS    (200U)
#define BSP_EEPROM_PROBE_RETRIES    (3U)

static bool s_ready;

static uint16_t eeprom_dev_addr(uint16_t addr)
{
    return (uint16_t)(BSP_EEPROM_DEV_ADDR_BASE |
                      (((addr >> 8U) & 0x07U) << 1U));
}

static void eeprom_log_i2c(const char *stage, uint16_t addr, uint16_t chunk,
                           HAL_StatusTypeDef st)
{
    printf("[EEPROM] %s addr=0x%04X dev=0x%02X word=0x%02X chunk=%u hal=%d "
           "i2c_st=%lu err=0x%08lX tick=%lu\r\n",
           stage,
           (unsigned int)addr,
           (unsigned int)eeprom_dev_addr(addr),
           (unsigned int)(addr & 0xFFU),
           (unsigned int)chunk,
           (int)st,
           (unsigned long)hi2c4.State,
           (unsigned long)hi2c4.ErrorCode,
           (unsigned long)HAL_GetTick());
}

static bool eeprom_probe(void)
{
    return (HAL_I2C_IsDeviceReady(&hi2c4,
                                  BSP_EEPROM_DEV_ADDR_BASE,
                                  BSP_EEPROM_PROBE_RETRIES,
                                  BSP_EEPROM_I2C_TIMEOUT_MS) == HAL_OK);
}

static void eeprom_bus_reset(void)
{
    (void)HAL_I2C_DeInit(&hi2c4);
    MX_I2C4_Init();
    s_ready = false;
}

static bool eeprom_wait_ready(uint16_t addr)
{
    uint32_t start = HAL_GetTick();

    while (HAL_I2C_IsDeviceReady(&hi2c4, eeprom_dev_addr(addr), 1U, 1U) != HAL_OK) {
        if ((HAL_GetTick() - start) > BSP_EEPROM_WAIT_READY_MS) {
            eeprom_log_i2c("wait_ready timeout", addr, 0U, HAL_TIMEOUT);
            return false;
        }
    }

    return true;
}

static bool eeprom_do_read(uint16_t addr, uint8_t *data, size_t len)
{
    while (len > 0U) {
        uint16_t block_remain = (uint16_t)(BSP_EEPROM_BLOCK_SIZE -
                                           (addr & (BSP_EEPROM_BLOCK_SIZE - 1U)));
        uint16_t chunk = (len < block_remain) ? (uint16_t)len : block_remain;
        HAL_StatusTypeDef st;

        st = HAL_I2C_Mem_Read(&hi2c4,
                              eeprom_dev_addr(addr),
                              (uint16_t)(addr & 0xFFU),
                              I2C_MEMADD_SIZE_8BIT,
                              data,
                              chunk,
                              BSP_EEPROM_I2C_TIMEOUT_MS);
        if (st != HAL_OK) {
            eeprom_log_i2c("read fail", addr, chunk, st);
            return false;
        }

        addr = (uint16_t)(addr + chunk);
        data += chunk;
        len -= chunk;
    }

    return true;
}

static bool eeprom_do_write(uint16_t addr, const uint8_t *data, size_t len)
{
    while (len > 0U) {
        uint16_t page_offset = (uint16_t)(addr & (BSP_EEPROM_PAGE_SIZE - 1U));
        uint16_t page_remain = (uint16_t)(BSP_EEPROM_PAGE_SIZE - page_offset);
        uint16_t chunk = (len < page_remain) ? (uint16_t)len : page_remain;
        HAL_StatusTypeDef st;

        st = HAL_I2C_Mem_Write(&hi2c4,
                               eeprom_dev_addr(addr),
                               (uint16_t)(addr & 0xFFU),
                               I2C_MEMADD_SIZE_8BIT,
                               (uint8_t *)data,
                               chunk,
                               BSP_EEPROM_I2C_TIMEOUT_MS);
        if (st != HAL_OK) {
            eeprom_log_i2c("write fail", addr, chunk, st);
            return false;
        }

        if (!eeprom_wait_ready(addr)) {
            return false;
        }

        addr = (uint16_t)(addr + chunk);
        data += chunk;
        len -= chunk;
    }

    return true;
}

bool BSP_Eeprom_Recover(void)
{
    eeprom_bus_reset();

    if (!eeprom_probe()) {
        eeprom_bus_reset();
        if (!eeprom_probe()) {
            printf("[EEPROM] recover failed i2c_st=%lu err=0x%08lX tick=%lu\r\n",
                   (unsigned long)hi2c4.State,
                   (unsigned long)hi2c4.ErrorCode,
                   (unsigned long)HAL_GetTick());
            s_ready = false;
            return false;
        }
    }

    s_ready = true;
    printf("[EEPROM] recover OK tick=%lu\r\n", (unsigned long)HAL_GetTick());
    return true;
}

bool BSP_Eeprom_Init(void)
{
    if (!eeprom_probe()) {
        printf("[EEPROM] init probe failed dev=0x%02X i2c_st=%lu err=0x%08lX tick=%lu\r\n",
               (unsigned int)BSP_EEPROM_DEV_ADDR_BASE,
               (unsigned long)hi2c4.State,
               (unsigned long)hi2c4.ErrorCode,
               (unsigned long)HAL_GetTick());
        s_ready = false;
        return false;
    }

    s_ready = true;
    return true;
}

bool BSP_Eeprom_IsReady(void)
{
    return s_ready;
}

bool BSP_Eeprom_Read(uint16_t addr, uint8_t *data, size_t len)
{
    if ((data == NULL) || (len == 0U) ||
        ((uint32_t)addr + len) > BSP_EEPROM_SIZE) {
        return false;
    }

    if (!s_ready) {
        return false;
    }

    if (eeprom_do_read(addr, data, len)) {
        return true;
    }

    if (!BSP_Eeprom_Recover()) {
        return false;
    }

    return eeprom_do_read(addr, data, len);
}

bool BSP_Eeprom_Write(uint16_t addr, const uint8_t *data, size_t len)
{
    if ((data == NULL) || (len == 0U) ||
        ((uint32_t)addr + len) > BSP_EEPROM_SIZE) {
        return false;
    }

    if (!s_ready) {
        return false;
    }

    if (eeprom_do_write(addr, data, len)) {
        return true;
    }

    if (!BSP_Eeprom_Recover()) {
        return false;
    }

    return eeprom_do_write(addr, data, len);
}
