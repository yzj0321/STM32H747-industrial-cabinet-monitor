/**
 * @file    bsp_log_xfer.h
 * @brief   以太网日志文件服务（与 OTA 同模型：上位机监听，板子主动连）
 *
 * 设备作为 TCP Client：连接 BSP_W5500_OTA_HOST:5001（socket=1）
 * 上位机作为 TCP Server 监听 5001，连上后发文本命令拉取 SD 日志。
 *
 * 命令（以 \\n 结尾）：
 *   PING / STAT / LIST / GET alarms.csv
 */
#ifndef BSP_LOG_XFER_H
#define BSP_LOG_XFER_H

#ifdef __cplusplus
extern "C" {
#endif

/** 启动日志文件 TCP 客户端任务（在 W5500 启动后调用） */
void bsp_log_xfer_start(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_LOG_XFER_H */
