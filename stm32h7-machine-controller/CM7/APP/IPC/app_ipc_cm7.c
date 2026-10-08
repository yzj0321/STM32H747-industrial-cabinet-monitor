/**
 * @file    app_ipc_cm7.c
 * @brief   CM7：OpenAMP Master，接收 CM4 温湿度 / 水浸 ADC 数据
 */
#include "app_ipc_cm7.h"
#include "cm4_boot_handoff.h"

#include "bsp_app_data.h"
#include "bsp_log.h"
#include "main.h"
#include "openamp.h"

#include "cmsis_os2.h"

#include <string.h>

#define IPC_TAG                 "IPC"
#define MODBUS_TH_TAG           "MODBUS-TH"

static struct rpmsg_endpoint s_ept;
static volatile int s_service_ready;
static volatile int s_ack_ready;
static uint8_t s_master_ok;

/**
 * @brief  共享内存写屏障
 */
static void app_ipc_shm_barrier(void)
{
    __DSB();
    __ISB();
}

/**
 * @brief  清空 D3 OpenAMP 共享区
 */
void app_ipc_d3_clear(void)
{
    volatile uint32_t *p = (volatile uint32_t *)APP_IPC_D3_BASE;
    uint32_t words = APP_IPC_D3_SIZE / sizeof(uint32_t);
    uint32_t i;

    for (i = 0U; i < words; i++) {
        p[i] = 0U;
    }
    app_ipc_shm_barrier();
}

static int rpmsg_recv_cb(struct rpmsg_endpoint *ept, void *data,
                         size_t len, uint32_t src, void *priv)
{
    const app_ipc_msg_t *msg;

    (void)ept;
    (void)src;
    (void)priv;

    if ((data == NULL) || (len < sizeof(app_ipc_msg_t))) {
        return 0;
    }

    msg = (const app_ipc_msg_t *)data;
    if (msg->magic != APP_IPC_MAGIC) {
        return 0;
    }

    if (msg->mode == APP_IPC_MODE_ACK) {
        s_ack_ready = 1;
    } else if (msg->mode == APP_IPC_MODE_SENSOR) {
        BSP_AppData_SetTemperature(msg->temperature);
        BSP_AppData_SetHumidity(msg->humidity);
    } else if (msg->mode == APP_IPC_MODE_ADC) {
        BSP_AppData_SetAdcMv((uint16_t)(msg->counter & 0xFFFFU));
        BSP_AppData_SetWet((uint8_t)((msg->counter >> APP_IPC_ADC_WET_BIT) & 0x1U));
    } else if (msg->mode == APP_IPC_MODE_SENSOR_STATUS) {
        uint32_t status = msg->counter >> 16;
        uint32_t detail = msg->counter & 0xFFFFU;
        if (status == APP_IPC_SENSOR_STATUS_READ_OK) {
            LOG_I(MODBUS_TH_TAG, "read OK raw_temp=%u raw_humi=%u",
                  (unsigned)msg->temperature, (unsigned)msg->humidity);
        } else {
            LOG_W(MODBUS_TH_TAG, "read failed detail=%lu (check power, A/B, MAX485 and 9600-8N1)",
                  (unsigned long)detail);
        }
    }

    return 0;
}

/**
 * @brief  RPMsg 服务销毁回调
 */
static void service_destroy_cb(struct rpmsg_endpoint *ept)
{
    (void)ept;
    s_service_ready = 0;
}

static void new_service_cb(struct rpmsg_device *rdev, const char *name,
                           uint32_t dest)
{
    (void)rdev;
    (void)name;
    OPENAMP_create_endpoint(&s_ept, name, dest, rpmsg_recv_cb, service_destroy_cb);
    s_service_ready = 1;
}

/**
 * @brief  发送 IPC 控制帧
 */
static int app_ipc_send_ctrl(uint32_t mode, uint32_t seq)
{
    app_ipc_msg_t ctrl;

    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.magic = APP_IPC_MAGIC;
    ctrl.mode = mode;
    ctrl.seq = seq;
    app_ipc_shm_barrier();
    return OPENAMP_send(&s_ept, &ctrl, sizeof(ctrl));
}

/**
 * @brief  初始化 CM7 OpenAMP Master
 */
int app_ipc_cm7_init_master(void)
{
    OPENAMP_init_ept(&s_ept);
    s_service_ready = 0;
    s_ack_ready = 0;
    s_master_ok = 0U;

    if (MX_OPENAMP_Init(RPMSG_MASTER, new_service_cb) != 0) {
        return -1;
    }

    s_master_ok = 1U;
    LOG_I(IPC_TAG, "OpenAMP master ready");
    return 0;
}

/**
 * @brief  等待 CM4 端点就绪
 */
int app_ipc_cm7_wait_endpoint(void)
{
    uint32_t t0;

    if (s_master_ok == 0U) {
        return -1;
    }

    t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < 5000U) {
        OPENAMP_check_for_message();
        if (s_service_ready != 0) {
            LOG_I(IPC_TAG, "CM4 endpoint ready");

            s_ack_ready = 0;
            (void)app_ipc_send_ctrl(APP_IPC_MODE_PING, 0U);

            t0 = HAL_GetTick();
            while ((HAL_GetTick() - t0) < 300U) {
                OPENAMP_check_for_message();
                if (s_ack_ready != 0) {
                    s_ack_ready = 0;
                    LOG_I(IPC_TAG, "CM4 ACK received");
                    return 0;
                }
                osDelay(1);
            }

            LOG_W(IPC_TAG, "CM4 ACK timeout, keep polling");
            return 0;
        }
        osDelay(1);
    }

    LOG_W(IPC_TAG, "wait CM4 endpoint timeout, CM4 diag=%lu",
          (unsigned long)cm4_ipc_diag_get());
    return -2;
}

/**
 * @brief  轮询接收 CM4 IPC 数据
 */
void app_ipc_cm7_poll(void)
{
    if ((s_master_ok == 0U) || (s_service_ready == 0)) {
        return;
    }

    OPENAMP_check_for_message();
}
