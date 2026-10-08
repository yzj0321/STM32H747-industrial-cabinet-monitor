/**
 * @file    bsp_modbus_coil_nv.h
 * @brief   Modbus 线圈非易失性存储（BL24C16F EEPROM 用户区双备份）
 */
#ifndef BSP_MODBUS_COIL_NV_H
#define BSP_MODBUS_COIL_NV_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  查询线圈 NV 存储介质（EEPROM）是否就绪
 * @retval true 就绪，false 未就绪
 */
bool BSP_Modbus_CoilNv_MediaReady(void);

/**
 * @brief  初始化线圈 NV 模块
 * @retval true 成功，false 失败
 */
bool BSP_Modbus_CoilNv_Init(void);

/**
 * @brief  从双备份区加载线圈状态
 * @param  coils  输出线圈数组
 * @param  count  线圈数量
 * @retval true 成功，false 失败
 */
bool BSP_Modbus_CoilNv_Load(uint8_t *coils, uint16_t count);

/**
 * @brief  保存线圈状态到双备份区
 * @param  coils  输入线圈数组
 * @param  count  线圈数量
 * @retval true 成功，false 失败
 */
bool BSP_Modbus_CoilNv_Save(const uint8_t *coils, uint16_t count);

#endif /* BSP_MODBUS_COIL_NV_H */
