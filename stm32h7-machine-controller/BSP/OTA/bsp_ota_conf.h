/**
 * @file    bsp_ota_conf.h
 * @brief   OTA 模块编译期配置宏
 *
 * 集中定义 SOIP 协议标识、通信缓冲、调试开关、RTOS 任务参数及 Flash/SDRAM
 * 备份区地址。修改本文件即可调整 OTA 行为，无需改动业务逻辑代码。
 */
#ifndef BSP_OTA_CONF_H
#define BSP_OTA_CONF_H

#include "boot_slot.h"

/** @defgroup OTA_PROTO 协议与设备标识 */
/** SOIP 协议版本字符串，GET_INFO 应答中上报给上位机 */
#define BSP_OTA_PROTO_VERSION       "SOIP v2.0"
/** 设备名称，GET_INFO 应答中上报 */
#define BSP_OTA_DEVICE_NAME         "STM32H747-CM7"
#define BSP_OTA_DEVICE_NAME_CM4     "STM32H747-CM4"
/** @} */

/** @defgroup OTA_BUFFER 通信缓冲与分片参数 */
/**
 * SOIP 收/发缓冲容量（字节）。
 * DATA 帧最大约：头 8 + token/offset 8 + chunk 512 + CRC 2 ≈ 530，留余量取 768。
 */
#define BSP_OTA_RX_BUF_SIZE         (768U)
/**
 * DATA 分片（与 Qt kDefaultChunkSize 对齐）。
 * 512B：减少停等往返次数；SOIP DATA 帧长约 530B（含头/CRC）。
 */
#define BSP_OTA_CHUNK_SIZE          (512U)
/** 单包 DATA 载荷上限，超出则 manager 返回 ERR_PARAM */
#define BSP_OTA_CHUNK_MAX           (512U)
/** @} */

/** @defgroup OTA_DEBUG 调试与日志开关 */
/** 0=DATA 写 Flash 时不打 program begin/done，减轻 UART 占用 */
#define BSP_OTA_FLASH_VERBOSE       0U
/** 0=不打印每条 SOIP/DATA 应答，联调时可改 1 */
#define BSP_OTA_SOIP_VERBOSE        0U
/** 每 N 个 DATA 分片打一条进度（0=关闭；传输期打日志会拖慢 ACK） */
#define BSP_OTA_DATA_LOG_INTERVAL   0U
/** @} */

/** @defgroup OTA_TASK RTOS 任务配置 */
/** OTA 任务栈大小（字，FreeRTOS 下 1 word = 4 字节） */
#define BSP_OTA_TASK_STACK_WORDS    (2048U)
/** @} */

/** @defgroup OTA_FLASH 分区说明（见 boot_slot.h） */
/** S4 Reserve 基址（0x08080000），A/B 槽之间隔离区，OTA 不擦除 */
#define BSP_OTA_RESERVE_BASE        (0x08080000UL)
/** Reserve 大小（128KB） */
#define BSP_OTA_RESERVE_SIZE        (128U * 1024U)
/** @} */

#endif /* BSP_OTA_CONF_H */

