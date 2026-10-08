/**
 * @file    bsp_ota_protocol.c
 * @brief   SOIP v2 协议帧组装与流式解析实现
 */
#include "bsp_ota_protocol.h"

#include <string.h>

#include "bsp_ota_crc.h"

/**
 * @brief  组装 SOIP 协议帧（含 CRC16）
 * @param  out         输出缓冲区
 * @param  out_cap     缓冲区容量
 * @param  cmd         命令码
 * @param  seq         序列号
 * @param  payload     载荷数据
 * @param  payload_len 载荷长度
 * @retval 帧总长度（字节），0 表示失败
 */
uint16_t bsp_ota_build_frame(uint8_t *out, uint16_t out_cap,
                             bsp_ota_cmd_t cmd, uint16_t seq,
                             const uint8_t *payload, uint16_t payload_len)
{
    uint16_t total;
    uint16_t crc;

    if ((out == NULL) || (payload_len > 1024U)) {
        return 0U;
    }

    total = (uint16_t)(BSP_OTA_HEADER_SIZE + payload_len + BSP_OTA_CRC_SIZE);
    if (total > out_cap) {
        return 0U;
    }

    out[0] = BSP_OTA_SOF0;
    out[1] = BSP_OTA_SOF1;
    out[2] = BSP_OTA_VERSION;
    out[3] = (uint8_t)cmd;
    out[4] = (uint8_t)(seq & 0xFFU);          /* seq 低字节 */
    out[5] = (uint8_t)((seq >> 8) & 0xFFU);   /* seq 高字节 */
    out[6] = (uint8_t)(payload_len & 0xFFU);    /* payload 长度低字节 */
    out[7] = (uint8_t)((payload_len >> 8) & 0xFFU);

    if ((payload != NULL) && (payload_len > 0U)) {
        memcpy(&out[BSP_OTA_HEADER_SIZE], payload, payload_len);
    }

    /* CRC16 覆盖 version~payload 全部字段（不含 SOF 两字节） */
    crc = bsp_ota_crc16_ccitt(&out[2], (uint32_t)(total - BSP_OTA_CRC_SIZE - 2U), 0xFFFFU);
    out[total - 2U] = (uint8_t)(crc & 0xFFU);       /* CRC 低字节在前（小端） */
    out[total - 1U] = (uint8_t)((crc >> 8) & 0xFFU);
    return total;
}

/**
 * @brief  校验帧内 CRC16 是否与尾部一致
 * @param  frame     完整帧数据
 * @param  total_len 帧总长度
 * @retval 1 校验通过
 * @retval 0 校验失败
 */
static int bsp_ota_verify_crc(const uint8_t *frame, uint16_t total_len)
{
    uint16_t expected;
    uint16_t got;

    if (total_len < (BSP_OTA_HEADER_SIZE + BSP_OTA_CRC_SIZE)) {
        return 0;
    }

    expected = bsp_ota_crc16_ccitt(&frame[2], (uint32_t)(total_len - BSP_OTA_CRC_SIZE - 2U), 0xFFFFU);
    got = (uint16_t)frame[total_len - 2U] | ((uint16_t)frame[total_len - 1U] << 8);
    return (expected == got) ? 1 : 0;
}

/**
 * @brief  从接收缓冲尝试解析一帧 SOIP 数据
 * @param  rx_buf  接收缓冲（原地消费已解析字节）
 * @param  rx_len  当前缓冲长度（入参/出参）
 * @param  frame   解析结果
 * @retval 1 解析成功
 * @retval 0 数据不足或无效
 */
int bsp_ota_try_parse(uint8_t *rx_buf, uint16_t *rx_len, bsp_ota_frame_t *frame)
{
    uint16_t len;
    uint16_t payload_len;
    uint16_t total_len;
    uint16_t i;

    if ((rx_buf == NULL) || (rx_len == NULL) || (frame == NULL)) {
        return 0;
    }

    len = *rx_len;
    /* 外层循环：缓冲中可能有多帧或半帧，逐帧尝试解析 */
    while (len >= (BSP_OTA_HEADER_SIZE + BSP_OTA_CRC_SIZE)) {
        int start = -1;

        /* 步骤 1：扫描 SOF0 SOF1 同步头 */
        for (i = 0U; (i + 1U) < len; i++) {
            if ((rx_buf[i] == BSP_OTA_SOF0) && (rx_buf[i + 1U] == BSP_OTA_SOF1)) {
                start = (int)i;
                break;
            }
        }

        if (start < 0) {
            /* 未找到 SOF 同步头，丢弃全部脏数据 */
            *rx_len = 0U;
            return 0;
        }

        if (start > 0) {
            /* 对齐到帧头，丢弃 SOF 前的噪声字节 */
            memmove(rx_buf, &rx_buf[start], (size_t)(len - (uint16_t)start));
            len = (uint16_t)(len - (uint16_t)start);
        }

        if (len < BSP_OTA_HEADER_SIZE) {
            *rx_len = len;
            return 0;
        }

        payload_len = (uint16_t)rx_buf[6] | ((uint16_t)rx_buf[7] << 8);
        if (payload_len > 1024U) {
            /* 非法长度，跳过当前 SOF 继续搜下一帧 */
            memmove(rx_buf, &rx_buf[2], (size_t)(len - 2U));
            len = (uint16_t)(len - 2U);
            continue;
        }

        total_len = (uint16_t)(BSP_OTA_HEADER_SIZE + payload_len + BSP_OTA_CRC_SIZE);
        if (len < total_len) {
            /* 步骤 2：帧不完整，保留缓冲等待后续字节 */
            *rx_len = len;
            return 0;
        }

        /* 步骤 3：帧长度足够，校验 CRC */
        if (bsp_ota_verify_crc(rx_buf, total_len) == 0) {
            /* CRC 失败，丢弃 2 字节 SOF 后重试（可能是假同步） */
            memmove(rx_buf, &rx_buf[2], (size_t)(len - 2U));
            len = (uint16_t)(len - 2U);
            continue;
        }

        frame->cmd = rx_buf[3];
        frame->seq = (uint16_t)rx_buf[4] | ((uint16_t)rx_buf[5] << 8);
        frame->payload = &rx_buf[BSP_OTA_HEADER_SIZE];
        frame->payload_len = payload_len;
        frame->valid = 1U;

        /* 步骤 4：消费已解析帧，剩余字节留待下次解析（粘包场景） */
        memmove(rx_buf, &rx_buf[total_len], (size_t)(len - total_len));
        *rx_len = (uint16_t)(len - total_len);
        return 1;
    }

    *rx_len = len;
    return 0;
}
