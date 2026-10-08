/**
 * @file    app_ipc_cm4.c
 * @brief   CM4：OpenAMP Remote，接收握手并向 CM7 发送温湿度 / 水浸 ADC 数据
 */
#include "app_ipc_cm4.h"

#include "app_ipc_proto.h"
#include "main.h"
#include "openamp.h"

#include <string.h>

static struct rpmsg_endpoint s_ept;
static volatile int s_ping_pending;
static volatile uint32_t s_ping_seq;
static uint32_t s_remote_ok;
static uint32_t s_sensor_seq;
static uint32_t s_adc_seq;

/**
 * @brief  共享内存写屏障
 */
static void app_ipc_shm_barrier(void)
{
    __DSB();
    __ISB();
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

    if (msg->mode == APP_IPC_MODE_PING) {
        s_ping_seq = msg->seq;
        s_ping_pending = 1;
    }

    return 0;
}

/**
 * @brief  尝试应答 PING
 */
static void try_send_ping_ack(void)
{
    app_ipc_msg_t ack;

    if (s_ping_pending == 0) {
        return;
    }
    s_ping_pending = 0;

    if (!is_rpmsg_ept_ready(&s_ept)) {
        return;
    }

    memset(&ack, 0, sizeof(ack));
    ack.magic = APP_IPC_MAGIC;
    ack.mode = APP_IPC_MODE_ACK;
    ack.seq = s_ping_seq;
    app_ipc_shm_barrier();
    (void)rpmsg_trysend(&s_ept, &ack, sizeof(ack));
}

/**
 * @brief  初始化 CM4 OpenAMP Remote
 */
int app_ipc_cm4_init(void)
{
    int status;

    s_remote_ok = 0U;
    s_ping_pending = 0;
    s_sensor_seq = 0U;
    s_adc_seq = 0U;

    status = MX_OPENAMP_Init(RPMSG_REMOTE, NULL);
    if (status != 0) {
        return -1;
    }

    status = OPENAMP_create_endpoint(&s_ept, APP_IPC_CHAN_NAME, RPMSG_ADDR_ANY,
                                     rpmsg_recv_cb, NULL);
    if (status < 0) {
        return -2;
    }

    s_remote_ok = 1U;
    return 0;
}

/**
 * @brief  轮询处理 CM4 IPC
 */
void app_ipc_cm4_poll(void)
{
    if (s_remote_ok == 0U) {
        return;
    }

    OPENAMP_check_for_message();
    try_send_ping_ack();
}

/**
 * @brief  发送温湿度到 CM7
 */
int app_ipc_cm4_send_sensor(uint16_t temperature, uint16_t humidity)
{
    app_ipc_msg_t msg;

    if ((s_remote_ok == 0U) || (!is_rpmsg_ept_ready(&s_ept))) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.magic = APP_IPC_MAGIC;
    msg.mode = APP_IPC_MODE_SENSOR;
    msg.seq = ++s_sensor_seq;
    msg.temperature = temperature;
    msg.humidity = humidity;
    app_ipc_shm_barrier();
    return (rpmsg_trysend(&s_ept, &msg, sizeof(msg)) == 0) ? 0 : -1;
}

int app_ipc_cm4_send_sensor_status(uint32_t status,
                                   uint32_t detail,
                                   uint16_t raw_temperature,
                                   uint16_t raw_humidity)
{
    app_ipc_msg_t msg;

    if ((s_remote_ok == 0U) || (!is_rpmsg_ept_ready(&s_ept))) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.magic = APP_IPC_MAGIC;
    msg.mode = APP_IPC_MODE_SENSOR_STATUS;
    msg.seq = ++s_sensor_seq;
    msg.counter = (status << 16) | (detail & 0xFFFFU);
    msg.temperature = raw_temperature;
    msg.humidity = raw_humidity;
    app_ipc_shm_barrier();
    return (rpmsg_trysend(&s_ept, &msg, sizeof(msg)) == 0) ? 0 : -1;
}

/**
 * @brief  发送水浸 ADC 到 CM7
 */
int app_ipc_cm4_send_adc(uint16_t adc_mv, uint8_t wet)
{
    app_ipc_msg_t msg;

    if ((s_remote_ok == 0U) || (!is_rpmsg_ept_ready(&s_ept))) {
        return -1;
    }

    memset(&msg, 0, sizeof(msg));
    msg.magic = APP_IPC_MAGIC;
    msg.mode = APP_IPC_MODE_ADC;
    msg.seq = ++s_adc_seq;
    msg.counter = (uint32_t)adc_mv;
    if (wet != 0U) {
        msg.counter |= (1UL << APP_IPC_ADC_WET_BIT);
    }
    app_ipc_shm_barrier();
    return (rpmsg_trysend(&s_ept, &msg, sizeof(msg)) == 0) ? 0 : -1;
}
