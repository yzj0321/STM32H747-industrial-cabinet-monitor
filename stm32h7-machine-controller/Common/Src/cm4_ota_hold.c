/**
 * @file    cm4_ota_hold.c
 * @brief   CM4 OTA 停核握手实现（CM7 / CM4 各编进本文件对应分支）
 */
#include "cm4_ota_hold.h"

#include "stm32h7xx_hal.h"

#if defined(CORE_CM7)
#ifndef CM4_OTA_HOLD_HSEM_ID
#define CM4_OTA_HOLD_HSEM_ID  (0U)
#endif
#endif

static volatile uint32_t *const s_req =
    (volatile uint32_t *)CM4_OTA_HOLD_REQ_ADDR;
static volatile uint32_t *const s_ack =
    (volatile uint32_t *)CM4_OTA_HOLD_ACK_ADDR;

#if defined(CORE_CM7)
/**
 * @brief  请求 CM4 进入 OTA hold
 */
int cm4_ota_hold_request(uint32_t timeout_ms)
{
    uint32_t t0;

    *s_ack = 0U;
    *s_req = CM4_OTA_HOLD_REQ_MAGIC;
    __DSB();
    __DMB();

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (*s_ack == CM4_OTA_HOLD_ACK_MAGIC) {
            HAL_Delay(20);
            return 0;
        }
        HAL_Delay(1);
    }

    return -1;
}

/**
 * @brief  释放 CM4 OTA hold
 */
void cm4_ota_hold_release(void)
{
    *s_req = 0U;
    __DSB();
    __DMB();

    __HAL_RCC_HSEM_CLK_ENABLE();
    if (HAL_HSEM_FastTake(CM4_OTA_HOLD_HSEM_ID) != HAL_OK) {
        HAL_HSEM_Release(CM4_OTA_HOLD_HSEM_ID, 0);
        (void)HAL_HSEM_FastTake(CM4_OTA_HOLD_HSEM_ID);
    }
    HAL_HSEM_Release(CM4_OTA_HOLD_HSEM_ID, 0);

    *s_ack = 0U;
    __DSB();
}
#endif /* CORE_CM7 */

#if defined(CORE_CM4)
/**
 * @brief  CM4 侧处理 OTA hold 请求
 */
int cm4_ota_hold_service(void)
{
    if (*s_req != CM4_OTA_HOLD_REQ_MAGIC) {
        return 0;
    }

    *s_ack = CM4_OTA_HOLD_ACK_MAGIC;
    __DSB();
    __DMB();

    HAL_PWREx_ClearPendingEvent();
    HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);

    *s_ack = 0U;
    __DSB();
    return 1;
}
#endif /* CORE_CM4 */
