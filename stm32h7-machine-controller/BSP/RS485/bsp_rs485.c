/**
 * @file    bsp_rs485.c
 * @brief   MAX485 RS485 半双工驱动
 */
#include "bsp_rs485_conf.h"
#include "bsp_rs485.h"

#include "main.h"
#include "cmsis_os2.h"

#include <string.h>

static osMutexId_t s_bus_mutex;

/**
 * @brief  初始化 RS485 方向控制 GPIO
 */
static void rs485_gpio_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(BSP_RS485_DE_GPIO_PORT, BSP_RS485_DE_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_PORT, BSP_RS485_RE_GPIO_PIN, GPIO_PIN_RESET);

    gpio.Pin = BSP_RS485_DE_GPIO_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BSP_RS485_DE_GPIO_PORT, &gpio);

    gpio.Pin = BSP_RS485_RE_GPIO_PIN;
    HAL_GPIO_Init(BSP_RS485_RE_GPIO_PORT, &gpio);
}

/**
 * @brief  获取 RS485 互斥锁
 */
static void rs485_lock(void)
{
    if (s_bus_mutex != NULL) {
        (void)osMutexAcquire(s_bus_mutex, osWaitForever);
    }
}

/**
 * @brief  释放 RS485 互斥锁
 */
static void rs485_unlock(void)
{
    if (s_bus_mutex != NULL) {
        (void)osMutexRelease(s_bus_mutex);
    }
}

/**
 * @brief  初始化 RS485 驱动
 */
void BSP_Rs485_Init(void)
{
    rs485_gpio_init();
    BSP_Rs485_SetRxMode();

    if (s_bus_mutex == NULL) {
        s_bus_mutex = osMutexNew(NULL);
    }
}

/**
 * @brief  切换 RS485 为接收模式
 */
void BSP_Rs485_SetRxMode(void)
{
    HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_PORT, BSP_RS485_RE_GPIO_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(BSP_RS485_DE_GPIO_PORT, BSP_RS485_DE_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  切换 RS485 为发送模式
 */
void BSP_Rs485_SetTxMode(void)
{
    HAL_GPIO_WritePin(BSP_RS485_RE_GPIO_PORT, BSP_RS485_RE_GPIO_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BSP_RS485_DE_GPIO_PORT, BSP_RS485_DE_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  设置 RS485 波特率
 */
int BSP_Rs485_SetBaud(uint32_t baud)
{
    int ret = HAL_ERROR;

    rs485_lock();
    if (HAL_UART_DeInit(&BSP_RS485_UART_HANDLE) == HAL_OK) {
        BSP_RS485_UART_HANDLE.Init.BaudRate = baud;
        if (HAL_UART_Init(&BSP_RS485_UART_HANDLE) == HAL_OK) {
            BSP_Rs485_SetRxMode();
            ret = HAL_OK;
        }
    }
    rs485_unlock();

    return ret;
}

/**
 * @brief  RS485 发送数据
 */
int BSP_Rs485_Send(const uint8_t *data, uint16_t len, uint32_t timeout)
{
    HAL_StatusTypeDef status;

    if ((data == NULL) || (len == 0U)) {
        return HAL_ERROR;
    }

    rs485_lock();
    BSP_Rs485_SetTxMode();
    status = HAL_UART_Transmit(&BSP_RS485_UART_HANDLE, (uint8_t *)data, len, timeout);
    if (status == HAL_OK) {
        while (__HAL_UART_GET_FLAG(&BSP_RS485_UART_HANDLE, UART_FLAG_TC) == RESET) {
        }
    }
    BSP_Rs485_SetRxMode();
    rs485_unlock();

    return status;
}

/**
 * @brief  RS485 发送字符串
 */
int BSP_Rs485_SendString(const char *str)
{
    if (str == NULL) {
        return HAL_ERROR;
    }

    return BSP_Rs485_Send((const uint8_t *)str, (uint16_t)strlen(str), 100U);
}

/**
 * @brief  RS485 接收单字节
 */
int BSP_Rs485_RecvByte(uint8_t *data, uint32_t timeout_ms)
{
    int ret = -1;

    if (data == NULL) {
        return -1;
    }

    rs485_lock();
    BSP_Rs485_SetRxMode();
    if (HAL_UART_Receive(&BSP_RS485_UART_HANDLE, data, 1U, timeout_ms) == HAL_OK) {
        ret = 0;
    }
    rs485_unlock();

    return ret;
}

/**
 * @brief  清空 RS485 接收缓冲
 */
int BSP_Rs485_Flush(void)
{
    uint8_t dummy;
    int count = 0;

    rs485_lock();
    BSP_Rs485_SetRxMode();
    while (HAL_UART_Receive(&BSP_RS485_UART_HANDLE, &dummy, 1U, 1U) == HAL_OK) {
        count++;
    }
    rs485_unlock();

    return count;
}
