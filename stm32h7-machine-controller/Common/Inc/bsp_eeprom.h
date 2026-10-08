/**
 * @file    bsp_eeprom.h
 * @brief   BL24C16F I2C EEPROM 驱动（I2C4 / PD12 SCL / PD13 SDA）
 *
 * 芯片：16Kbit（2048 字节），页大小 16B。
 * 寻址：高 3 位 A10~A8 编入 I2C 设备地址，低 8 位为字地址（8-bit MemAdd）。
 */
#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** 基础 8 位写地址（A10=A9=A8=0） */
#define BSP_EEPROM_DEV_ADDR_BASE    (0xA0U)
/** 页大小（字节）——单次页写不可跨页 */
#define BSP_EEPROM_PAGE_SIZE        (16U)
/** 地址块大小（字节）——跨块需切换设备地址 */
#define BSP_EEPROM_BLOCK_SIZE       (256U)
/** 芯片容量 */
#define BSP_EEPROM_SIZE             (2048U)

/**
 * @brief  初始化 I2C EEPROM 并探测芯片
 * @retval true 成功，false 失败
 */
bool BSP_Eeprom_Init(void);

/**
 * @brief  查询 EEPROM 是否已探测成功
 * @retval true 就绪，false 未就绪
 */
bool BSP_Eeprom_IsReady(void);

/**
 * @brief  长 Flash 操作后恢复 I2C4 并重探测 EEPROM
 * @retval true 恢复成功，false 失败
 */
bool BSP_Eeprom_Recover(void);

/**
 * @brief  从 EEPROM 读取数据（可跨页/跨块）
 * @param  addr  起始地址（0..2047）
 * @param  data  输出缓冲
 * @param  len   字节数
 * @retval true 成功，false 失败
 */
bool BSP_Eeprom_Read(uint16_t addr, uint8_t *data, size_t len);

/**
 * @brief  向 EEPROM 写入数据（按页拆分，等待写周期）
 * @param  addr  起始地址
 * @param  data  输入缓冲
 * @param  len   字节数
 * @retval true 成功，false 失败
 */
bool BSP_Eeprom_Write(uint16_t addr, const uint8_t *data, size_t len);

#endif /* BSP_EEPROM_H */
