/**
 * @file bsp_log.c
 * @brief UART1 分级日志输出实现
 */
#include "bsp_log_conf.h"
#include "bsp_log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"
#include "usart.h"

#ifndef BSP_LOG_LEVEL
#define BSP_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#ifndef BSP_LOG_BUF_SIZE
#define BSP_LOG_BUF_SIZE 384
#endif

#define BSP_LOG_UART_TIMEOUT_MS 1000U

typedef struct {
    log_level_t level;
    char letter;
    const char *color;
} bsp_log_level_desc_t;

static const bsp_log_level_desc_t s_level_desc[] = {
    {LOG_LEVEL_ERROR,   'E', "\033[31m"},
    {LOG_LEVEL_WARN,    'W', "\033[33m"},
    {LOG_LEVEL_INFO,    'I', "\033[32m"},
    {LOG_LEVEL_DEBUG,   'D', "\033[36m"},
    {LOG_LEVEL_VERBOSE, 'V', "\033[90m"},
};

static const char s_color_reset[] = "\033[0m";

static log_level_t s_runtime_level = LOG_LEVEL_DEBUG;
static osMutexId_t s_log_mutex;
static uint8_t s_rtos_ready;

/**
 * @brief  查找日志级别对应的描述项
 * @param  level  日志级别
 * @retval 级别描述指针；未知级别时返回 ERROR 描述
 */
static const bsp_log_level_desc_t *bsp_log_level_desc_get(log_level_t level)
{
    uint32_t i;

    for (i = 0; i < (sizeof(s_level_desc) / sizeof(s_level_desc[0])); i++) {
        if (s_level_desc[i].level == level) {
            return &s_level_desc[i];
        }
    }

    return &s_level_desc[0];
}

/**
 * @brief  锁定日志 UART 发送
 * @retval 无
 */
void bsp_log_uart_lock(void)
{
    if (s_rtos_ready != 0U) {
        (void)osMutexAcquire(s_log_mutex, osWaitForever);
    }
}

/**
 * @brief  解锁日志 UART 发送
 * @retval 无
 */
void bsp_log_uart_unlock(void)
{
    if (s_rtos_ready != 0U) {
        (void)osMutexRelease(s_log_mutex);
    }
}

/**
 * @brief  通过 UART1 阻塞发送日志数据
 * @param  data  待发送缓冲区
 * @param  len   字节数
 * @retval 无
 */
static void bsp_log_uart_write(const char *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U)) {
        return;
    }

    (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, BSP_LOG_UART_TIMEOUT_MS);
}

/**
 * @brief  初始化日志模块（设置默认运行时级别）
 * @retval 无
 */
void bsp_log_init(void)
{
    s_runtime_level = (log_level_t)BSP_LOG_LEVEL;
}

/**
 * @brief  创建日志 UART 互斥锁（RTOS 启动后调用）
 * @retval 无
 */
void bsp_log_rtos_init(void)
{
    if (s_rtos_ready != 0U) {
        return;
    }

    s_log_mutex = osMutexNew(NULL);
    s_rtos_ready = 1U;
}

/**
 * @brief  设置运行时日志级别
 * @param  level  日志级别
 * @retval 无
 */
void bsp_log_set_level(log_level_t level)
{
    s_runtime_level = level;
}

/**
 * @brief  获取当前运行时日志级别
 * @retval 当前日志级别
 */
log_level_t bsp_log_get_level(void)
{
    return s_runtime_level;
}

/**
 * @brief  格式化并输出一条日志
 * @param  level  日志级别
 * @param  tag    模块标签
 * @param  fmt    printf 风格格式串
 * @retval 无
 */
void bsp_log_write(log_level_t level, const char *tag, const char *fmt, ...)
{
    const bsp_log_level_desc_t *desc;
    char message[BSP_LOG_BUF_SIZE];
    char line[BSP_LOG_BUF_SIZE + 64U];
    va_list args;
    int prefix_len;
    int msg_len;
    int total_len;

    if ((level == LOG_LEVEL_NONE) || (level > LOG_LEVEL_VERBOSE) || (level > s_runtime_level)) {
        return;
    }

    if ((tag == NULL) || (fmt == NULL)) {
        return;
    }

    desc = bsp_log_level_desc_get(level);

    va_start(args, fmt);
    msg_len = vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    if (msg_len < 0) {
        return;
    }

    if ((size_t)msg_len >= sizeof(message)) {
        msg_len = (int)(sizeof(message) - 1U);
        message[msg_len] = '\0';
    }

    prefix_len = 0;

#if BSP_LOG_ENABLE_COLOR
    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "%s", desc->color);
#endif

    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "[");

#if BSP_LOG_ENABLE_LEVEL_CHAR
    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "%c", desc->letter);
#endif

#if BSP_LOG_ENABLE_TIMESTAMP
    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "][%06lu",
                           (unsigned long)HAL_GetTick());
#endif

#if BSP_LOG_ENABLE_TAG
    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "][%s", tag);
#endif

    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "] %s", message);

#if BSP_LOG_ENABLE_COLOR
    prefix_len += snprintf(line + prefix_len, sizeof(line) - (size_t)prefix_len, "%s", s_color_reset);
#endif

    if (prefix_len < 0) {
        return;
    }

    total_len = prefix_len;
    if ((size_t)total_len >= (sizeof(line) - 3U)) {
        total_len = (int)(sizeof(line) - 3U);
    }

    line[total_len++] = '\r';
    line[total_len++] = '\n';
    line[total_len] = '\0';

    bsp_log_uart_lock();
    bsp_log_uart_write(line, (uint16_t)total_len);
    bsp_log_uart_unlock();
}
