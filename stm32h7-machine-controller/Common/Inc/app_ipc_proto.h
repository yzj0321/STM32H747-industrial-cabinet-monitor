/**
 * @file    app_ipc_proto.h
 * @brief   CM7/CM4 OpenAMP RPMsg 协议（双核共用）
 */
#ifndef APP_IPC_PROTO_H
#define APP_IPC_PROTO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RPMsg 通道名：Master / Remote 必须一致 */
#define APP_IPC_CHAN_NAME       "cabinet_ipc"

/** 小端 'CBIP' */
#define APP_IPC_MAGIC           (0x43424950UL)

/** CM7 → CM4：握手，完成 Remote dest 绑定 */
#define APP_IPC_MODE_PING       (2U)

/** CM4 → CM7：握手应答 */
#define APP_IPC_MODE_ACK        (3U)

/** CM4 → CM7：温湿度传感器数据 */
#define APP_IPC_MODE_SENSOR     (4U)

/** CM4 → CM7：水浸 ADC（counter 低 16 位=mV，bit16=浸水标志） */
#define APP_IPC_MODE_ADC        (5U)

/** CM4 → CM7：临时 Modbus 温湿度诊断状态 */
#define APP_IPC_MODE_SENSOR_STATUS (6U)

#define APP_IPC_SENSOR_STATUS_INIT_OK   (1U)
#define APP_IPC_SENSOR_STATUS_READ_OK   (2U)
#define APP_IPC_SENSOR_STATUS_READ_FAIL (3U)

/** MODE_ADC：浸水标志位于 counter[16] */
#define APP_IPC_ADC_WET_BIT     (16U)

/** D3 共享区（OpenAMP SHM + mailbox） */
#define APP_IPC_D3_BASE         (0x38000000UL)
#define APP_IPC_D3_SIZE         (64U * 1024U)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t mode;
    uint32_t seq;
    uint32_t counter;      /**< MODE_ADC: [15:0]=mV, [16]=wet */
    uint16_t temperature;
    uint16_t humidity;
} app_ipc_msg_t;

#ifdef __cplusplus
}
#endif

#endif /* APP_IPC_PROTO_H */
