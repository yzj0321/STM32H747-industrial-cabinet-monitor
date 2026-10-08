/**
 * @file    bsp_ota_coop.c
 * @brief   OTA 期间挂起/恢复其他 RTOS 任务实现
 *
 * OTA 擦写 Flash 与 W5500 TCP 收发均为耗时操作。若 LVGL、Modbus 等任务
 * 同时运行，会抢占 CPU 导致 RX 缓冲积压丢包。
 * 因此在 BEGIN_SESSION 后 suspend 已注册任务，VERIFY/ABORT 后 resume。
 */
#include "bsp_ota_coop.h"

#include "bsp_log.h"

#define OTA_COOP_TAG        "OTA-COOP"
#define OTA_COOP_MAX        10U

/** 待挂起的 peer 任务句柄表（由 bsp_ota_coop_register 填充） */
static osThreadId_t s_peers[OTA_COOP_MAX];
static uint8_t s_peer_count;
/** 当前是否已执行 suspend（防止重复 suspend） */
static uint8_t s_suspended;

/**
 * @brief  注册需在 OTA 期间挂起的 RTOS 线程
 * @param  thread_id 线程句柄
 */
void bsp_ota_coop_register(osThreadId_t thread_id)
{
    uint8_t i;

    if (thread_id == NULL) {
        return;
    }

    /* 去重：同一任务多次 register 只保留一条 */
    for (i = 0U; i < s_peer_count; i++) {
        if (s_peers[i] == thread_id) {
            return;
        }
    }

    if (s_peer_count >= OTA_COOP_MAX) {
        LOG_W(OTA_COOP_TAG, "peer table full, skip register");
        return;
    }

    s_peers[s_peer_count] = thread_id;
    s_peer_count++;
}

/**
 * @brief  BEGIN_SESSION 后挂起已注册线程，避免抢占 UART/CPU
 */
void bsp_ota_coop_suspend_peers(void)
{
    osThreadId_t self = osThreadGetId();
    uint8_t i;

    if (s_suspended != 0U) {
        return;
    }

    for (i = 0U; i < s_peer_count; i++) {
        if ((s_peers[i] != NULL) && (s_peers[i] != self)) {
            /* 不挂起 OTA 任务自身，避免 poll 无法执行 */
            (void)osThreadSuspend(s_peers[i]);
        }
    }

    s_suspended = 1U;
    LOG_I(OTA_COOP_TAG, "suspended %u peer task(s) for OTA", (unsigned int)s_peer_count);
}

/**
 * @brief  VERIFY 完成或 ABORT 后恢复已挂起线程
 */
void bsp_ota_coop_resume_peers(void)
{
    uint8_t i;

    if (s_suspended == 0U) {
        return;
    }

    for (i = 0U; i < s_peer_count; i++) {
        if (s_peers[i] != NULL) {
            (void)osThreadResume(s_peers[i]);
        }
    }

    s_suspended = 0U;
    LOG_I(OTA_COOP_TAG, "resumed peer task(s)");
}
