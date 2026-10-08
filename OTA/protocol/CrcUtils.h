/**
 * @file    CrcUtils.h
 * @brief   CRC16-CCITT 与 CRC32 校验工具
 */

#ifndef CRCUTILS_H
#define CRCUTILS_H

#include <QByteArray>
#include <cstdint>

namespace CrcUtils {

/**
 * @brief  计算 QByteArray 的 CRC16-CCITT 校验值
 * @param  data 待校验数据
 * @param  init CRC 初始值
 * @retval CRC16 校验结果
 */
quint16 crc16Ccitt(const QByteArray &data, quint16 init = 0xFFFF);

/**
 * @brief  计算字节数组的 CRC16-CCITT 校验值
 * @param  data 待校验数据指针
 * @param  length 数据长度
 * @param  init CRC 初始值
 * @retval CRC16 校验结果
 */
quint16 crc16Ccitt(const uint8_t *data, int length, quint16 init = 0xFFFF);

/**
 * @brief  计算 QByteArray 的 CRC32 校验值
 * @param  data 待校验数据
 * @param  init CRC 初始值
 * @retval CRC32 校验结果
 */
quint32 crc32(const QByteArray &data, quint32 init = 0xFFFFFFFF);

/**
 * @brief  计算字节数组的 CRC32 校验值
 * @param  data 待校验数据指针
 * @param  length 数据长度
 * @param  init CRC 初始值
 * @retval CRC32 校验结果
 */
quint32 crc32(const uint8_t *data, int length, quint32 init = 0xFFFFFFFF);

} // namespace CrcUtils

#endif // CRCUTILS_H
