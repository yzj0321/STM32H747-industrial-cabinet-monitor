/**
 * @file    bsp_w5500_net.h
 * @brief   W5500 芯片初始化与网络参数
 */
#ifndef BSP_W5500_NET_H
#define BSP_W5500_NET_H

#include <stdint.h>

extern uint8_t g_w5500_board_ip[4];
extern uint8_t g_w5500_gateway_ip[4];

/**
 * @brief  初始化 W5500 硬件与网络参数
 * @retval 0 成功
 * @retval -1 失败
 */
int bsp_w5500_net_init(void);

/**
 * @brief  查询以太网 PHY 链路是否 up
 * @retval 1 链路 up
 * @retval 0 链路 down 或查询失败
 */
uint8_t bsp_w5500_net_phy_link_up(void);

/**
 * @brief  获取本机 IP 字符串
 * @param  buf  输出缓冲区
 * @param  len  缓冲区长度
 * @retval 0 成功
 * @retval -1 失败
 */
int bsp_w5500_net_get_ip_str(char *buf, uint32_t len);

#endif /* BSP_W5500_NET_H */
