/**
 * @file    bsp_eeprom.h
 * @brief   BL24C16F I2C EEPROM 驱动（I2C4 / PD12 SCL / PD13 SDA）
 */
#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_EEPROM_DEV_ADDR_BASE    (0xA0U)
#define BSP_EEPROM_PAGE_SIZE        (16U)
#define BSP_EEPROM_BLOCK_SIZE       (256U)
#define BSP_EEPROM_SIZE             (2048U)

bool BSP_Eeprom_Init(void);
bool BSP_Eeprom_IsReady(void);
bool BSP_Eeprom_Recover(void);
bool BSP_Eeprom_Read(uint16_t addr, uint8_t *data, size_t len);
bool BSP_Eeprom_Write(uint16_t addr, const uint8_t *data, size_t len);

#endif /* BSP_EEPROM_H */
