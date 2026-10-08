/**
 * @file bsp_log.h
 * @brief UART1 分级日志输出（支持 ANSI 颜色与编译期裁剪）
 */
#ifndef BSP_LOG_H
#define BSP_LOG_H

#include <stdint.h>

#ifndef BSP_LOG_COMPILE_LEVEL
#define BSP_LOG_COMPILE_LEVEL   5
#endif

typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_VERBOSE,
} log_level_t;

/**
 * @brief  初始化日志模块（设置默认运行时级别）
 * @retval 无
 */
void bsp_log_init(void);

/**
 * @brief  创建日志 UART 互斥锁（RTOS 启动后调用）
 * @retval 无
 */
void bsp_log_rtos_init(void);

/**
 * @brief  锁定日志 UART 发送
 * @retval 无
 */
void bsp_log_uart_lock(void);

/**
 * @brief  解锁日志 UART 发送
 * @retval 无
 */
void bsp_log_uart_unlock(void);

/**
 * @brief  设置运行时日志级别
 * @param  level  日志级别
 * @retval 无
 */
void bsp_log_set_level(log_level_t level);

/**
 * @brief  获取当前运行时日志级别
 * @retval 当前日志级别
 */
log_level_t bsp_log_get_level(void);

/**
 * @brief  格式化并输出一条日志
 * @param  level  日志级别
 * @param  tag    模块标签
 * @param  fmt    printf 风格格式串
 * @retval 无
 */
void bsp_log_write(log_level_t level, const char *tag, const char *fmt, ...);

#if BSP_LOG_COMPILE_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(tag, fmt, ...) bsp_log_write(LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...) ((void)0)
#endif

#if BSP_LOG_COMPILE_LEVEL >= LOG_LEVEL_WARN
#define LOG_W(tag, fmt, ...) bsp_log_write(LOG_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...) ((void)0)
#endif

#if BSP_LOG_COMPILE_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(tag, fmt, ...) bsp_log_write(LOG_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...) ((void)0)
#endif

#if BSP_LOG_COMPILE_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D(tag, fmt, ...) bsp_log_write(LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...) ((void)0)
#endif

#if BSP_LOG_COMPILE_LEVEL >= LOG_LEVEL_VERBOSE
#define LOG_V(tag, fmt, ...) bsp_log_write(LOG_LEVEL_VERBOSE, tag, fmt, ##__VA_ARGS__)
#else
#define LOG_V(tag, fmt, ...) ((void)0)
#endif

#endif /* BSP_LOG_H */
