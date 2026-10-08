/**
 * @file    bsp_ota_manager.h
 * @brief   OTA 会话状态机与 SOIP 命令调度接口
 */
#ifndef BSP_OTA_MANAGER_H
#define BSP_OTA_MANAGER_H

#include <stdint.h>

/**
 * @brief  轮询传输层并解析、分发 SOIP 帧
 */
void bsp_ota_manager_poll(void);

/**
 * @brief  查询 OTA 会话是否处于活动状态
 * @retval 1 活动（BEGIN_SESSION 至 VERIFY/ABORT 之间）
 * @retval 0 空闲
 */
uint8_t bsp_ota_manager_is_active(void);

/**
 * @brief  查询是否正在接收固件数据或待发 DATA 应答
 * @retval 1 正在接收或 ACK 待发送
 * @retval 0 否
 */
uint8_t bsp_ota_manager_is_receiving(void);

#endif /* BSP_OTA_MANAGER_H */
