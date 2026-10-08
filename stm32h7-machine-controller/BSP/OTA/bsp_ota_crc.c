/**
 * @file    bsp_ota_crc.c
 * @brief   OTA CRC16/CRC32 算法实现
 *
 * 位运算逐字节实现，无查表，Flash 占用小。
 * 算法参数必须与 SOIP 上位机、boot_meta CRC 及镜像打包工具保持一致，
 * 任一参数（初值 / 多项式 / 是否反射 / 是否最终取反）不匹配都会导致：
 *   - 帧 CRC 校验失败（CRC16）
 *   - VERIFY 失败或误升级（CRC32）
 */
#include "bsp_ota_crc.h"

/**
 * @brief  计算 CRC16-CCITT（SOIP 帧校验）
 *
 * 处理流程（每输入字节）：
 *   1. 将字节左移 8 位后与当前 CRC 异或（移入寄存器高 8 位）
 *   2. 对 8 个 bit 做非反射左移：MSB=1 时左移并异或 0x1021，否则仅左移
 *
 * @param  data   输入数据指针
 * @param  length 字节数
 * @param  init   初始值（SOIP 使用 0xFFFF）
 * @retval CRC16 结果（不做最终取反）
 * @note   多项式 0x1021 = x^16 + x^12 + x^5 + 1（CCITT）
 */
uint16_t bsp_ota_crc16_ccitt(const uint8_t *data, uint32_t length, uint16_t init)
{
    uint16_t crc = init;

    for (uint32_t i = 0U; i < length; i++) {
        /* 当前字节移入 CRC 寄存器高 8 位 */
        crc ^= (uint16_t)data[i] << 8;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x8000U) != 0U) {
                /* MSB 为 1：左移并与 CCITT 多项式异或 */
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

/**
 * @brief  计算 CRC32（镜像校验 / session_token）
 *
 * 处理流程（每输入字节）：
 *   1. 字节直接与 CRC 低 8 位异或（反射输入）
 *   2. 对 8 个 bit 做反射右移：LSB=1 时右移并异或 0xEDB88320，否则仅右移
 *   3. 全部字节处理完后对寄存器按位取反再返回
 *
 * @param  data   输入数据指针
 * @param  length 字节数
 * @param  init   初始值（镜像/token 使用 0xFFFFFFFF）
 * @retval CRC32 结果（已按位取反）
 * @note   反射多项式 0xEDB88320 对应正向 0x04C11DB7，与 Ethernet CRC32 相同
 */
uint32_t bsp_ota_crc32(const uint8_t *data, uint32_t length, uint32_t init)
{
    uint32_t crc = init;

    for (uint32_t i = 0U; i < length; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            if ((crc & 1U) != 0U) {
                /* LSB 为 1：右移并与反射多项式异或 */
                crc = (crc >> 1) ^ 0xEDB88320U;
            } else {
                crc >>= 1;
            }
        }
    }

    /* 标准 CRC32 输出需对寄存器取反，与上位机 / ZIP / Ethernet 约定一致 */
    return ~crc;
}
