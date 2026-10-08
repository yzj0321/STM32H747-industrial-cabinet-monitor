/**
 * @file    bsp_ota_manager.c
 * @brief   OTA 会话状态机与 SOIP 命令调度实现
 *
 * 典型 OTA 时序（上位机 -> 设备）：
 *   PING / GET_INFO          探测与获取 inactive 槽基址
 *   BEGIN_SESSION            交换 session_token，选定 Slot B 为目标
 *   ERASE                    擦除目标槽（含共享扇区保护）
 *   START_TRANSFER           记录 image_size / crc32
 *   DATA (多次)              按 offset 写入 Flash；同步回 ACK（停等）
 *   END_TRANSFER             确认 written == size
 *   VERIFY                   CRC 校验 + EEPROM pending，成功后 MCU 自动复位
 *   RESET / ABORT            手动复位或放弃会话
 *
 * 状态机：IDLE -> SESSION -> ERASING -> RECEIVING -> READY_VERIFY -> IDLE
 *
 * EEPROM 时序对齐 CamView：ERASE/DATA 只动 Flash；pending 仅在 VERIFY 成功后写入。
 */
#include "bsp_ota_manager.h"

#include <string.h>

#include "bsp_log.h"
#include "bsp_w5500_conf.h"
#include "bsp_ota_conf.h"
#include "bsp_ota_coop.h"
#include "bsp_ota_crc.h"
#include "bsp_ota_flash.h"
#include "bsp_ota_port.h"
#include "bsp_ota_protocol.h"
#include "bsp_ota_transport.h"
#include "bsp_watchdog.h"
#include "bsp_eeprom.h"
#include "boot_eeprom.h"
#include "boot_slot.h"
#include "cmsis_os.h"
#include "stm32h7xx_hal.h"

/** 日志标签 */
#define OTA_TAG                     "OTA"

/** OTA 目标核 */
typedef enum {
    OTA_CORE_CM7 = 0,
    OTA_CORE_CM4 = 1,
} ota_core_t;

#define OTA_CM4_HOLD_MS             (3000U)

/** OTA 状态机各阶段 */
typedef enum {
    OTA_STATE_IDLE = 0,         /**< 空闲，无活动会话 */
    OTA_STATE_SESSION,          /**< 会话已建立，等待擦除/传输 */
    OTA_STATE_ERASING,          /**< 正在擦除目标槽位 */
    OTA_STATE_RECEIVING,        /**< 正在接收固件数据 */
    OTA_STATE_READY_VERIFY      /**< 数据接收完毕，等待校验 */
} ota_state_t;

/** OTA 会话是否活动（用于协作式调度） */
static volatile uint8_t s_ota_active;
/** 当前状态机状态 */
static ota_state_t s_state = OTA_STATE_IDLE;
/** 当前会话令牌 */
static uint32_t s_session_token;
/** OTA 目标槽位索引 */
static uint8_t s_target_slot;
/** 当前 OTA 目标核 */
static ota_core_t s_ota_core = OTA_CORE_CM7;
/** CM4 OTA 停核是否已建立 */
static uint8_t s_cm4_held;
/** 固件镜像总大小 */
static uint32_t s_image_size;
/** 固件镜像 CRC32 */
static uint32_t s_image_crc32;
/** 已写入字节数 */
static uint32_t s_bytes_written;

/** SOIP 接收缓冲区 */
static uint8_t s_rx_buf[BSP_OTA_RX_BUF_SIZE];
/** 接收缓冲区当前长度 */
static uint16_t s_rx_len;
/** SOIP 发送缓冲区 */
static uint8_t s_tx_buf[BSP_OTA_RX_BUF_SIZE];
/** DATA 应答是否待发送 */
static volatile uint8_t s_data_ack_pending;
/** 待发送 DATA 应答的序列号 */
static uint16_t s_data_ack_seq;
/** DATA 应答连续失败计数 */
static uint16_t s_data_ack_fail_streak;
/** 已接收 DATA 包计数 */
static uint32_t s_data_rx_count;

/**
 * @brief  根据主机随机数与芯片 UID 生成设备会话令牌
 * @param  host_nonce 主机下发的随机数
 * @retval 设备会话令牌
 */
static uint32_t ota_device_token(uint32_t host_nonce)
{
    uint32_t uid0 = HAL_GetUIDw0();
    uint32_t uid1 = HAL_GetUIDw1();
    uint32_t uid2 = HAL_GetUIDw2();
    uint32_t mix = host_nonce ^ uid0 ^ uid1 ^ uid2;

    /* 混合 UID 与主机随机数后做 CRC，使令牌与设备绑定且不可预测 */
    return bsp_ota_crc32((const uint8_t *)&mix, sizeof(mix), 0xFFFFFFFFU);
}

/**
 * @brief  构建并发送 SOIP 应答帧
 * @param  cmd       命令字
 * @param  seq       序列号
 * @param  status    应答状态码
 * @param  extra     附加载荷（可为 NULL）
 * @param  extra_len 附加载荷长度
 */
static void ota_send_response(bsp_ota_cmd_t cmd, uint16_t seq,
                              bsp_ota_status_t status, const uint8_t *extra, uint16_t extra_len)
{
    uint8_t payload[64];
    uint16_t payload_len = 1U;
    uint16_t frame_len;

    payload[0] = (uint8_t)status;
    if ((extra != NULL) && (extra_len > 0U)) {
        if ((1U + extra_len) > sizeof(payload)) {
            return;
        }
        /* 应答载荷格式：[status(1B)] [extra...]，如 BEGIN_SESSION 附带 4 字节 token */
        memcpy(&payload[1], extra, extra_len);
        payload_len = (uint16_t)(1U + extra_len);
    }

    /* 应答帧 cmd 需置 RESPONSE 位，seq 与请求帧保持一致 */
    frame_len = bsp_ota_build_frame(s_tx_buf, sizeof(s_tx_buf),
                                    (bsp_ota_cmd_t)((uint8_t)cmd | BSP_OTA_CMD_RESPONSE_MASK),
                                    seq, payload, payload_len);
    if (frame_len == 0U) {
        return;
    }

    {
        uint8_t attempt;
        int write_ok = -1;

        for (attempt = 0U; attempt < 3U; attempt++) {
            bsp_ota_transport_poll();
            write_ok = bsp_ota_transport_write(s_tx_buf, frame_len);
            if (write_ok == 0) {
                break;
            }
            /* CIPSEND 可能短暂阻塞，轮询收包并延时后重试 */
            osDelay(30U);
        }

        if (write_ok != 0) {
            LOG_E(OTA_TAG, "reply send failed cmd=0x%02X seq=%u len=%u",
                  (unsigned int)cmd, (unsigned int)seq, (unsigned int)frame_len);
        }
#if (BSP_OTA_SOIP_VERBOSE != 0U)
        else {
            LOG_I(OTA_TAG, "reply sent cmd=0x%02X seq=%u len=%u",
                  (unsigned int)((uint8_t)cmd | BSP_OTA_CMD_RESPONSE_MASK),
                  (unsigned int)seq, (unsigned int)frame_len);
        }
#endif
    }
}

/**
 * @brief  同步发送 DATA ACK（停等核心：写完 Flash 必须立刻回包）
 * @retval 1 成功，0 失败（留下 pending 供 poll 补发）
 */
static uint8_t ota_send_data_ack_now(uint16_t seq)
{
    uint8_t payload[1];
    uint16_t frame_len;
    uint8_t attempt;

    payload[0] = (uint8_t)BSP_OTA_STATUS_OK;
    frame_len = bsp_ota_build_frame(s_tx_buf, sizeof(s_tx_buf),
                                    (bsp_ota_cmd_t)(BSP_OTA_CMD_DATA | BSP_OTA_CMD_RESPONSE_MASK),
                                    seq, payload, 1U);
    if (frame_len == 0U) {
        return 0U;
    }

    for (attempt = 0U; attempt < 5U; attempt++) {
        bsp_ota_transport_poll();
        if (bsp_ota_transport_write(s_tx_buf, frame_len) == 0) {
            s_data_ack_pending = 0U;
            s_data_ack_fail_streak = 0U;
            return 1U;
        }
        osDelay(2);
    }

    /* 失败则挂起，后续 poll 补发，避免上位机永久等 ACK */
    s_data_ack_seq = seq;
    s_data_ack_pending = 1U;
    s_data_ack_fail_streak++;
    LOG_W(OTA_TAG, "DATA ack defer seq=%u streak=%u",
          (unsigned int)seq, (unsigned int)s_data_ack_fail_streak);
    return 0U;
}

/**
 * @brief  刷新并发送 OTA DATA ACK
 */
static uint8_t ota_flush_data_ack(void)
{
    if (s_data_ack_pending == 0U) {
        return 1U;
    }
    return ota_send_data_ack_now(s_data_ack_seq);
}

/**
 * @brief  设置 OTA 活动状态，并协调其他任务挂起/恢复
 * @param  active 1 进入活动，0 退出活动
 */
static void ota_set_active(uint8_t active)
{
    if (active != 0U) {
        if (s_ota_active == 0U) {
            /* OTA 期间挂起 LVGL/Modbus 等任务，避免 Flash 擦写与 UI 争用 CPU */
            bsp_ota_coop_suspend_peers();
            /* OTA/Flash 长操作期间仅监控 W5500 任务心跳 */
            BSP_Watchdog_SetLongOp(1U);
        }
        s_ota_active = 1U;
        return;
    }

    if (s_ota_active != 0U) {
        /* 会话结束：恢复 LVGL/Modbus/ADC 等被挂起的任务 */
        bsp_ota_coop_resume_peers();
        /* 取消长操作豁免，恢复全任务心跳监控 */
        BSP_Watchdog_SetLongOp(0U);
    }
    s_ota_active = 0U;
}

/**
 * @brief  获取 OTA 槽基址
 */
static uint32_t ota_slot_base(uint8_t slot_index)
{
    if (s_ota_core == OTA_CORE_CM4) {
        return cm4_boot_slot_base(slot_index);
    }

    return boot_slot_base(slot_index);
}

/**
 * @brief  获取 OTA 槽大小
 */
static uint32_t ota_slot_size(void)
{
    return (s_ota_core == OTA_CORE_CM4) ? CM4_APP_SLOT_SIZE : CM7_APP_SLOT_SIZE;
}

/**
 * @brief  获取当前运行槽
 */
static uint8_t ota_running_slot(const boot_eeprom_meta_t *ee_meta)
{
    if (s_ota_core == OTA_CORE_CM4) {
        uint8_t running = boot_eeprom_select_cm4_boot_slot(ee_meta);

        if (running >= CM4_APP_SLOT_COUNT) {
            running = CM4_APP_SLOT_A_INDEX;
        }
        return running;
    }

    return boot_slot_detect_running();
}

/**
 * @brief  获取非活动槽
 */
static uint8_t ota_inactive_slot(uint8_t running)
{
    if (s_ota_core == OTA_CORE_CM4) {
        return cm4_boot_slot_inactive_index(running);
    }

    return boot_slot_inactive_index(running);
}

/**
 * @brief  判断 OTA 槽是否合法
 */
static int ota_slot_is_valid(uint8_t slot_index)
{
    if (s_ota_core == OTA_CORE_CM4) {
        return cm4_boot_slot_is_valid(slot_index);
    }

    return boot_slot_is_valid(slot_index);
}

/**
 * @brief  异常终止 OTA 会话并发送 ABORT 应答
 * @param  status 终止原因状态码
 */
static void ota_abort_session(bsp_ota_status_t status)
{
    /* 复位全部会话上下文，确保下次 BEGIN_SESSION 从干净状态开始 */
    s_state = OTA_STATE_IDLE;
    s_session_token = 0U;
    s_image_size = 0U;
    s_image_crc32 = 0U;
    s_bytes_written = 0U;
    s_data_ack_pending = 0U;
    if (s_cm4_held != 0U) {
        bsp_ota_port_release_cm4_ota();
        s_cm4_held = 0U;
    }
    ota_set_active(0U);
    bsp_ota_port_exit();
    LOG_W(OTA_TAG, "session aborted status=%u", (unsigned int)status);
    /* 主动通知上位机会话已终止（seq=0 表示非针对某一请求的应答） */
    ota_send_response(BSP_OTA_CMD_ABORT, 0U, status, NULL, 0U);
}

/**
 * @brief  处理 PING 命令
 * @param  seq 序列号
 */
static void ota_handle_ping(uint16_t seq)
{
    ota_send_response(BSP_OTA_CMD_PING, seq, BSP_OTA_STATUS_OK, NULL, 0U);
}

/**
 * @brief  处理 GET_INFO 命令，返回设备信息与 OTA 目标槽位
 * @param  seq 序列号
 */
static void ota_handle_get_info(const bsp_ota_frame_t *frame)
{
    uint8_t payload[128];
    uint16_t pos = 0U;
    boot_eeprom_meta_t ee_meta;
    uint8_t running;
    uint8_t inactive;
    const char *name = (s_ota_core == OTA_CORE_CM4) ? BSP_OTA_DEVICE_NAME_CM4
                                                     : BSP_OTA_DEVICE_NAME;
    const char *boot_ver = "APP-OTA-EEPROM";
    const char *proto = BSP_OTA_PROTO_VERSION;
    uint8_t name_len = (uint8_t)strlen(name);
    uint8_t boot_len = (uint8_t)strlen(boot_ver);
    uint8_t proto_len = (uint8_t)strlen(proto);
    uint32_t inactive_base;
    ota_core_t req_core = OTA_CORE_CM7;

    if ((frame != NULL) && (frame->payload_len >= 1U)) {
        if (frame->payload[0] == 1U) {
            req_core = OTA_CORE_CM4;
        }
    }
    s_ota_core = req_core;

    memset(&ee_meta, 0, sizeof(ee_meta));
    ee_meta.active_slot = CM7_APP_SLOT_A_INDEX;
    ee_meta.pending_slot = CM7_APP_SLOT_A_INDEX;
    ee_meta.cm4_active_slot = CM4_APP_SLOT_A_INDEX;
    ee_meta.cm4_pending_slot = CM4_APP_SLOT_A_INDEX;
    (void)boot_eeprom_load(&ee_meta);

    running = ota_running_slot(&ee_meta);
    inactive = ota_inactive_slot(running);
    inactive_base = ota_slot_base(inactive);

    LOG_I(OTA_TAG, "GET_INFO seq=%u core=%u run=%u inactive=%u base=0x%08lX",
          (unsigned int)((frame != NULL) ? frame->seq : 0U),
          (unsigned int)s_ota_core,
          (unsigned int)running,
          (unsigned int)inactive,
          (unsigned long)inactive_base);

    payload[pos++] = BSP_OTA_STATUS_OK;
    payload[pos++] = name_len;
    memcpy(&payload[pos], name, name_len);
    pos = (uint16_t)(pos + name_len);

    {
        uint32_t chip_id = HAL_GetUIDw0();
        memcpy(&payload[pos], &chip_id, 4U);
    }
    pos = (uint16_t)(pos + 4U);

    payload[pos++] = boot_len;
    memcpy(&payload[pos], boot_ver, boot_len);
    pos = (uint16_t)(pos + boot_len);

    {
        uint32_t flash_size = 1024U * 1024U;
        memcpy(&payload[pos], &flash_size, 4U);
        pos = (uint16_t)(pos + 4U);
        memcpy(&payload[pos], &inactive_base, 4U);
        pos = (uint16_t)(pos + 4U);
    }

    payload[pos++] = proto_len;
    memcpy(&payload[pos], proto, proto_len);
    pos = (uint16_t)(pos + proto_len);

    /* v2 扩展：A/B 分区状态（小端，固定 6 字节） */
    payload[pos++] = running;
    payload[pos++] = inactive;
    if (s_ota_core == OTA_CORE_CM4) {
        payload[pos++] = ee_meta.cm4_active_slot;
        payload[pos++] = ee_meta.cm4_pending_slot;
        payload[pos++] = ee_meta.cm4_flags;
        payload[pos++] = ee_meta.cm4_boot_trial;
    } else {
        payload[pos++] = ee_meta.active_slot;
        payload[pos++] = ee_meta.pending_slot;
        payload[pos++] = ee_meta.flags;
        payload[pos++] = ee_meta.boot_trial;
    }

    {
        uint16_t frame_len = bsp_ota_build_frame(s_tx_buf, sizeof(s_tx_buf),
                                                 (bsp_ota_cmd_t)(BSP_OTA_CMD_GET_INFO | BSP_OTA_CMD_RESPONSE_MASK),
                                                 (frame != NULL) ? frame->seq : 0U,
                                                 payload, pos);
        if (frame_len > 0U) {
            if (bsp_ota_transport_write(s_tx_buf, frame_len) != 0) {
                LOG_E(OTA_TAG, "GET_INFO reply send failed");
            } else {
                LOG_I(OTA_TAG, "GET_INFO reply sent (%u bytes)", (unsigned int)frame_len);
            }
        }
    }
}

/**
 * @brief  从载荷指定偏移读取 32 位无符号整数
 * @param  payload 载荷指针
 * @param  len     载荷总长度
 * @param  index   字节偏移
 * @param  value   输出值指针
 * @retval 0 成功，-1 参数无效或越界
 */
static int ota_payload_read_u32(const uint8_t *payload, uint16_t len, uint32_t index, uint32_t *value)
{
    if ((value == NULL) || ((index + 4U) > len)) {
        return -1;
    }

    memcpy(value, &payload[index], 4U);
    return 0;
}

/**
 * @brief  处理 BEGIN_SESSION 命令，建立 OTA 会话
 * @param  frame 接收到的 SOIP 帧
 */
static void ota_handle_begin_session(const bsp_ota_frame_t *frame)
{
    uint32_t host_nonce;

    if ((s_state != OTA_STATE_IDLE) && (s_state != OTA_STATE_SESSION)) {
        /* 正在擦除/接收/校验时不允许重建会话，防止 Flash 操作被打断 */
        ota_send_response(BSP_OTA_CMD_BEGIN_SESSION, frame->seq, BSP_OTA_STATUS_ERR_BUSY, NULL, 0U);
        return;
    }

    if (frame->payload_len < 4U) {
        ota_send_response(BSP_OTA_CMD_BEGIN_SESSION, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    /* 载荷前 4 字节为 host_nonce，设备据此生成 session_token 回给上位机 */
    memcpy(&host_nonce, frame->payload, 4U);
    s_session_token = ota_device_token(host_nonce);
    s_target_slot = ota_inactive_slot(ota_running_slot(&(boot_eeprom_meta_t){0}));
    {
        boot_eeprom_meta_t ee_meta;
        memset(&ee_meta, 0, sizeof(ee_meta));
        (void)boot_eeprom_load(&ee_meta);
        s_target_slot = ota_inactive_slot(ota_running_slot(&ee_meta));
    }
    bsp_ota_flash_set_target_core((s_ota_core == OTA_CORE_CM4) ? BSP_OTA_FLASH_CORE_CM4
                                                                 : BSP_OTA_FLASH_CORE_CM7);
    s_state = OTA_STATE_SESSION;
    ota_set_active(1U);
    s_image_size = 0U;
    s_image_crc32 = 0U;
    s_bytes_written = 0U;

    LOG_I(OTA_TAG, "session begin/retry core=%u target slot %u base=0x%08lX",
          (unsigned int)s_ota_core,
          (unsigned int)s_target_slot,
          (unsigned long)ota_slot_base(s_target_slot));

    ota_send_response(BSP_OTA_CMD_BEGIN_SESSION, frame->seq, BSP_OTA_STATUS_OK,
                      (const uint8_t *)&s_session_token, 4U);
}

/**
 * @brief  校验帧中的会话令牌是否有效
 * @param  frame      SOIP 帧指针
 * @param  token_out  输出令牌（可为 NULL）
 * @retval 0 校验通过，-1 无效
 */
static int ota_check_session(const bsp_ota_frame_t *frame, uint32_t *token_out)
{
    uint32_t token;

    if (s_state == OTA_STATE_IDLE) {
        return -1;
    }

    if (frame->payload_len < 4U) {
        return -1;
    }

    memcpy(&token, frame->payload, 4U);
    /* 后续命令载荷首 4 字节必须是 BEGIN_SESSION 返回的 session_token */
    if (token != s_session_token) {
        return -1;
    }

    if (token_out != NULL) {
        *token_out = token;
    }

    return 0;
}

/**
 * @brief  处理 ERASE 命令，擦除目标槽位 Flash
 * @param  frame SOIP 帧指针
 */
static void ota_handle_erase(const bsp_ota_frame_t *frame)
{
    uint32_t flash_addr;
    uint32_t size;

    LOG_I(OTA_TAG, "ERASE request seq=%u state=%u target=%u",
          (unsigned int)frame->seq,
          (unsigned int)s_state,
          (unsigned int)s_target_slot);

    if (ota_check_session(frame, NULL) != 0) {
        LOG_W(OTA_TAG, "ERASE rejected: bad session");
        ota_send_response(BSP_OTA_CMD_ERASE, frame->seq, BSP_OTA_STATUS_ERR_SESSION, NULL, 0U);
        return;
    }

    /* ERASE 载荷：[token:4][flash_addr:4][size:4]，共 12 字节起 */
    if ((ota_payload_read_u32(frame->payload, frame->payload_len, 4U, &flash_addr) != 0) ||
        (ota_payload_read_u32(frame->payload, frame->payload_len, 8U, &size) != 0)) {
        LOG_W(OTA_TAG, "ERASE rejected: bad payload len=%u", (unsigned int)frame->payload_len);
        ota_send_response(BSP_OTA_CMD_ERASE, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    LOG_I(OTA_TAG, "ERASE param addr=0x%08lX size=%lu expect=0x%08lX",
          (unsigned long)flash_addr,
          (unsigned long)size,
          (unsigned long)ota_slot_base(s_target_slot));

    if (flash_addr != ota_slot_base(s_target_slot)) {
        LOG_W(OTA_TAG, "ERASE rejected: addr mismatch");
        ota_send_response(BSP_OTA_CMD_ERASE, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    LOG_I(OTA_TAG, "ERASE enter port, core=%u slot=%u",
          (unsigned int)s_ota_core,
          (unsigned int)s_target_slot);
    bsp_ota_port_enter();
    s_state = OTA_STATE_ERASING;

    /* 对齐 CamView：仅 CM4 OTA（擦 Bank2）前 STOP CM4；CM7 OTA 只清 BOOT_C2 */
    if ((s_ota_core == OTA_CORE_CM4) && (s_cm4_held == 0U)) {
        if (bsp_ota_port_hold_cm4_ota(OTA_CM4_HOLD_MS) != 0) {
            LOG_E(OTA_TAG, "ERASE rejected: CM4 hold timeout core=%u",
                  (unsigned int)s_ota_core);
            ota_abort_session(BSP_OTA_STATUS_ERR_BUSY);
            ota_send_response(BSP_OTA_CMD_ERASE, frame->seq, BSP_OTA_STATUS_ERR_BUSY, NULL, 0U);
            return;
        }
        s_cm4_held = 1U;
    }

    if (bsp_ota_flash_erase_slot(s_target_slot) != 0) {
        ota_abort_session(BSP_OTA_STATUS_ERR_FLASH);
        ota_send_response(BSP_OTA_CMD_ERASE, frame->seq, BSP_OTA_STATUS_ERR_FLASH, NULL, 0U);
        return;
    }

    bsp_ota_flash_set_target_slot(s_target_slot);
    s_bytes_written = 0U;
    /* 擦除完成即进入 RECEIVING，允许 ERASE 后直接发 DATA（也可再发 START_TRANSFER） */
    s_state = OTA_STATE_RECEIVING;
    ota_send_response(BSP_OTA_CMD_ERASE, frame->seq, BSP_OTA_STATUS_OK, NULL, 0U);
}

/**
 * @brief  处理 START_TRANSFER 命令，记录固件大小与 CRC
 * @param  frame SOIP 帧指针
 */
static void ota_handle_start_transfer(const bsp_ota_frame_t *frame)
{
    uint32_t flash_addr;

    if (ota_check_session(frame, NULL) != 0) {
        ota_send_response(BSP_OTA_CMD_START_TRANSFER, frame->seq, BSP_OTA_STATUS_ERR_SESSION, NULL, 0U);
        return;
    }

    if ((ota_payload_read_u32(frame->payload, frame->payload_len, 4U, &flash_addr) != 0) ||
        (ota_payload_read_u32(frame->payload, frame->payload_len, 8U, &s_image_size) != 0) ||
        (ota_payload_read_u32(frame->payload, frame->payload_len, 12U, &s_image_crc32) != 0)) {
        ota_send_response(BSP_OTA_CMD_START_TRANSFER, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    /* START_TRANSFER 载荷：[token:4][addr:4][image_size:4][image_crc32:4] */
    if ((flash_addr != ota_slot_base(s_target_slot)) ||
        (s_image_size == 0U) ||
        (s_image_size > ota_slot_size())) {
        ota_send_response(BSP_OTA_CMD_START_TRANSFER, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    s_bytes_written = 0U;
    s_state = OTA_STATE_RECEIVING;
    s_data_rx_count = 0U;
    s_data_ack_pending = 0U;
    LOG_I(OTA_TAG, "transfer start size=%lu crc=0x%08lX",
          (unsigned long)s_image_size, (unsigned long)s_image_crc32);
    ota_send_response(BSP_OTA_CMD_START_TRANSFER, frame->seq, BSP_OTA_STATUS_OK, NULL, 0U);
}

/**
 * @brief  处理 DATA 命令，写入固件数据块
 * @param  frame SOIP 帧指针
 */
static void ota_handle_data(const bsp_ota_frame_t *frame)
{
    uint32_t offset;
    const uint8_t *chunk;
    uint32_t chunk_len;

    if (ota_check_session(frame, NULL) != 0) {
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_SESSION, NULL, 0U);
        return;
    }

    if (s_state != OTA_STATE_RECEIVING) {
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_STATE, NULL, 0U);
        return;
    }

    if (frame->payload_len < 8U) {
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    if (ota_payload_read_u32(frame->payload, frame->payload_len, 4U, &offset) != 0) {
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    /* DATA 载荷：[token:4][offset:4][chunk_data...]，固件数据从第 8 字节起 */
    chunk = &frame->payload[8];
    chunk_len = (uint32_t)frame->payload_len - 8U;

    if ((chunk_len == 0U) || (chunk_len > BSP_OTA_CHUNK_MAX)) {
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    if (((offset + chunk_len) < s_image_size) && (chunk_len != BSP_OTA_CHUNK_SIZE)) {
        /* 非最后一包必须满 chunk，防止上位机分包异常 */
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    if ((offset + chunk_len) > s_image_size) {
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    /* 同序重传：已写入只回 ACK */
    if ((offset + chunk_len) <= s_bytes_written) {
        (void)ota_send_data_ack_now(frame->seq);
        return;
    }

    if (bsp_ota_flash_write(offset, chunk, chunk_len) != 0) {
        ota_abort_session(BSP_OTA_STATUS_ERR_FLASH);
        ota_send_response(BSP_OTA_CMD_DATA, frame->seq, BSP_OTA_STATUS_ERR_FLASH, NULL, 0U);
        return;
    }

    if ((offset + chunk_len) > s_bytes_written) {
        s_bytes_written = offset + chunk_len;
    }

    s_data_rx_count++;
#if (BSP_OTA_DATA_LOG_INTERVAL != 0U)
    if ((s_data_rx_count == 1U) ||
        ((s_data_rx_count % BSP_OTA_DATA_LOG_INTERVAL) == 0U)) {
        LOG_I(OTA_TAG, "DATA rx seq=%u offset=0x%lX chunk#=%lu written=%lu/%lu",
              (unsigned int)frame->seq,
              (unsigned long)offset,
              (unsigned long)s_data_rx_count,
              (unsigned long)s_bytes_written,
              (unsigned long)s_image_size);
    }
#endif

    BSP_Watchdog_Kick(BSP_WDG_DEFAULT);

    /* 停等：写完立刻 ACK，成功前不接收下一逻辑包（本函数返回后 poll 才继续） */
    (void)ota_send_data_ack_now(frame->seq);
}

/**
 * @brief  处理 END_TRANSFER 命令，确认数据接收完毕
 * @param  frame SOIP 帧指针
 */
static void ota_handle_end_transfer(const bsp_ota_frame_t *frame)
{
    if (ota_check_session(frame, NULL) != 0) {
        ota_send_response(BSP_OTA_CMD_END_TRANSFER, frame->seq, BSP_OTA_STATUS_ERR_SESSION, NULL, 0U);
        return;
    }

    if (s_bytes_written != s_image_size) {
        /* 实际写入量与 START_TRANSFER 声明的大小不一致，拒绝结束 */
        ota_send_response(BSP_OTA_CMD_END_TRANSFER, frame->seq, BSP_OTA_STATUS_ERR_STATE, NULL, 0U);
        return;
    }

    /* 数据收齐，等待 VERIFY 命令做 CRC 校验并写 EEPROM 元数据 */
    s_state = OTA_STATE_READY_VERIFY;
    ota_send_response(BSP_OTA_CMD_END_TRANSFER, frame->seq, BSP_OTA_STATUS_OK, NULL, 0U);
}

/**
 * @brief  处理 VERIFY 命令，校验固件并写入 EEPROM 启动元数据
 * @param  frame SOIP 帧指针
 */
static void ota_handle_verify(const bsp_ota_frame_t *frame)
{
    uint32_t req_crc;
    boot_eeprom_meta_t meta;

    if (ota_check_session(frame, NULL) != 0) {
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_SESSION, NULL, 0U);
        return;
    }

    if (ota_payload_read_u32(frame->payload, frame->payload_len, 4U, &req_crc) != 0) {
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_PARAM, NULL, 0U);
        return;
    }

    if (bsp_ota_flash_verify(s_target_slot, s_image_size, req_crc) != 0) {
        ota_abort_session(BSP_OTA_STATUS_ERR_VERIFY);
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_VERIFY, NULL, 0U);
        return;
    }

    if (ota_slot_is_valid(s_target_slot) == 0) {
        uint32_t sp = *(__IO uint32_t *)ota_slot_base(s_target_slot);
        uint32_t pc = *(__IO uint32_t *)(ota_slot_base(s_target_slot) + 4U);

        LOG_E(OTA_TAG, "verify: slot invalid SP=0x%08lX Reset=0x%08lX",
              (unsigned long)sp, (unsigned long)pc);
        ota_abort_session(BSP_OTA_STATUS_ERR_VERIFY);
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_VERIFY, NULL, 0U);
        return;
    }

    if (boot_eeprom_is_ready() == 0) {
        LOG_E(OTA_TAG, "verify: EEPROM not ready (main init failed?)");
        ota_abort_session(BSP_OTA_STATUS_ERR_FLASH);
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_FLASH, NULL, 0U);
        return;
    }

    memset(&meta, 0, sizeof(meta));
    if (boot_eeprom_load(&meta) != 0) {
        LOG_W(OTA_TAG, "EEPROM load warn (continue with defaults)");
    }

    LOG_I(OTA_TAG, "verify: EEPROM save pending begin tick=%lu written=%lu",
          (unsigned long)HAL_GetTick(),
          (unsigned long)s_bytes_written);

    if (BSP_Eeprom_Recover() == false) {
        LOG_E(OTA_TAG, "verify: EEPROM recover failed before pending write");
        ota_abort_session(BSP_OTA_STATUS_ERR_FLASH);
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_FLASH, NULL, 0U);
        return;
    }

    if (s_ota_core == OTA_CORE_CM4) {
        if (boot_eeprom_set_cm4_pending(&meta, s_target_slot, s_image_size, req_crc, 0U) != 0) {
            LOG_E(OTA_TAG, "verify: CM4 EEPROM save pending failed slot=%u size=%lu crc=0x%08lX",
                  (unsigned int)s_target_slot,
                  (unsigned long)s_image_size,
                  (unsigned long)req_crc);
            ota_abort_session(BSP_OTA_STATUS_ERR_FLASH);
            ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_FLASH, NULL, 0U);
            return;
        }
    } else if (boot_eeprom_set_pending(&meta, s_target_slot, s_image_size, req_crc, 0U) != 0) {
        LOG_E(OTA_TAG, "verify: EEPROM save pending failed slot=%u size=%lu crc=0x%08lX",
              (unsigned int)s_target_slot,
              (unsigned long)s_image_size,
              (unsigned long)req_crc);
        ota_abort_session(BSP_OTA_STATUS_ERR_FLASH);
        ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_ERR_FLASH, NULL, 0U);
        return;
    }

    s_state = OTA_STATE_IDLE;
    ota_set_active(0U);
    bsp_ota_port_exit();
    LOG_I(OTA_TAG, "EEPROM pending set slot=%u size=%lu crc=0x%08lX -> reboot",
          (unsigned int)s_target_slot,
          (unsigned long)s_image_size,
          (unsigned long)req_crc);
    ota_send_response(BSP_OTA_CMD_VERIFY, frame->seq, BSP_OTA_STATUS_OK, NULL, 0U);
    osDelay(50);
    NVIC_SystemReset();
}

/**
 * @brief  处理 RESET 命令，应答后系统复位
 * @param  frame SOIP 帧指针
 */
static void ota_handle_reset(const bsp_ota_frame_t *frame)
{
    ota_send_response(BSP_OTA_CMD_RESET, frame->seq, BSP_OTA_STATUS_OK, NULL, 0U);
    osDelay(50);  /* 留时间让应答帧从 TCP 发出 */
    NVIC_SystemReset();
}

/**
 * @brief  处理 ABORT 命令，终止当前 OTA 会话
 * @param  frame SOIP 帧指针
 */
static void ota_handle_abort(const bsp_ota_frame_t *frame)
{
    if (s_state != OTA_STATE_IDLE) {
        /* 擦除/传输中途 ABORT 需恢复 OTA 端口与 Flash 相关配置 */
        bsp_ota_port_exit();
    }

    s_state = OTA_STATE_IDLE;
    s_session_token = 0U;
    ota_set_active(0U);  /* 恢复被挂起的 LVGL/Modbus 等任务 */
    ota_send_response(BSP_OTA_CMD_ABORT, frame->seq, BSP_OTA_STATUS_OK, NULL, 0U);
}

/**
 * @brief  根据命令字分发 SOIP 帧到对应处理函数
 * @param  frame 已解析的 SOIP 帧
 */
static void ota_dispatch_frame(const bsp_ota_frame_t *frame)
{
    bsp_ota_cmd_t cmd = (bsp_ota_cmd_t)(frame->cmd & (uint8_t)~BSP_OTA_CMD_RESPONSE_MASK);

    /* 标准 OTA 流程：BEGIN -> ERASE -> START_TRANSFER -> DATA* -> END -> VERIFY -> RESET */
    switch (cmd) {
    case BSP_OTA_CMD_PING:
        ota_handle_ping(frame->seq);
        break;
    case BSP_OTA_CMD_GET_INFO:
        ota_handle_get_info(frame);
        break;
    case BSP_OTA_CMD_BEGIN_SESSION:
        ota_handle_begin_session(frame);
        break;
    case BSP_OTA_CMD_ERASE:
        ota_handle_erase(frame);
        break;
    case BSP_OTA_CMD_START_TRANSFER:
        ota_handle_start_transfer(frame);
        break;
    case BSP_OTA_CMD_DATA:
        ota_handle_data(frame);
        break;
    case BSP_OTA_CMD_END_TRANSFER:
        ota_handle_end_transfer(frame);
        break;
    case BSP_OTA_CMD_VERIFY:
        ota_handle_verify(frame);
        break;
    case BSP_OTA_CMD_RESET:
        ota_handle_reset(frame);
        break;
    case BSP_OTA_CMD_ABORT:
        ota_handle_abort(frame);
        break;
    default:
        ota_send_response((bsp_ota_cmd_t)frame->cmd, frame->seq, BSP_OTA_STATUS_ERR_CMD, NULL, 0U);
        break;
    }
}

/**
 * @brief  轮询传输层并解析、分发 SOIP 帧
 */
void bsp_ota_manager_poll(void)
{
    uint8_t byte;
    bsp_ota_frame_t frame;

    if (!bsp_ota_transport_ready()) {
        /* TCP 未连接时不解析，避免误处理残留字节 */
        return;
    }

    /* ---- 阶段 1：优先尝试发送 pending 的 DATA ACK ---- */
    /* ACK 发送失败时也必须继续收包，否则 PC 重传永远进不了 SOIP 解析（停等协议死锁） */
    if (s_data_ack_pending != 0U) {
        (void)ota_flush_data_ack();
    }

    /* ---- 阶段 2：从 TCP pending 缓冲逐字节读取并组帧 ---- */
    for (;;) {
        if (bsp_ota_transport_try_read(&byte) != 0) {
            break;  /* 当前无更多字节，退出内层循环 */
        }

        if (s_rx_len >= BSP_OTA_RX_BUF_SIZE) {
            /* 缓冲溢出时丢弃已收数据，等待上位机重传 */
            s_rx_len = 0U;
        }

        s_rx_buf[s_rx_len++] = byte;

        /* 同一缓冲区内可能连续解析多帧（粘包） */
        while (bsp_ota_try_parse(s_rx_buf, &s_rx_len, &frame) != 0) {
#if (BSP_OTA_SOIP_VERBOSE != 0U)
            LOG_I(OTA_TAG, "SOIP cmd=0x%02X seq=%u len=%u",
                  (unsigned int)frame.cmd, (unsigned int)frame.seq,
                  (unsigned int)frame.payload_len);
#endif
            ota_dispatch_frame(&frame);
            frame.valid = 0U;
        }
    }

    /* ---- 阶段 3：收包循环结束后再试一次 ACK（提高发送成功率） ---- */
    if (s_data_ack_pending != 0U) {
        (void)ota_flush_data_ack();
    }
}

/**
 * @brief  查询 OTA 会话是否处于活动状态
 * @retval 1 活动，0 空闲
 */
uint8_t bsp_ota_manager_is_active(void)
{
    return s_ota_active;
}

/**
 * @brief  查询是否正在接收固件数据或待发 DATA 应答
 * @retval 1 正在接收或 ACK 待发送，0 否
 */
uint8_t bsp_ota_manager_is_receiving(void)
{
    return ((s_state == OTA_STATE_RECEIVING) || (s_data_ack_pending != 0U)) ? 1U : 0U;
}
