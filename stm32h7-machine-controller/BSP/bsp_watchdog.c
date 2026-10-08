/**
 * @file bsp_watchdog.c
 * @brief 独立看门狗（IWDG）任务健康监控与喂狗
 */
#include "bsp_watchdog.h"

#include "iwdg.h"
#include "bsp_log.h"
#include "cmsis_os.h"
#include "main.h"

#define WDG_TAG                 "WDG"
#define WDG_FEED_MS             2000U
#define WDG_BOOT_GRACE_MS       45000U
#define WDG_TASK_STACK_WORDS    384U
#define WDG_STATUS_LOG_INTERVAL 5U

typedef struct {
    volatile uint32_t counter;
    uint32_t last_counter;
    uint32_t last_progress_ms;
    uint8_t ever_kicked;
    uint8_t first_kick_logged;
} bsp_wdg_slot_t;

static bsp_wdg_slot_t s_slots[BSP_WDG_TASK_COUNT];

static const uint32_t s_slot_timeout_ms[BSP_WDG_TASK_COUNT] = {
    [BSP_WDG_LVGL]    = 5000U,
    [BSP_WDG_DEFAULT] = 8000U,
    [BSP_WDG_MODBUS]  = 10000U,
    [BSP_WDG_W5500] = 60000U,
};

static const char *const s_slot_names[BSP_WDG_TASK_COUNT] = {
    "lvgl",
    "default",
    "modbus",
    "w5500",
};

static uint32_t s_grace_end_ms;
static uint32_t s_feed_count;
static volatile uint8_t s_long_op_active;

/**
 * @brief  看门狗喂狗任务入口
 * @param  argument 线程参数（未使用）
 */
static void WatchdogTask(void *argument);

/**
 * @brief  打印各任务槽位健康状态
 * @param  now_ms 当前系统 tick（毫秒）
 * @param  reason 日志原因描述
 */
static void WatchdogLogSlotStatus(uint32_t now_ms, const char *reason);

/**
 * @brief  计算任务槽位自上次进展以来的停滞毫秒数
 * @param  slot   任务槽位
 * @param  now_ms 当前系统 tick（毫秒）
 * @retval 停滞毫秒数；未喂过狗时返回 0
 */
static uint32_t WatchdogSlotStaleMs(const bsp_wdg_slot_t *slot, uint32_t now_ms);

/**
 * @brief  记录并打印 MCU 复位原因
 * @retval 无
 */
void BSP_Watchdog_LogResetCause(void)
{
    uint32_t rsr = RCC->RSR;

    if ((rsr & RCC_RSR_IWDG1RSTF) != 0U) {
        LOG_W(WDG_TAG, "reset cause: IWDG1 (independent watchdog)");
    } else if ((rsr & RCC_RSR_WWDG1RSTF) != 0U) {
        LOG_W(WDG_TAG, "reset cause: WWDG1");
    } else if ((rsr & RCC_RSR_PORRSTF) != 0U) {
        LOG_I(WDG_TAG, "reset cause: POR/PDR");
    } else if ((rsr & RCC_RSR_PINRSTF) != 0U) {
        LOG_I(WDG_TAG, "reset cause: NRST pin");
    } else if ((rsr & RCC_RSR_SFT1RSTF) != 0U) {
        LOG_I(WDG_TAG, "reset cause: CM7 software reset");
    } else if ((rsr & RCC_RSR_SFT2RSTF) != 0U) {
        LOG_I(WDG_TAG, "reset cause: CM4 software reset");
    } else {
        LOG_I(WDG_TAG, "reset cause: other/unknown (RSR=0x%08lX)",
              (unsigned long)rsr);
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/**
 * @brief  标记 W5500 等长时间阻塞网络操作进行中
 * @param  active  非 0 表示长操作进行中
 * @retval 无
 */
void BSP_Watchdog_SetLongOp(uint8_t active)
{
    s_long_op_active = (active != 0U) ? 1U : 0U;
}

/**
 * @brief  任务喂狗（递增心跳计数）
 * @param  task  任务槽位编号
 * @retval 无
 */
void BSP_Watchdog_Kick(bsp_wdg_task_t task)
{
    if (task >= BSP_WDG_TASK_COUNT) {
        return;
    }

    if (s_slots[task].ever_kicked == 0U) {
        s_slots[task].ever_kicked = 1U;
        s_slots[task].last_progress_ms = osKernelGetTickCount();

        if (s_slots[task].first_kick_logged == 0U) {
            s_slots[task].first_kick_logged = 1U;
            LOG_I(WDG_TAG, "task '%s' first kick (tick=%lu)",
                  s_slot_names[task],
                  (unsigned long)s_slots[task].last_progress_ms);
        }
    }

    s_slots[task].counter++;  /* 看门狗任务通过 counter 变化判断任务是否卡死 */
}

/**
 * @brief  计算任务槽位自上次进展以来的停滞毫秒数
 * @param  slot     任务槽位
 * @param  now_ms   当前系统 tick（ms）
 * @retval 停滞毫秒数；未喂过狗时返回 0
 */
static uint32_t WatchdogSlotStaleMs(const bsp_wdg_slot_t *slot, uint32_t now_ms)
{
    if (slot->ever_kicked == 0U) {
        return 0U;
    }

    return now_ms - slot->last_progress_ms;
}

/**
 * @brief  打印各任务槽位健康状态
 * @param  now_ms  当前系统 tick（ms）
 * @param  reason  日志原因描述字符串
 * @retval 无
 */
static void WatchdogLogSlotStatus(uint32_t now_ms, const char *reason)
{
    LOG_I(WDG_TAG, "status [%s] tick=%lu grace=%s feed#=%lu",
          reason,
          (unsigned long)now_ms,
          (now_ms < s_grace_end_ms) ? "yes" : "no",
          (unsigned long)s_feed_count);

    for (uint32_t i = 0U; i < BSP_WDG_TASK_COUNT; i++) {
        const bsp_wdg_slot_t *slot = &s_slots[i];

        if (slot->ever_kicked == 0U) {
            LOG_I(WDG_TAG, "  %-7s: not started", s_slot_names[i]);
        } else {
            LOG_I(WDG_TAG, "  %-7s: cnt=%lu stale=%lu ms (limit %lu)",
                  s_slot_names[i],
                  (unsigned long)slot->counter,
                  (unsigned long)WatchdogSlotStaleMs(slot, now_ms),
                  (unsigned long)s_slot_timeout_ms[i]);
        }
    }
}

/**
 * @brief  检查各任务心跳计数是否在超时阈值内有进展
 * @param  now_ms  当前系统 tick（毫秒）
 * @retval 1 全部健康，0 存在超时任务
 */
static int WatchdogTasksHealthy(uint32_t now_ms)
{
    int healthy = 1;
    uint32_t start = 0U;
    uint32_t end = BSP_WDG_TASK_COUNT;

    if (s_long_op_active != 0U) {
        /* OTA/Flash 长操作期间仅监控 W5500 任务，其余任务已挂起 */
        start = (uint32_t)BSP_WDG_W5500;
        end = start + 1U;
    }

    for (uint32_t i = start; i < end; i++) {
        bsp_wdg_slot_t *slot = &s_slots[i];

        if (slot->ever_kicked == 0U) {
            continue;
        }

        if (slot->counter != slot->last_counter) {
            /* 心跳计数有变化，说明任务仍在运行 */
            slot->last_counter = slot->counter;
            slot->last_progress_ms = now_ms;
            continue;
        }

        if ((now_ms - slot->last_progress_ms) > s_slot_timeout_ms[i]) {
            LOG_E(WDG_TAG, "HEALTH FAIL: '%s' stale %lu ms (limit %lu), cnt=%lu",
                  s_slot_names[i],
                  (unsigned long)(now_ms - slot->last_progress_ms),
                  (unsigned long)s_slot_timeout_ms[i],
                  (unsigned long)slot->counter);
            healthy = 0;
        }
    }

    return healthy;
}

/**
 * @brief  看门狗喂狗任务主循环
 * @param  argument  线程参数（未使用）
 * @retval 无
 */
static void WatchdogTask(void *argument)
{
    (void)argument;

    s_grace_end_ms = osKernelGetTickCount() + WDG_BOOT_GRACE_MS;
    LOG_I(WDG_TAG, "task start: feed every %lu ms, grace until tick %lu",
          (unsigned long)WDG_FEED_MS,
          (unsigned long)s_grace_end_ms);

    for (;;) {
        uint32_t now_ms = osKernelGetTickCount();
        int in_grace = (now_ms < s_grace_end_ms) ? 1 : 0;
        int healthy = 1;

        if (in_grace == 0) {
            /* 启动宽限期过后才开始检查各任务心跳 */
            healthy = WatchdogTasksHealthy(now_ms);
        }

        if (healthy != 0) {
            /* 任务健康（或仍在启动宽限期）才喂 IWDG */
            if (HAL_IWDG_Refresh(&hiwdg1) != HAL_OK) {
                LOG_E(WDG_TAG, "HAL_IWDG_Refresh failed");
            } else {
                s_feed_count++;
                if ((s_feed_count % WDG_STATUS_LOG_INTERVAL) == 1U) {
                    WatchdogLogSlotStatus(now_ms,
                                         in_grace ? "grace+feed" : "feed");
                } else {
                    LOG_D(WDG_TAG, "refresh ok tick=%lu grace=%d",
                          (unsigned long)now_ms, in_grace);
                }
            }
        } else {
            /* 停止喂狗，等待 IWDG 硬件复位以保留现场日志 */
            WatchdogLogSlotStatus(now_ms, "STOP feed -> IWDG reset soon");
            LOG_E(WDG_TAG, "stop feeding IWDG, waiting hardware reset");
            osDelay(10000);
            continue;
        }

        if (in_grace != 0) {
            uint32_t remain = s_grace_end_ms - now_ms;
            if ((s_feed_count % WDG_STATUS_LOG_INTERVAL) == 1U) {
                LOG_I(WDG_TAG, "grace remain %lu ms (still feeding IWDG)",
                      (unsigned long)remain);
            }
        }

        osDelay(WDG_FEED_MS);
    }
}

/**
 * @brief  创建看门狗喂狗任务
 * @retval 无
 */
void BSP_Watchdog_Start(void)
{
    const osThreadAttr_t attr = {
        .name = "watchdog",
        .stack_size = WDG_TASK_STACK_WORDS * 4U,
        .priority = (osPriority_t)osPriorityRealtime,
    };

    LOG_I(WDG_TAG, "IWDG cfg: PR=128 RL=2000 WIN=4095 (~8s @ 32k LSI)");

    if (osThreadNew(WatchdogTask, NULL, &attr) == NULL) {
        LOG_E(WDG_TAG, "osThreadNew(watchdog) failed");
    }
}
