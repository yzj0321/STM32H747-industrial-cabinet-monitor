/**
 * @file    cm4_ota_hold.h
 * @brief   CM4 OTA 前停核握手（D3 SRAM mailbox）
 */
#ifndef CM4_OTA_HOLD_H
#define CM4_OTA_HOLD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CM4_OTA_HOLD_REQ_ADDR   (0x3800F018UL)
#define CM4_OTA_HOLD_ACK_ADDR   (0x3800F01CUL)

#define CM4_OTA_HOLD_REQ_MAGIC  (0x4F544148UL) /* 'OTAH' */
#define CM4_OTA_HOLD_ACK_MAGIC  (0x4F54414BUL) /* 'OTAK' */

/**
 * @brief  CM7 请求 CM4 进入 OTA hold，并等待 ACK
 * @param  timeout_ms  超时（毫秒）
 * @retval 0 成功，非 0 超时/失败
 */
int cm4_ota_hold_request(uint32_t timeout_ms);

/**
 * @brief  CM7 释放 CM4 OTA hold 请求
 */
void cm4_ota_hold_release(void);

/**
 * @brief  CM4 侧轮询：若收到 hold 请求则应答并停机等待
 * @retval 非 0 表示已进入 hold 路径
 */
int cm4_ota_hold_service(void);

#ifdef __cplusplus
}
#endif

#endif /* CM4_OTA_HOLD_H */
