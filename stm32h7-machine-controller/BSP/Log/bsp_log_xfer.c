/**
 * @file    bsp_log_xfer.c
 * @brief   W5500 TCP Server：供 QT 列目录/下载 SD 异常日志
 */
#include "bsp_log_xfer.h"

#include "bsp_log.h"
#include "bsp_log_sd.h"
#include "bsp_ota_app.h"
#include "bsp_w5500_app.h"
#include "bsp_w5500_conf.h"
#include "bsp_w5500_net.h"
#include "main.h"
#include "socket.h"
#include "w5500.h"

#include "cmsis_os2.h"

#include <stdio.h>
#include <string.h>

#define XFER_TAG            "LOG-XFER"
#define XFER_SOCKET         BSP_W5500_LOG_SOCKET
#define XFER_PORT           BSP_W5500_LOG_PORT
#define XFER_RX_MAX         256U
#define XFER_TX_CHUNK       512U
#define XFER_SEND_TIMEOUT   3000U

static osThreadId_t s_task;

/**
 * @brief  日志导出发送原始数据
 */
static uint8_t xfer_send(uint8_t sn, const void *data, uint16_t len)
{
    uint32_t start = HAL_GetTick();
    const uint8_t *p = (const uint8_t *)data;
    uint16_t left = len;

    while (left > 0U) {
        uint16_t free_sz;
        uint16_t chunk;
        uint8_t ir;

        if (getSn_SR(sn) != SOCK_ESTABLISHED) {
            return 0U;
        }
        if ((HAL_GetTick() - start) >= XFER_SEND_TIMEOUT) {
            return 0U;
        }

        free_sz = getSn_TX_FSR(sn);
        if (free_sz == 0U) {
            osDelay(1);
            continue;
        }
        chunk = (left < free_sz) ? left : free_sz;
        if (chunk > XFER_TX_CHUNK) {
            chunk = XFER_TX_CHUNK;
        }

        wiz_send_data(sn, (uint8_t *)p, chunk);
        setSn_CR(sn, Sn_CR_SEND);
        while (getSn_CR(sn) != 0U) {
            if ((HAL_GetTick() - start) >= XFER_SEND_TIMEOUT) {
                return 0U;
            }
        }

        ir = getSn_IR(sn);
        if ((ir & Sn_IR_TIMEOUT) != 0U) {
            setSn_IR(sn, Sn_IR_TIMEOUT);
            return 0U;
        }
        if ((ir & Sn_IR_SENDOK) != 0U) {
            setSn_IR(sn, Sn_IR_SENDOK);
        }

        p += chunk;
        left = (uint16_t)(left - chunk);
    }
    return 1U;
}

/**
 * @brief  日志导出发送字符串
 */
static uint8_t xfer_send_str(uint8_t sn, const char *s)
{
    if (s == NULL) {
        return 0U;
    }
    return xfer_send(sn, s, (uint16_t)strlen(s));
}

/**
 * @brief  日志导出接收一行命令
 */
static int xfer_recv_line(uint8_t sn, char *buf, size_t buflen, uint32_t timeout_ms)
{
    size_t pos = 0U;
    uint32_t start = HAL_GetTick();

    if ((buf == NULL) || (buflen < 2U)) {
        return -1;
    }

    while (pos < (buflen - 1U)) {
        int32_t avail;
        uint8_t ch;
        int32_t got;

        if (getSn_SR(sn) != SOCK_ESTABLISHED) {
            return -2;
        }
        if ((HAL_GetTick() - start) >= timeout_ms) {
            return -3;
        }

        avail = getSn_RX_RSR(sn);
        if (avail <= 0) {
            osDelay(5);
            continue;
        }

        got = recv(sn, &ch, 1);
        if (got <= 0) {
            osDelay(1);
            continue;
        }
        if ((ch == '\r') || (ch == '\n')) {
            if (pos == 0U) {
                continue;
            }
            buf[pos] = '\0';
            return (int)pos;
        }
        buf[pos++] = (char)ch;
    }
    buf[buflen - 1U] = '\0';
    return -4;
}

/**
 * @brief  处理 LIST 命令
 */
static void handle_list(uint8_t sn)
{
    bsp_log_sd_file_info_t files[BSP_LOG_SD_MAX_FILES];
    char line[96];
    int n;
    int i;

    if (bsp_log_sd_ensure_mounted() != 0) {
        (void)xfer_send_str(sn, "ERR 2 list_failed\n");
        LOG_W(XFER_TAG, "LIST: SD not mounted");
        return;
    }

    n = bsp_log_sd_list(files, BSP_LOG_SD_MAX_FILES);
    if (n < 0) {
        (void)xfer_send_str(sn, "ERR 2 list_failed\n");
        return;
    }

    (void)snprintf(line, sizeof(line), "LIST %d\n", n);
    if (xfer_send_str(sn, line) == 0U) {
        return;
    }
    for (i = 0; i < n; i++) {
        (void)snprintf(line, sizeof(line), "%s\t%lu\n",
                       files[i].name, (unsigned long)files[i].size);
        if (xfer_send_str(sn, line) == 0U) {
            return;
        }
    }
    (void)xfer_send_str(sn, "END\n");
}

/**
 * @brief  处理 GET 命令
 */
static void handle_get(uint8_t sn, const char *name)
{
    char hdr[96];
    uint8_t chunk[XFER_TX_CHUNK];
    uint32_t size = 0U;
    uint32_t sent = 0U;
    int rd;

    if ((name == NULL) || (name[0] == '\0')) {
        (void)xfer_send_str(sn, "ERR 3 bad_name\n");
        return;
    }

    if (bsp_log_sd_open_read(name, &size) != 0) {
        if (bsp_log_sd_ensure_mounted() != 0) {
            (void)xfer_send_str(sn, "ERR 4 open_failed\n");
            return;
        }
        if (bsp_log_sd_open_read(name, &size) != 0) {
            (void)xfer_send_str(sn, "ERR 4 open_failed\n");
            return;
        }
    }

    (void)snprintf(hdr, sizeof(hdr), "FILE %s %lu\n", name, (unsigned long)size);
    if (xfer_send_str(sn, hdr) == 0U) {
        bsp_log_sd_close_read();
        return;
    }

    while (sent < size) {
        uint32_t want = size - sent;
        if (want > sizeof(chunk)) {
            want = sizeof(chunk);
        }
        rd = bsp_log_sd_read(chunk, want);
        if (rd <= 0) {
            break;
        }
        if (xfer_send(sn, chunk, (uint16_t)rd) == 0U) {
            break;
        }
        sent += (uint32_t)rd;
    }
    bsp_log_sd_close_read();
}

/**
 * @brief  分发日志导出命令
 */
static void handle_command(uint8_t sn, char *line)
{
    char *arg;

    if (strcmp(line, "PING") == 0) {
        (void)xfer_send_str(sn, "PONG\n");
        return;
    }
    if (strcmp(line, "STAT") == 0) {
        char resp[48];
        (void)bsp_log_sd_ensure_mounted();
        (void)snprintf(resp, sizeof(resp), "STAT mounted=%u\n",
                       (unsigned)bsp_log_sd_is_ready());
        (void)xfer_send_str(sn, resp);
        return;
    }
    if (strcmp(line, "SPACE") == 0) {
        bsp_log_sd_space_t sp;
        char resp[128];
        if (bsp_log_sd_get_space(&sp) != 0) {
            (void)xfer_send_str(sn, "ERR 5 space_failed\n");
            return;
        }
        (void)snprintf(resp, sizeof(resp),
                       "SPACE card=%lu total=%lu free=%lu\n",
                       (unsigned long)sp.card_bytes,
                       (unsigned long)sp.total_bytes,
                       (unsigned long)sp.free_bytes);
        (void)xfer_send_str(sn, resp);
        return;
    }
    if (strcmp(line, "LIST") == 0) {
        handle_list(sn);
        return;
    }
    if (strncmp(line, "GET ", 4) == 0) {
        arg = line + 4;
        while (*arg == ' ') {
            arg++;
        }
        handle_get(sn, arg);
        return;
    }
    (void)xfer_send_str(sn, "ERR 1 unknown_cmd\n");
}

/**
 * @brief  解析 IPv4 字符串
 */
static uint8_t parse_ipv4(const char *host, uint8_t out[4])
{
    unsigned a = 0U, b = 0U, c = 0U, d = 0U;
    char tail = 0;

    if ((host == NULL) || (out == NULL)) {
        return 0U;
    }
    if (sscanf(host, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) < 4) {
        return 0U;
    }
    if ((a > 255U) || (b > 255U) || (c > 255U) || (d > 255U)) {
        return 0U;
    }
    out[0] = (uint8_t)a;
    out[1] = (uint8_t)b;
    out[2] = (uint8_t)c;
    out[3] = (uint8_t)d;
    return 1U;
}

/**
 * @brief  非阻塞式连接上位机日志 TCP Server（轮询 + osDelay，避免饿死其它任务）
 * @retval 0 成功
 * @retval <0 失败
 */
static int client_connect(void)
{
    uint8_t dest[4];
    int8_t ret;
    uint32_t start;
    uint8_t sr;

    if (parse_ipv4(BSP_W5500_OTA_HOST, dest) == 0U) {
        LOG_E(XFER_TAG, "bad host %s", BSP_W5500_OTA_HOST);
        return -1;
    }

    LOG_I(XFER_TAG, "try connect %u.%u.%u.%u:%u sock=%u",
          (unsigned)dest[0], (unsigned)dest[1], (unsigned)dest[2], (unsigned)dest[3],
          (unsigned)XFER_PORT, (unsigned)XFER_SOCKET);

    (void)close(XFER_SOCKET);
    /* 非阻塞：避免 WIZnet connect() 内部空转把同级任务饿死 */
    ret = socket(XFER_SOCKET, Sn_MR_TCP, 0U, SF_IO_NONBLOCK);
    if (ret != (int8_t)XFER_SOCKET) {
        LOG_E(XFER_TAG, "socket() failed ret=%d sr=0x%02X",
              (int)ret, (unsigned)getSn_SR(XFER_SOCKET));
        return -2;
    }

    ret = connect(XFER_SOCKET, dest, XFER_PORT);
    /* 非阻塞模式下立即返回 SOCK_BUSY(0) 或已成功 */
    if ((ret != SOCK_OK) && (ret != SOCK_BUSY)) {
        sr = getSn_SR(XFER_SOCKET);
        LOG_W(XFER_TAG, "connect() failed ret=%d sr=0x%02X",
              (int)ret, (unsigned)sr);
        (void)close(XFER_SOCKET);
        return -3;
    }

    start = HAL_GetTick();
    for (;;) {
        sr = getSn_SR(XFER_SOCKET);
        if (sr == SOCK_ESTABLISHED) {
            LOG_I(XFER_TAG, "TCP ESTABLISHED -> %s:%u",
                  BSP_W5500_OTA_HOST, (unsigned)XFER_PORT);
            return 0;
        }
        if ((sr == SOCK_CLOSED) || ((getSn_IR(XFER_SOCKET) & Sn_IR_TIMEOUT) != 0U)) {
            setSn_IR(XFER_SOCKET, Sn_IR_TIMEOUT);
            LOG_W(XFER_TAG, "connect aborted sr=0x%02X (QT listen :%u ?)",
                  (unsigned)sr, (unsigned)XFER_PORT);
            (void)close(XFER_SOCKET);
            return -4;
        }
        if ((HAL_GetTick() - start) >= BSP_W5500_TCP_CONNECT_TIMEOUT_MS) {
            LOG_W(XFER_TAG, "connect timeout %ums sr=0x%02X (QT must listen :%u)",
                  (unsigned)BSP_W5500_TCP_CONNECT_TIMEOUT_MS,
                  (unsigned)sr, (unsigned)XFER_PORT);
            (void)close(XFER_SOCKET);
            return -5;
        }
        osDelay(50);
    }
}

/**
 * @brief  日志导出 TCP 任务
 */
static void LogXferTask(void *argument)
{
    char line[XFER_RX_MAX];
    uint32_t wait_net_log_ms = 0U;
    uint32_t fail_count = 0U;

    (void)argument;
    /* 首行立刻打印：用于确认任务已真正跑起来（不被 SD 挂载饿死） */
    LOG_I(XFER_TAG, "task RUNNING, target %s:%u",
          BSP_W5500_OTA_HOST, (unsigned)XFER_PORT);
    osDelay(500);

    for (;;) {
        bsp_w5500_state_t st = bsp_w5500_get_state();
        uint8_t phy = bsp_w5500_net_phy_link_up();

        if (BSP_Ota_IsActive() != 0U) {
            if ((HAL_GetTick() - wait_net_log_ms) >= 3000U) {
                wait_net_log_ms = HAL_GetTick();
                LOG_I(XFER_TAG, "pause: OTA active");
            }
            osDelay(500);
            continue;
        }

        if ((st == BSP_W5500_STATE_OFF) ||
            (st == BSP_W5500_STATE_BOOT) ||
            (st == BSP_W5500_STATE_NET_ERROR) ||
            (phy == 0U)) {
            if ((HAL_GetTick() - wait_net_log_ms) >= 3000U) {
                wait_net_log_ms = HAL_GetTick();
                LOG_W(XFER_TAG, "wait net: state=%d phy=%u", (int)st, (unsigned)phy);
            }
            osDelay(500);
            continue;
        }

        {
            int rc = client_connect();
            if (rc != 0) {
                fail_count++;
                LOG_W(XFER_TAG, "connect fail #%lu rc=%d, retry %ums",
                      (unsigned long)fail_count, rc,
                      (unsigned)BSP_W5500_TCP_RETRY_MS);
                osDelay(BSP_W5500_TCP_RETRY_MS);
                continue;
            }
        }

        fail_count = 0U;
        LOG_I(XFER_TAG, "log link UP, waiting QT commands (LIST/GET/STAT)");

        for (;;) {
            int n;

            if (BSP_Ota_IsActive() != 0U) {
                LOG_I(XFER_TAG, "drop link: OTA active");
                break;
            }
            if (getSn_SR(XFER_SOCKET) != SOCK_ESTABLISHED) {
                LOG_W(XFER_TAG, "drop link: sr=0x%02X",
                      (unsigned)getSn_SR(XFER_SOCKET));
                break;
            }

            n = xfer_recv_line(XFER_SOCKET, line, sizeof(line), 5000U);
            if (n == -3) {
                continue;
            }
            if (n < 0) {
                LOG_W(XFER_TAG, "recv line err=%d sr=0x%02X",
                      n, (unsigned)getSn_SR(XFER_SOCKET));
                break;
            }
            LOG_I(XFER_TAG, "cmd: %s", line);
            handle_command(XFER_SOCKET, line);
        }

        (void)disconnect(XFER_SOCKET);
        (void)close(XFER_SOCKET);
        LOG_I(XFER_TAG, "log link CLOSED, retry soon");
        osDelay(BSP_W5500_TCP_RETRY_MS);
    }
}

/**
 * @brief  启动日志导出任务
 */
void bsp_log_xfer_start(void)
{
    const osThreadAttr_t attr = {
        .name = "logXfer",
        /* SD 挂载可能长时间占住 BelowNormal；日志连接用 Normal 避免饿死 */
        .stack_size = 2048U * 4U,
        .priority = (osPriority_t)osPriorityNormal,
    };

    if (s_task != NULL) {
        return;
    }
    s_task = osThreadNew(LogXferTask, NULL, &attr);
    if (s_task == NULL) {
        LOG_E(XFER_TAG, "osThreadNew(logXfer) failed");
    } else {
        LOG_I(XFER_TAG, "logXfer thread created (prio=Normal)");
    }
}
