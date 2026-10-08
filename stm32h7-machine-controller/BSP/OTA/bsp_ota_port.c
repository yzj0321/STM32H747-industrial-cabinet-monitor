/**
 * @file    bsp_ota_port.c
 * @brief   OTA 双核与看门狗端口控制实现
 *
 * 对齐 CamView：
 *   - CM7 OTA（Bank1）：清 BOOT_C2 + 静默总线主设备即可
 *   - CM4 OTA（Bank2）：须 D3 mailbox STOP CM4，再擦写
 * 并放宽看门狗对 W5500 长网络/Flash 操作的超时判定。
 */
#include "bsp_ota_port.h"

#include "bsp_watchdog.h"
#include "cm4_ota_hold.h"
#include "stm32h7xx.h"

static uint8_t s_ltdc_was_enabled;
static uint8_t s_cm4_ota_held;

/**
 * @brief  请求 CM4 进入 OTA hold
 */
int bsp_ota_port_hold_cm4_ota(uint32_t timeout_ms)
{
    if (cm4_ota_hold_request(timeout_ms) != 0) {
        return -1;
    }

    s_cm4_ota_held = 1U;
    return 0;
}

/**
 * @brief  释放 CM4 OTA hold
 */
void bsp_ota_port_release_cm4_ota(void)
{
    if (s_cm4_ota_held == 0U) {
        return;
    }

    cm4_ota_hold_release();
    s_cm4_ota_held = 0U;
}

/**
 * @brief  静默总线主设备以便擦写 Flash
 */
static void bsp_ota_port_quiesce_bus_masters(void)
{
    uint32_t timeout = 1000000U;

    s_ltdc_was_enabled = ((LTDC->GCR & LTDC_GCR_LTDCEN) != 0U) ? 1U : 0U;
    CLEAR_BIT(LTDC->GCR, LTDC_GCR_LTDCEN);

    if ((DMA2D->CR & DMA2D_CR_START) != 0U) {
        SET_BIT(DMA2D->CR, DMA2D_CR_ABORT);
        while (((DMA2D->CR & DMA2D_CR_START) != 0U) && (timeout-- > 0U)) {
        }
    }
    __DSB();
}

/**
 * @brief  阻止 CM4 从 Flash 启动（保持 CM7 独占 Flash 总线）
 *
 * 清除 RCC_GCR 的 BOOT_C2 位后，复位时 CM4 不会从 Flash 启动；
 * 配合 cm4_ota_hold_request() 将已运行的 CM4 置入 STOP。
 */
void bsp_ota_port_hold_cm4(void)
{
    CLEAR_BIT(RCC->GCR, RCC_GCR_BOOT_C2);
}

/**
 * @brief  进入 OTA 端口模式（挂起 CM4、启用长操作看门狗）
 *
 * 在 ERASE 命令处理前调用：
 * 1. hold CM4，防止双核 Flash 争用
 * 2. SetLongOp(1)，看门狗任务只监控 W5500 槽位，忽略已挂起的 LVGL/Modbus 等
 */
void bsp_ota_port_enter(void)
{
    bsp_ota_port_hold_cm4();
    bsp_ota_port_quiesce_bus_masters();
    BSP_Watchdog_SetLongOp(1U);
}

/**
 * @brief  退出 OTA 端口模式（恢复正常看门狗策略）
 *
 * VERIFY 成功、ABORT 或 Flash 失败 abort 时调用，恢复全任务心跳监控。
 * 注意：CM4 hold 状态保持至下次复位或 Boot 配置，本函数不恢复 CM4 启动。
 */
void bsp_ota_port_exit(void)
{
    bsp_ota_port_release_cm4_ota();
    if (s_ltdc_was_enabled != 0U) {
        SET_BIT(LTDC->GCR, LTDC_GCR_LTDCEN);
    }
    BSP_Watchdog_SetLongOp(0U);
}

