/**
 * @file    bsp_w5500_net.c
 * @brief   W5500 芯片初始化与静态网络配置
 */
#include "bsp_w5500_net.h"

#include "bsp_w5500_conf.h"
#include "bsp_log.h"
#include "wizchip_conf.h"
#include "w5500.h"

#include <stdio.h>
#include <string.h>

#define W5500_NET_TAG   "W5500-NET"

uint8_t g_w5500_board_ip[4] = {
    BSP_W5500_BOARD_IP0, BSP_W5500_BOARD_IP1,
    BSP_W5500_BOARD_IP2, BSP_W5500_BOARD_IP3
};

uint8_t g_w5500_gateway_ip[4] = {
    BSP_W5500_GATEWAY_IP0, BSP_W5500_GATEWAY_IP1,
    BSP_W5500_GATEWAY_IP2, BSP_W5500_GATEWAY_IP3
};

/**
 * @brief  初始化 W5500 网络
 */
int bsp_w5500_net_init(void)
{
    uint8_t version;
    uint8_t phy_link = PHY_LINK_OFF;

    W5500_ChipInit();

    version = getVERSIONR();
    LOG_I(W5500_NET_TAG, "version=0x%02X", version);
    if (version != 0x04U) {
        LOG_E(W5500_NET_TAG, "version check failed, SPI/wiring issue?");
        return -1;
    }

    if (ctlwizchip(CW_GET_PHYLINK, (void *)&phy_link) == 0) {
        LOG_I(W5500_NET_TAG, "PHY link %s", (phy_link == PHY_LINK_ON) ? "up" : "down");
    }

    /* 加长 TCP 重传：默认 200ms×8 在 PC 短暂卡住时易 TCPTO 关 socket */
    {
        wiz_NetTimeout tmo;
        tmo.retry_cnt = 8U;
        tmo.time_100us = 3000U; /* 300ms */
        (void)ctlnetwork(CN_SET_TIMEOUT, (void *)&tmo);
    }

    return 0;
}

/**
 * @brief  查询 PHY 链路状态
 */
uint8_t bsp_w5500_net_phy_link_up(void)
{
    uint8_t phy_link = PHY_LINK_OFF;

    if (ctlwizchip(CW_GET_PHYLINK, (void *)&phy_link) != 0) {
        return 0U;
    }

    return (phy_link == PHY_LINK_ON) ? 1U : 0U;
}

/**
 * @brief  获取本机 IP 字符串
 */
int bsp_w5500_net_get_ip_str(char *buf, uint32_t len)
{
    if ((buf == NULL) || (len == 0U)) {
        return -1;
    }

    (void)snprintf(buf, len, "%u.%u.%u.%u",
                   (unsigned)g_w5500_board_ip[0],
                   (unsigned)g_w5500_board_ip[1],
                   (unsigned)g_w5500_board_ip[2],
                   (unsigned)g_w5500_board_ip[3]);
    return 0;
}
