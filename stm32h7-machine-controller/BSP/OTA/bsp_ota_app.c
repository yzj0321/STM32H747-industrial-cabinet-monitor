/**
 * @file    bsp_ota_app.c
 * @brief   OTA 应用层：初始化、RTOS 任务与 trial boot 确认
 */
#include "bsp_ota_app.h"

#include "bsp_ota_conf.h"
#include "bsp_log.h"
#include "bsp_w5500_tcp.h"
#include "bsp_ota_manager.h"
#include "bsp_ota_transport.h"
#include "bsp_watchdog.h"
#include "boot_eeprom.h"
#include "boot_slot.h"
#include "cm4_boot_handoff.h"
#include "cmsis_os.h"

#define OTA_APP_TAG                 "OTA-APP"
/** 空闲时 wait_rx 超时，用于喂狗；有数据时由信号量立即唤醒 */
#define OTA_IDLE_WAIT_RX_MS         100U
#define OTA_ACTIVE_WAIT_RX_MS       20U
/** DATA 停等阶段：每轮少量 poll 即可（一包一 ACK，无需狂转） */
#define OTA_RECEIVING_SPIN          8U

static void OtaTask(void *argument);

/**
 * @brief  初始化 boot meta 与 OTA 传输层
 */
void BSP_Ota_Init(void)
{
    bsp_ota_transport_init();
}

/**
 * @brief  创建 OTA RTOS 任务并初始化 TCP 接收通知
 */
void BSP_Ota_Start(void)
{
    const osThreadAttr_t attr = {
        .name = "ota",
        .stack_size = BSP_OTA_TASK_STACK_WORDS * 4U,
        .priority = osPriorityRealtime,
    };

    bsp_w5500_tcp_rx_notify_init();

    if (osThreadNew(OtaTask, NULL, &attr) == NULL) {
        LOG_E(OTA_APP_TAG, "osThreadNew(ota) failed");
    }
}

/**
 * @brief  查询 OTA 是否处于活动会话
 * @retval 1 活动
 * @retval 0 空闲
 */
uint8_t BSP_Ota_IsActive(void)
{
    return bsp_ota_manager_is_active();
}

/**
 * @brief  上电时处理 pending 槽位的 trial boot 确认
 */
void BSP_Ota_OnBoot(void)
{
    boot_eeprom_meta_t meta;
    uint8_t running;

    if (boot_eeprom_load(&meta) != 0) {
        return;
    }

    if ((meta.flags & BOOT_EE_FLAG_PENDING) == 0U) {
        return;
    }

    if (meta.boot_trial == 0U) {
        return;
    }

    running = boot_slot_detect_running();
    if (running != meta.pending_slot) {
        LOG_W(OTA_APP_TAG, "pending slot %u but running %u, skip commit",
              (unsigned int)meta.pending_slot, (unsigned int)running);
        return;
    }

    LOG_I(OTA_APP_TAG, "trial boot slot %u remain=%u",
          (unsigned int)meta.pending_slot, (unsigned int)meta.boot_trial);
    boot_eeprom_commit_pending(&meta);
    LOG_I(OTA_APP_TAG, "boot confirm slot %u (EEPROM)", (unsigned int)meta.active_slot);
}

/**
 * @brief  轮询并确认 CM4 trial boot alive
 */
void BSP_Ota_PollCm4BootAlive(void)
{
    uint8_t cm4_slot = 0U;
    boot_eeprom_meta_t ee;

    if (cm4_boot_alive_consume(&cm4_slot) == 0) {
        return;
    }

    if (boot_eeprom_load(&ee) != 0) {
        return;
    }

    if (((ee.cm4_flags & BOOT_EE_FLAG_CM4_PENDING) == 0U) ||
        (ee.cm4_pending_slot != cm4_slot)) {
        return;
    }

    boot_eeprom_commit_cm4_pending(&ee);
    LOG_I(OTA_APP_TAG, "CM4 boot confirm slot %u (EEPROM)", (unsigned int)ee.cm4_active_slot);
}

/**
 * @brief  OTA 主任务：轮询 SOIP 帧并喂狗
 * @param  argument RTOS 任务参数（未使用）
 */
static void OtaTask(void *argument)
{
    (void)argument;

    LOG_I(OTA_APP_TAG, "task start SOIP/Ethernet (W5500)");

    for (;;) {
        if (bsp_ota_transport_ready()) {
            if (bsp_ota_manager_is_receiving() != 0U) {
                /* DATA 阶段：密集 poll，尽快把 +IPD 搬进 SOIP 解析器，防止 pending 缓冲溢出 */
                uint32_t spin;

                for (spin = 0U; spin < OTA_RECEIVING_SPIN; spin++) {
                    bsp_ota_manager_poll();
                }
                osDelay(1);
            } else {
                /* 非 DATA 阶段：单次 poll 后阻塞等 RX 信号量，降低 CPU 占用 */
                bsp_ota_manager_poll();
                if (bsp_ota_manager_is_active() != 0U) {
                    /* 会话活动中缩短 wait 超时，提高 ERASE/VERIFY 等命令响应速度 */
                    (void)bsp_ota_transport_wait_rx(OTA_ACTIVE_WAIT_RX_MS);
                } else {
                    (void)bsp_ota_transport_wait_rx(OTA_IDLE_WAIT_RX_MS);
                }
            }
        } else {
            /* 以太网/TCP 未就绪，降低轮询频率 */
            osDelay(200);
        }
        BSP_Watchdog_Kick(BSP_WDG_DEFAULT);
    }
}
