/**
 * @file    bsp_ota_protocol.h
 * @brief   SOIP v2 OTA 协议帧格式、命令码与解析接口
 *
 * 帧布局（小端多字节字段）：
 *   [SOF0=0xAA][SOF1=0x55][version][cmd][seq_lo][seq_hi]
 *   [plen_lo][plen_hi][payload...][crc16_lo][crc16_hi]
 *
 * CRC16 覆盖范围：version ~ payload（不含 SOF 与 CRC 自身），初值 0xFFFF。
 * 应答帧将请求 cmd 的最高位置 1（BSP_OTA_CMD_RESPONSE_MASK）。
 */
#ifndef BSP_OTA_PROTOCOL_H
#define BSP_OTA_PROTOCOL_H

#include <stdint.h>

/** 帧起始标志字节 0 / 1 */
#define BSP_OTA_SOF0                (0xAAU)
#define BSP_OTA_SOF1                (0x55U)
/** SOIP 协议版本号，写入帧头 version 字段 */
#define BSP_OTA_VERSION             (0x02U)
/** 固定头长度：SOF(2) + version + cmd + seq(2) + payload_len(2) */
#define BSP_OTA_HEADER_SIZE         (8U)
/** 帧尾 CRC16 字节数 */
#define BSP_OTA_CRC_SIZE            (2U)

/**
 * @brief  SOIP 命令码
 *
 * 请求侧使用 0x01~0x0A；设备应答时 cmd |= RESPONSE_MASK（0x80）。
 */
typedef enum {
    BSP_OTA_CMD_PING = 0x01,           /**< 连通性探测 */
    BSP_OTA_CMD_GET_INFO = 0x02,       /**< 查询设备/槽位信息（含 inactive 基址） */
    BSP_OTA_CMD_BEGIN_SESSION = 0x03,  /**< 开启会话，交换 session_token */
    BSP_OTA_CMD_ERASE = 0x04,          /**< 擦除目标槽 Flash */
    BSP_OTA_CMD_START_TRANSFER = 0x05, /**< 登记 image_size / image_crc32，准备收 DATA */
    BSP_OTA_CMD_DATA = 0x06,           /**< 固件数据分片（停等 ACK） */
    BSP_OTA_CMD_END_TRANSFER = 0x07,   /**< 声明传输结束 */
    BSP_OTA_CMD_VERIFY = 0x08,         /**< 对已写入镜像做 CRC32 校验并写 footer/meta */
    BSP_OTA_CMD_RESET = 0x09,          /**< 请求复位以切换新固件 */
    BSP_OTA_CMD_ABORT = 0x0A,          /**< 中止当前会话 */
    BSP_OTA_CMD_RESPONSE_MASK = 0x80U  /**< 应答标志：与请求 cmd 按位或 */
} bsp_ota_cmd_t;

/**
 * @brief  SOIP 应答状态码（通常位于应答载荷首字节）
 */
typedef enum {
    BSP_OTA_STATUS_OK = 0x00,          /**< 成功 */
    BSP_OTA_STATUS_ERR_CRC = 0x01,     /**< 帧 CRC16 错误 */
    BSP_OTA_STATUS_ERR_CMD = 0x02,     /**< 未知或不支持的命令 */
    BSP_OTA_STATUS_ERR_STATE = 0x03,   /**< 当前状态机不允许该命令 */
    BSP_OTA_STATUS_ERR_FLASH = 0x04,   /**< Flash 擦写失败 */
    BSP_OTA_STATUS_ERR_VERIFY = 0x05,  /**< 镜像 CRC32 校验失败 */
    BSP_OTA_STATUS_ERR_TIMEOUT = 0x06, /**< 操作超时 */
    BSP_OTA_STATUS_ERR_BUSY = 0x07,    /**< 设备忙 */
    BSP_OTA_STATUS_ERR_PARAM = 0x08,   /**< 参数非法（长度/地址/大小等） */
    BSP_OTA_STATUS_ERR_SESSION = 0x09, /**< session_token 不匹配或无会话 */
    BSP_OTA_STATUS_ERR_ABORTED = 0x0A  /**< 会话已被 ABORT */
} bsp_ota_status_t;

/**
 * @brief  解析得到的一帧 SOIP 数据（payload 指向 rx 缓冲内部，勿长期持有）
 */
typedef struct {
    uint8_t cmd;                /**< 命令码（请求侧不含 RESPONSE_MASK） */
    uint16_t seq;               /**< 序列号，用于 DATA 停等匹配 */
    const uint8_t *payload;     /**< 载荷指针，指向解析缓冲内 */
    uint16_t payload_len;       /**< 载荷长度（字节） */
    uint8_t valid;              /**< 非 0 表示帧有效且 CRC 通过 */
} bsp_ota_frame_t;

/**
 * @brief  组装 SOIP 协议帧（含 CRC16）
 * @param  out         输出缓冲区
 * @param  out_cap     缓冲区容量
 * @param  cmd         命令码
 * @param  seq         序列号
 * @param  payload     载荷数据（可为 NULL 当 payload_len==0）
 * @param  payload_len 载荷长度（上限 1024）
 * @retval 帧总长度（字节），0 表示参数或容量不足
 */
uint16_t bsp_ota_build_frame(uint8_t *out, uint16_t out_cap,
                             bsp_ota_cmd_t cmd, uint16_t seq,
                             const uint8_t *payload, uint16_t payload_len);

/**
 * @brief  从接收缓冲尝试解析一帧 SOIP 数据
 *
 * 成功时会从 rx_buf 头部消费已解析字节并更新 *rx_len；
 * 数据不足时返回 0 且不破坏缓冲内容。
 *
 * @param  rx_buf  接收缓冲（原地消费已解析字节）
 * @param  rx_len  当前缓冲长度（入参/出参）
 * @param  frame   解析结果
 * @retval 1 解析成功（frame->valid=1）
 * @retval 0 数据不足、SOF 未对齐或 CRC/长度无效
 */
int bsp_ota_try_parse(uint8_t *rx_buf, uint16_t *rx_len, bsp_ota_frame_t *frame);

#endif /* BSP_OTA_PROTOCOL_H */
