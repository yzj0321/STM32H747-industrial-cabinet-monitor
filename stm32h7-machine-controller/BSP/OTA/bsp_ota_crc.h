/**
 * @file    bsp_ota_crc.h
 * @brief   OTA 协议与镜像校验 CRC 算法
 *
 * 本模块提供两套互相独立的校验算法，均与上位机 SOIP 工具保持一致：
 *   - CRC16-CCITT：SOIP 帧尾完整性校验（见 bsp_ota_protocol）
 *   - CRC32：VERIFY 阶段对 inactive 槽 Flash 镜像做完整性校验；
 *            BEGIN_SESSION 时亦用于由 UID/nonce 生成 session_token
 *
 * 实现为逐字节位运算，无查表，便于在 Boot/App 两侧复用且节省 Flash。
 */
#ifndef BSP_OTA_CRC_H
#define BSP_OTA_CRC_H

#include <stdint.h>

/**
 * @brief  计算 CRC16-CCITT（SOIP 帧校验）
 *
 * 用于 bsp_ota_protocol 组装/解析 SOIP 帧：覆盖 version~payload 字段
 * （不含 SOF 两字节与帧尾 CRC 本身），初值 0xFFFF，多项式 0x1021（非反射）。
 *
 * @param  data   输入数据指针（不可为 NULL；length 为 0 时直接返回 init）
 * @param  length 参与计算的字节数
 * @param  init   CRC 初始值（SOIP 固定为 0xFFFF）
 * @retval CRC16 计算结果（由调用方按小端写入帧尾）
 *
 * @note   与上位机 SOIP 工具必须使用相同算法与初值，否则帧将被丢弃
 * @note   本函数不做空指针检查，由协议层保证入参合法
 */
uint16_t bsp_ota_crc16_ccitt(const uint8_t *data, uint32_t length, uint16_t init);

/**
 * @brief  计算 CRC32（固件镜像完整性校验 / session_token 哈希）
 *
 * 典型用途：
 *   1. VERIFY：对 inactive 槽 Flash 映射区按 image_size 逐字节计算，
 *      初值 0xFFFFFFFF，结果按位取反后与上位机 image_crc32 比对
 *   2. BEGIN_SESSION：对 UID 与 host_nonce 的混合缓冲做哈希，生成 session_token
 *
 * 算法为 Ethernet/ZIP 常用的反射 CRC32（多项式 0xEDB88320），与 boot_meta
 * 内部 CRC 及上位机镜像打包工具一致。
 *
 * @param  data   输入数据指针（可为 Flash 映射地址；不可为 NULL）
 * @param  length 参与计算的字节数（镜像有效长度，不含 slot footer）
 * @param  init   CRC 初始值（镜像/token 场景固定为 0xFFFFFFFF）
 * @retval CRC32 结果（已按位取反，可直接与 SOIP VERIFY 载荷或 footer 比较）
 *
 * @note   返回值已含最终取反；调用方不要再次 ~crc
 */
uint32_t bsp_ota_crc32(const uint8_t *data, uint32_t length, uint32_t init);

#endif /* BSP_OTA_CRC_H */
