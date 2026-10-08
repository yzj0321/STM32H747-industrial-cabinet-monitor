/**
 * @file    bsp_log_sd.h
 * @brief   SDNAND 异常日志（FatFs CSV）
 *
 * 文件目录：0:/log/
 * 主文件：alarms.csv（超限后轮转为 alarms_NNN.csv）
 *
 * CSV 行格式：
 *   tick_ms,level,code,temp_c,humi_rh,adc_mv,message
 */
#ifndef BSP_LOG_SD_H
#define BSP_LOG_SD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LOG_SD_DIR           "0:/log"
#define BSP_LOG_SD_FILE_MAIN     "alarms.csv"
#define BSP_LOG_SD_MAX_NAME      32U
#define BSP_LOG_SD_MAX_FILES     32U

typedef struct {
    char name[BSP_LOG_SD_MAX_NAME];
    uint32_t size;
} bsp_log_sd_file_info_t;

/** SDNAND / FatFs 容量信息（字节） */
typedef struct {
    uint64_t card_bytes;   /**< 芯片标称容量（LogBlockNbr * LogBlockSize） */
    uint64_t total_bytes;  /**< 文件系统总容量 */
    uint64_t free_bytes;   /**< 文件系统剩余 */
} bsp_log_sd_space_t;

/**
 * @brief  启动 SD 日志任务（挂载 + 告警边沿写 CSV）
 * @note   在 FreeRTOS 调度后调用一次
 */
void bsp_log_sd_start(void);

/**
 * @brief  SD 是否已挂载成功
 */
uint8_t bsp_log_sd_is_ready(void);

/**
 * @brief  若尚未挂载则立即尝试挂载（可从 logXfer 等 Normal 任务调用）
 * @retval 0 已就绪；负值失败
 */
int bsp_log_sd_ensure_mounted(void);

/**
 * @brief  查询 SDNAND/FatFs 容量
 * @retval 0 成功；负值失败
 */
int bsp_log_sd_get_space(bsp_log_sd_space_t *out);

/**
 * @brief  追加一条异常/告警日志（线程安全）
 * @param  level    如 "ALARM"/"WARN"/"INFO"/"CLEAR"
 * @param  code     如 "WATER"/"TEMP"/"HUMI"
 * @param  message  简短描述
 * @retval 0 成功；负值失败
 */
int bsp_log_sd_write(const char *level, const char *code, const char *message);

/**
 * @brief  列出 log 目录下的文件（供以太网传输）
 * @param  out      输出数组
 * @param  max_out  容量
 * @retval 文件个数；负值失败
 */
int bsp_log_sd_list(bsp_log_sd_file_info_t *out, size_t max_out);

/**
 * @brief  打开指定日志文件只读（文件名不含路径）
 * @retval 0 成功；负值失败
 */
int bsp_log_sd_open_read(const char *name, uint32_t *out_size);

/**
 * @brief  从已打开文件读取一块
 * @retval 实际读取字节数；0 表示 EOF；负值失败
 */
int bsp_log_sd_read(void *buf, uint32_t len);

/**
 * @brief  关闭当前只读文件
 */
void bsp_log_sd_close_read(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LOG_SD_H */
