/**
 * @file    CrcUtils.cpp
 * @brief   CRC16-CCITT 与 CRC32 校验算法实现
 */

#include "CrcUtils.h"

namespace CrcUtils {

/**
 * @brief  计算 QByteArray 的 CRC16-CCITT 校验值
 */
quint16 crc16Ccitt(const QByteArray &data, quint16 init)
{
    return crc16Ccitt(reinterpret_cast<const uint8_t *>(data.constData()), data.size(), init);
}

/**
 * @brief  计算字节数组的 CRC16-CCITT 校验值
 */
quint16 crc16Ccitt(const uint8_t *data, int length, quint16 init)
{
    quint16 crc = init;
    for (int i = 0; i < length; ++i) {
        crc ^= static_cast<quint16>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000)
                crc = static_cast<quint16>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

/**
 * @brief  计算 QByteArray 的 CRC32 校验值
 */
quint32 crc32(const QByteArray &data, quint32 init)
{
    return crc32(reinterpret_cast<const uint8_t *>(data.constData()), data.size(), init);
}

/**
 * @brief  计算字节数组的 CRC32 校验值
 */
quint32 crc32(const uint8_t *data, int length, quint32 init)
{
    quint32 crc = init;
    for (int i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320U;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

} // namespace CrcUtils
