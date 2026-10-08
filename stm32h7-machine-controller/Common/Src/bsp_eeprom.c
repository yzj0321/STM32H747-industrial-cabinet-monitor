/**
 * @file    bsp_eeprom.c
 * @brief   BL24C16F I2C EEPROM 驱动实现
 */
#include "bsp_eeprom.h"

#include "i2c.h"
#include "stm32h7xx_hal.h"

#include <stdio.h>

#ifdef BSP_EEPROM_USE_RTOS_MUTEX
#include "cmsis_os2.h"
#endif

#define BSP_EEPROM_I2C_TIMEOUT_MS   (200U)
#define BSP_EEPROM_WAIT_READY_MS    (200U)
#define BSP_EEPROM_PROBE_RETRIES    (3U)

static bool s_ready;

#ifdef BSP_EEPROM_USE_RTOS_MUTEX
static osMutexId_t s_mutex;
#endif

/**
 * @brief  获取 EEPROM 访问互斥锁
 */
static void eeprom_lock(void)
{
#ifdef BSP_EEPROM_USE_RTOS_MUTEX
    if ((s_mutex != NULL) && (osKernelGetState() == osKernelRunning)) {
        (void)osMutexAcquire(s_mutex, osWaitForever);
    }
#endif
}

/**
 * @brief  释放 EEPROM 访问互斥锁
 */
static void eeprom_unlock(void)
{
#ifdef BSP_EEPROM_USE_RTOS_MUTEX
    if ((s_mutex != NULL) && (osKernelGetState() == osKernelRunning)) {
        (void)osMutexRelease(s_mutex);
    }
#endif
}

/**
 * @brief  由字地址计算 I2C 设备地址
 */
static uint16_t eeprom_dev_addr(uint16_t addr)
{
    return (uint16_t)(BSP_EEPROM_DEV_ADDR_BASE |
                      (((addr >> 8U) & 0x07U) << 1U));
}

static void eeprom_log_i2c(const char *stage, uint16_t addr, uint16_t chunk,
                           HAL_StatusTypeDef st)
{
    printf("[EEPROM] %s addr=0x%04X dev=0x%02X word=0x%02X chunk=%u hal=%d "
           "i2c_st=%lu err=0x%08lX tick=%lu\r\n",
           stage,
           (unsigned int)addr,
           (unsigned int)eeprom_dev_addr(addr),
           (unsigned int)(addr & 0xFFU),
           (unsigned int)chunk,
           (int)st,
           (unsigned long)hi2c4.State,
           (unsigned long)hi2c4.ErrorCode,
           (unsigned long)HAL_GetTick());
}

/**
 * @brief  探测 EEPROM 是否在线
 */
static bool eeprom_probe(void)
{
    return (HAL_I2C_IsDeviceReady(&hi2c4,
                                  BSP_EEPROM_DEV_ADDR_BASE,
                                  BSP_EEPROM_PROBE_RETRIES,
                                  BSP_EEPROM_I2C_TIMEOUT_MS) == HAL_OK);
}

/**
 * @brief  复位 I2C 总线状态
 */
static void eeprom_bus_reset(void)
{
    (void)HAL_I2C_DeInit(&hi2c4);
    MX_I2C4_Init();
    s_ready = false;
}

/**
 * @brief  等待 EEPROM 内部写周期结束
 */
static bool eeprom_wait_ready(uint16_t addr)
{
    uint32_t start = HAL_GetTick();

    while (HAL_I2C_IsDeviceReady(&hi2c4, eeprom_dev_addr(addr), 1U, 1U) != HAL_OK) {
        if ((HAL_GetTick() - start) > BSP_EEPROM_WAIT_READY_MS) {
            eeprom_log_i2c("wait_ready timeout", addr, 0U, HAL_TIMEOUT);
            return false;
        }
    }

    return true;
}

/**
 * @brief  执行一次 EEPROM 读事务
 */
static bool eeprom_do_read(uint16_t addr, uint8_t *data, size_t len)
{
    while (len > 0U) {
        uint16_t block_remain = (uint16_t)(BSP_EEPROM_BLOCK_SIZE -
                                           (addr & (BSP_EEPROM_BLOCK_SIZE - 1U)));
        uint16_t chunk = (len < block_remain) ? (uint16_t)len : block_remain;
        HAL_StatusTypeDef st;

        st = HAL_I2C_Mem_Read(&hi2c4,
                              eeprom_dev_addr(addr),
                              (uint16_t)(addr & 0xFFU),
                              I2C_MEMADD_SIZE_8BIT,
                              data,
                              chunk,
                              BSP_EEPROM_I2C_TIMEOUT_MS);
        if (st != HAL_OK) {
            eeprom_log_i2c("read fail", addr, chunk, st);
            return false;
        }

        addr = (uint16_t)(addr + chunk);
        data += chunk;
        len -= chunk;
    }

    return true;
}

/**
 * @brief  执行一次 EEPROM 页写事务
 */
static bool eeprom_do_write(uint16_t addr, const uint8_t *data, size_t len)
{
    while (len > 0U) {
        uint16_t page_offset = (uint16_t)(addr & (BSP_EEPROM_PAGE_SIZE - 1U));
        uint16_t page_remain = (uint16_t)(BSP_EEPROM_PAGE_SIZE - page_offset);
        uint16_t chunk = (len < page_remain) ? (uint16_t)len : page_remain;
        HAL_StatusTypeDef st;

        st = HAL_I2C_Mem_Write(&hi2c4,
                               eeprom_dev_addr(addr),
                               (uint16_t)(addr & 0xFFU),
                               I2C_MEMADD_SIZE_8BIT,
                               (uint8_t *)data,
                               chunk,
                               BSP_EEPROM_I2C_TIMEOUT_MS);
        if (st != HAL_OK) {
            eeprom_log_i2c("write fail", addr, chunk, st);
            return false;
        }

        if (!eeprom_wait_ready(addr)) {
            return false;
        }

        addr = (uint16_t)(addr + chunk);
        data += chunk;
        len -= chunk;
    }

    return true;
}

/**
 * @brief  Flash 长操作后恢复 I2C/EEPROM
 */
bool BSP_Eeprom_Recover(void)
{
    eeprom_bus_reset();

    if (!eeprom_probe()) {
        eeprom_bus_reset();
        if (!eeprom_probe()) {
            printf("[EEPROM] recover failed i2c_st=%lu err=0x%08lX tick=%lu\r\n",
                   (unsigned long)hi2c4.State,
                   (unsigned long)hi2c4.ErrorCode,
                   (unsigned long)HAL_GetTick());
            s_ready = false;
            return false;
        }
    }

    s_ready = true;
    printf("[EEPROM] recover OK tick=%lu\r\n", (unsigned long)HAL_GetTick());
    return true;
}

/**
 * @brief  初始化 EEPROM 驱动
 */
bool BSP_Eeprom_Init(void)
{
    if (!eeprom_probe()) {
        printf("[EEPROM] init probe failed dev=0x%02X i2c_st=%lu err=0x%08lX tick=%lu\r\n",
               (unsigned int)BSP_EEPROM_DEV_ADDR_BASE,
               (unsigned long)hi2c4.State,
               (unsigned long)hi2c4.ErrorCode,
               (unsigned long)HAL_GetTick());
        s_ready = false;
        return false;
    }

#ifdef BSP_EEPROM_USE_RTOS_MUTEX
    if ((s_mutex == NULL) && (osKernelGetState() >= osKernelReady)) {
        s_mutex = osMutexNew(NULL);
    }
#endif

    s_ready = true;
    return true;
}

/**
 * @brief  查询 EEPROM 是否就绪
 */
bool BSP_Eeprom_IsReady(void)
{
    return s_ready;
}

/**
 * @brief  读取 EEPROM 数据
 */
bool BSP_Eeprom_Read(uint16_t addr, uint8_t *data, size_t len)
{
    if ((data == NULL) || (len == 0U) ||
        ((uint32_t)addr + len) > BSP_EEPROM_SIZE) {
        return false;
    }

    if (!s_ready) {
        return false;
    }

    eeprom_lock();
    if (eeprom_do_read(addr, data, len)) {
        eeprom_unlock();
        return true;
    }
    eeprom_unlock();

    printf("[EEPROM] read retry after recover addr=0x%04X len=%u tick=%lu\r\n",
           (unsigned int)addr,
           (unsigned int)len,
           (unsigned long)HAL_GetTick());
    if (!BSP_Eeprom_Recover()) {
        return false;
    }

    eeprom_lock();
    if (eeprom_do_read(addr, data, len)) {
        eeprom_unlock();
        return true;
    }
    eeprom_unlock();
    return false;
}

/**
 * @brief  写入 EEPROM 数据
 */
bool BSP_Eeprom_Write(uint16_t addr, const uint8_t *data, size_t len)
{
    if ((data == NULL) || (len == 0U) ||
        ((uint32_t)addr + len) > BSP_EEPROM_SIZE) {
        printf("[EEPROM] write bad param addr=0x%04X len=%u\r\n",
               (unsigned int)addr, (unsigned int)len);
        return false;
    }

    if (!s_ready) {
        printf("[EEPROM] write not ready addr=0x%04X len=%u tick=%lu\r\n",
               (unsigned int)addr,
               (unsigned int)len,
               (unsigned long)HAL_GetTick());
        return false;
    }

    eeprom_lock();
    if (eeprom_do_write(addr, data, len)) {
        eeprom_unlock();
        return true;
    }
    eeprom_unlock();

    printf("[EEPROM] write retry after recover addr=0x%04X len=%u tick=%lu\r\n",
           (unsigned int)addr,
           (unsigned int)len,
           (unsigned long)HAL_GetTick());
    if (!BSP_Eeprom_Recover()) {
        return false;
    }

    eeprom_lock();
    if (eeprom_do_write(addr, data, len)) {
        eeprom_unlock();
        return true;
    }
    eeprom_unlock();
    return false;
}
