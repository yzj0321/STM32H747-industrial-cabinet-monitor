/**
 * @file bsp_log_conf.h
 * @brief BSP 日志模块配置项
 *
 * 需在包含 bsp_log.h 之前引入本文件以覆盖默认配置。
 */
#ifndef BSP_LOG_CONF_H
#define BSP_LOG_CONF_H

/** 默认运行时日志级别（低于此级别的消息将被丢弃） */
#ifndef BSP_LOG_LEVEL
#define BSP_LOG_LEVEL           4   /* LOG_LEVEL_DEBUG */
#endif

/** Release 构建时编译期裁剪日志 */
#ifndef BSP_LOG_COMPILE_LEVEL
#if defined(NDEBUG)
#define BSP_LOG_COMPILE_LEVEL   3   /* LOG_LEVEL_INFO */
#else
#define BSP_LOG_COMPILE_LEVEL   5   /* LOG_LEVEL_VERBOSE */
#endif
#endif

#define BSP_LOG_ENABLE_COLOR      1
#define BSP_LOG_ENABLE_TIMESTAMP  1
#define BSP_LOG_ENABLE_TAG        1
#define BSP_LOG_ENABLE_LEVEL_CHAR 1

/** 格式化消息最大长度（不含 ANSI 转义与后缀） */
#define BSP_LOG_BUF_SIZE          384

#endif /* BSP_LOG_CONF_H */
