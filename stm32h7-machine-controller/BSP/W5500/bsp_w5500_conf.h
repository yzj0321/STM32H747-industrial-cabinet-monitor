/**
 * @file    bsp_w5500_conf.h
 * @brief   W5500 以太网模块配置（静态 IP 与 OTA 主机）
 */
#ifndef BSP_W5500_CONF_H
#define BSP_W5500_CONF_H

/**
 * 网络配置切换：
 *   0 = 192.168.1.x 网段（与 W5500 参考工程一致）
 *   1 = 192.168.71.x 网段（PC 有线接入同网段时使用）
 */
#define BSP_W5500_NETWORK_PROFILE      0

#if (BSP_W5500_NETWORK_PROFILE == 0)
#define BSP_W5500_BOARD_IP0            192
#define BSP_W5500_BOARD_IP1            168
#define BSP_W5500_BOARD_IP2            1
#define BSP_W5500_BOARD_IP3            100
#define BSP_W5500_GATEWAY_IP0          192
#define BSP_W5500_GATEWAY_IP1          168
#define BSP_W5500_GATEWAY_IP2          1
#define BSP_W5500_GATEWAY_IP3          1
#define BSP_W5500_OTA_HOST             "192.168.1.10"
#elif (BSP_W5500_NETWORK_PROFILE == 1)
#define BSP_W5500_BOARD_IP0            192
#define BSP_W5500_BOARD_IP1            168
#define BSP_W5500_BOARD_IP2            71
#define BSP_W5500_BOARD_IP3            100
#define BSP_W5500_GATEWAY_IP0          192
#define BSP_W5500_GATEWAY_IP1          168
#define BSP_W5500_GATEWAY_IP2          71
#define BSP_W5500_GATEWAY_IP3          1
#define BSP_W5500_OTA_HOST             "192.168.71.63"
#else
#error "Invalid BSP_W5500_NETWORK_PROFILE"
#endif

#define BSP_W5500_OTA_PORT             5000U
#define BSP_W5500_OTA_SOCKET           0U
/** 日志拉取：上位机 TCP Server 端口；板子用 socket1 主动连接（与 OTA 同模型） */
#define BSP_W5500_LOG_PORT             5001U
#define BSP_W5500_LOG_SOCKET           1U

#define BSP_W5500_PHY_WAIT_MS          5000U
#define BSP_W5500_TCP_CONNECT_TIMEOUT_MS 15000U
#define BSP_W5500_TCP_CLOSE_TIMEOUT_MS   1000U
#define BSP_W5500_TCP_RETRY_MS         5000U
#define BSP_W5500_LINK_STABLE_MS       1000U
#define BSP_W5500_TASK_STACK_WORDS     1024U

/** 大包发送超时 */
#define BSP_W5500_OTA_SEND_TIMEOUT_MS  5000U
/** DATA ACK / 小帧 SENDOK 等待上限 */
#define BSP_W5500_OTA_ACK_TRY_MS        800U
/** 0=OTA 中禁止 TCP 重连 */
#define BSP_W5500_OTA_ACK_RECOVER_FAILS 0U
#define BSP_W5500_SEND_RETRIES          3U

#define BSP_W5500_TCP_RX_CHUNK          4096U
#define BSP_W5500_TCP_RX_SEM_MAX        32U

#endif /* BSP_W5500_CONF_H */
