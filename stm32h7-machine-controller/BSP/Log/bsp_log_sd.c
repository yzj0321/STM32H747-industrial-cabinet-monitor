/**
 * @file    bsp_log_sd.c
 * @brief   SDNAND FatFs 异常日志：挂载、CSV 追加、告警边沿监测
 */
#include "bsp_log_sd.h"

#include "bsp_app_data.h"
#include "bsp_driver_sd.h"
#include "bsp_log.h"
#include "fatfs.h"
#include "main.h"
#include "sdmmc.h"

#include "cmsis_os2.h"

#include <stdio.h>
#include <string.h>

#define LOG_SD_TAG           "LOG-SD"
#define LOG_SD_ROTATE_BYTES  (256U * 1024U)
#define LOG_SD_LINE_MAX      192U
#define TEMP_HIGH_C          60U
#define HUMI_LOW_RH          10U
#define HUMI_HIGH_RH         90U

static osMutexId_t s_fs_mutex;
static osThreadId_t s_task;
static uint8_t s_mounted;
static uint8_t s_mount_busy;
static uint8_t s_header_ok;
static FIL s_write_fil;
static FIL s_read_fil;
static uint8_t s_read_open;
static uint8_t s_write_open;

/**
 * @brief  获取 FatFs 访问锁
 */
static void fs_lock(void)
{
    if (s_fs_mutex != NULL) {
        (void)osMutexAcquire(s_fs_mutex, osWaitForever);
    }
}

/**
 * @brief  释放 FatFs 访问锁
 */
static void fs_unlock(void)
{
    if (s_fs_mutex != NULL) {
        (void)osMutexRelease(s_fs_mutex);
    }
}

/**
 * @brief  确保日志文件头已写入（持锁）
 */
static int ensure_header_locked(void)
{
    FRESULT fr;
    char line[64];

    if (s_header_ok != 0U) {
        return 0;
    }

    if (f_size(&s_write_fil) == 0U) {
        (void)snprintf(line, sizeof(line),
                       "tick_ms,level,code,temp_c,humi_rh,adc_mv,message\r\n");
        fr = f_write(&s_write_fil, line, (UINT)strlen(line), NULL);
        if (fr != FR_OK) {
            return -1;
        }
        (void)f_sync(&s_write_fil);
    }
    s_header_ok = 1U;
    return 0;
}

/**
 * @brief  以追加方式打开主日志（持锁）
 */
static int open_main_for_append_locked(void)
{
    FRESULT fr;
    char path[48];

    if (s_write_open != 0U) {
        return 0;
    }

    (void)snprintf(path, sizeof(path), "%s/%s", BSP_LOG_SD_DIR, BSP_LOG_SD_FILE_MAIN);
    fr = f_open(&s_write_fil, path, FA_OPEN_ALWAYS | FA_WRITE);
    if (fr != FR_OK) {
        return -1;
    }
    fr = f_lseek(&s_write_fil, f_size(&s_write_fil));
    if (fr != FR_OK) {
        (void)f_close(&s_write_fil);
        return -2;
    }
    s_write_open = 1U;
    s_header_ok = 0U;
    return ensure_header_locked();
}

/**
 * @brief  必要时轮转日志文件（持锁）
 */
static int rotate_if_needed_locked(void)
{
    FRESULT fr;
    char src[48];
    char dst[48];
    static uint16_t s_rotate_idx = 1U;

    if (s_write_open == 0U) {
        return 0;
    }
    if (f_size(&s_write_fil) < LOG_SD_ROTATE_BYTES) {
        return 0;
    }

    (void)f_close(&s_write_fil);
    s_write_open = 0U;

    (void)snprintf(src, sizeof(src), "%s/%s", BSP_LOG_SD_DIR, BSP_LOG_SD_FILE_MAIN);
    (void)snprintf(dst, sizeof(dst), "%s/alarms_%03u.csv",
                   BSP_LOG_SD_DIR, (unsigned)s_rotate_idx);
    s_rotate_idx++;
    if (s_rotate_idx > 999U) {
        s_rotate_idx = 1U;
    }

    (void)f_unlink(dst);
    fr = f_rename(src, dst);
    if (fr != FR_OK) {
        LOG_W(LOG_SD_TAG, "rotate rename fail=%d", (int)fr);
    } else {
        LOG_I(LOG_SD_TAG, "rotated -> %s", dst);
    }
    return open_main_for_append_locked();
}

/**
 * @brief  FatFs 错误码转字符串
 */
static const char *fr_str(FRESULT fr)
{
    switch (fr) {
    case FR_OK: return "FR_OK";
    case FR_DISK_ERR: return "FR_DISK_ERR";
    case FR_INT_ERR: return "FR_INT_ERR";
    case FR_NOT_READY: return "FR_NOT_READY";
    case FR_NO_FILE: return "FR_NO_FILE";
    case FR_NO_PATH: return "FR_NO_PATH";
    case FR_INVALID_NAME: return "FR_INVALID_NAME";
    case FR_DENIED: return "FR_DENIED";
    case FR_EXIST: return "FR_EXIST";
    case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
    case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
    case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
    case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
    case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM";
    case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
    case FR_TIMEOUT: return "FR_TIMEOUT";
    case FR_LOCKED: return "FR_LOCKED";
    case FR_NOT_ENOUGH_CORE: return "FR_NOT_ENOUGH_CORE";
    case FR_TOO_MANY_OPEN_FILES: return "FR_TOO_MANY_OPEN_FILES";
    case FR_INVALID_PARAMETER: return "FR_INVALID_PARAMETER";
    default: return "FR_?";
    }
}

/**
 * @brief  对齐 W5500_OTA：在 f_mount 前确保 SD 硬件就绪（必要时再跑 BSP_SD_Init）
 */
static int ensure_sd_hw_ready(void)
{
    uint8_t st;
    HAL_SD_CardInfoTypeDef info;

    LOG_I(LOG_SD_TAG, "BSP_SD_IsDetected=%u (1=PRESENT)",
          (unsigned)BSP_SD_IsDetected());
    LOG_I(LOG_SD_TAG, "hsd2.State=%u err=0x%08lX",
          (unsigned)hsd2.State, (unsigned long)hsd2.ErrorCode);

    st = BSP_SD_GetCardState();
    LOG_I(LOG_SD_TAG, "GetCardState=%u (0=TRANSFER_OK)", (unsigned)st);

    if ((hsd2.State == HAL_SD_STATE_READY) && (st == SD_TRANSFER_OK)) {
        BSP_SD_GetCardInfo(&info);
    LOG_I(LOG_SD_TAG,
          "HW already ready CardType=%lu BlockNbr=%lu BlockSize=%lu LogBlockNbr=%lu LogBlockSize=%lu",
          (unsigned long)info.CardType,
          (unsigned long)info.BlockNbr,
          (unsigned long)info.BlockSize,
          (unsigned long)info.LogBlockNbr,
          (unsigned long)info.LogBlockSize);
        return 0;
    }

    /* 启动阶段 soft-fail 或状态异常：参考工程会在 diskio 再 Init 一次 */
    LOG_W(LOG_SD_TAG, "HW not ready, HAL_SD_DeInit + BSP_SD_Init retry...");
    (void)HAL_SD_DeInit(&hsd2);
    st = BSP_SD_Init();
    if (st != MSD_OK) {
        LOG_E(LOG_SD_TAG, "BSP_SD_Init FAIL st=%u hsd.State=%u err=0x%08lX",
              (unsigned)st, (unsigned)hsd2.State, (unsigned long)hsd2.ErrorCode);
        return -1;
    }

    BSP_SD_GetCardInfo(&info);
    LOG_I(LOG_SD_TAG,
          "BSP_SD_Init OK CardType=%lu BlockNbr=%lu BlockSize=%lu LogBlockNbr=%lu LogBlockSize=%lu",
          (unsigned long)info.CardType,
          (unsigned long)info.BlockNbr,
          (unsigned long)info.BlockSize,
          (unsigned long)info.LogBlockNbr,
          (unsigned long)info.LogBlockSize);

    st = BSP_SD_GetCardState();
    LOG_I(LOG_SD_TAG, "post-init GetCardState=%u", (unsigned)st);
    if (st != SD_TRANSFER_OK) {
        LOG_W(LOG_SD_TAG, "card not in TRANSFER state");
        return -2;
    }
    return 0;
}

/**
 * @brief  尝试挂载 SDNAND 文件系统
 */
static int try_mount(void)
{
    FRESULT fr;
    /* 32B 对齐 + 足够大：mkfs 工作区；避免栈上未对齐缓冲进 SDMMC DMA */
    static ALIGN_32BYTES(BYTE s_mkfs_work[4096]);

    LOG_I(LOG_SD_TAG, "--- SDNAND/FatFs mount begin path=%s ---", SDPath);

    if (ensure_sd_hw_ready() != 0) {
        return -10;
    }

    fr = f_mount(&SDFatFS, SDPath, 1);
    LOG_I(LOG_SD_TAG, "f_mount(opt=1) -> %s (%d)", fr_str(fr), (int)fr);

    if (fr == FR_NO_FILESYSTEM) {
        LOG_W(LOG_SD_TAG, "no filesystem, try f_mkfs (SFD, no MBR)...");
        /* 先卸载，避免上次失败挂载留下脏状态 */
        (void)f_mount(NULL, SDPath, 0);
        /*
         * FM_SFD：超级软盘（VBR 在扇区 0），SDNAND 比“MBR+扇区63”更稳。
         * 之前 FM_ANY 会建分区表；若 DMA/cache 写坏 MBR，remount 一直 FR_NO_FILESYSTEM。
         */
        fr = f_mkfs(SDPath, (BYTE)(FM_ANY | FM_SFD), 0, s_mkfs_work, sizeof(s_mkfs_work));
        LOG_I(LOG_SD_TAG, "f_mkfs -> %s (%d)", fr_str(fr), (int)fr);
        if (fr != FR_OK) {
            return -1;
        }
        osDelay(50); /* 给 NAND 内部写完一点时间 */
        (void)f_mount(NULL, SDPath, 0);
        fr = f_mount(&SDFatFS, SDPath, 1);
        LOG_I(LOG_SD_TAG, "f_mount after mkfs -> %s (%d)", fr_str(fr), (int)fr);
    }
    if (fr != FR_OK) {
        LOG_E(LOG_SD_TAG, "FatFs mount FAILED: %s", fr_str(fr));
        return -2;
    }

    fr = f_mkdir(BSP_LOG_SD_DIR);
    LOG_I(LOG_SD_TAG, "f_mkdir(%s) -> %s (%d)", BSP_LOG_SD_DIR, fr_str(fr), (int)fr);
    if ((fr != FR_OK) && (fr != FR_EXIST)) {
        LOG_E(LOG_SD_TAG, "mkdir fail");
        return -3;
    }

    s_mounted = 1U;
    LOG_I(LOG_SD_TAG, "=== SDNAND + FatFs READY === path=%s dir=%s",
          SDPath, BSP_LOG_SD_DIR);
    return 0;
}

/**
 * @brief  查询 SD 日志是否就绪
 */
uint8_t bsp_log_sd_is_ready(void)
{
    return s_mounted;
}

/**
 * @brief  确保 SD 已挂载
 */
int bsp_log_sd_ensure_mounted(void)
{
    int rc;

    if (s_mounted != 0U) {
        return 0;
    }

    fs_lock();
    if (s_mounted != 0U) {
        fs_unlock();
        return 0;
    }
    if (s_mount_busy != 0U) {
        fs_unlock();
        /* 另一任务正在挂载：稍等再查结果 */
        osDelay(100);
        return (s_mounted != 0U) ? 0 : -9;
    }
    s_mount_busy = 1U;
    fs_unlock();

    rc = try_mount();

    fs_lock();
    s_mount_busy = 0U;
    if ((rc == 0) && (s_write_open == 0U)) {
        (void)open_main_for_append_locked();
    }
    fs_unlock();
    return rc;
}

/**
 * @brief  查询 SD 剩余空间
 */
int bsp_log_sd_get_space(bsp_log_sd_space_t *out)
{
    FRESULT fr;
    DWORD fre_clust = 0U;
    FATFS *fs = NULL;
    HAL_SD_CardInfoTypeDef info;
    uint32_t ss;

    if (out == NULL) {
        return -1;
    }
    out->card_bytes = 0U;
    out->total_bytes = 0U;
    out->free_bytes = 0U;

    if (bsp_log_sd_ensure_mounted() != 0) {
        return -2;
    }

    BSP_SD_GetCardInfo(&info);
    if ((info.LogBlockNbr > 0U) && (info.LogBlockSize > 0U)) {
        out->card_bytes = (uint64_t)info.LogBlockNbr * (uint64_t)info.LogBlockSize;
    }

    fs_lock();
    fr = f_getfree(SDPath, &fre_clust, &fs);
    if ((fr != FR_OK) || (fs == NULL)) {
        fs_unlock();
        LOG_E(LOG_SD_TAG, "f_getfree -> %s (%d)", fr_str(fr), (int)fr);
        return -3;
    }

#if _MAX_SS != _MIN_SS
    ss = fs->ssize;
#else
    ss = (uint32_t)_MAX_SS;
#endif
    if (ss == 0U) {
        ss = 512U;
    }

    /* n_fatent = 簇数 + 2；总数据扇区 = (n_fatent-2)*csize */
    out->total_bytes = (uint64_t)(fs->n_fatent - 2U) * (uint64_t)fs->csize * (uint64_t)ss;
    out->free_bytes = (uint64_t)fre_clust * (uint64_t)fs->csize * (uint64_t)ss;
    fs_unlock();

    LOG_I(LOG_SD_TAG,
          "space card=%lu total=%lu free=%lu bytes",
          (unsigned long)out->card_bytes,
          (unsigned long)out->total_bytes,
          (unsigned long)out->free_bytes);
    return 0;
}

/**
 * @brief  写入一行日志到 SD
 */
int bsp_log_sd_write(const char *level, const char *code, const char *message)
{
    char line[LOG_SD_LINE_MAX];
    FRESULT fr;
    UINT bw = 0U;
    uint16_t temp_c;
    uint16_t humi;
    uint16_t adc_mv;
    int n;

    if ((level == NULL) || (code == NULL) || (message == NULL)) {
        return -1;
    }
    if (s_mounted == 0U) {
        return -2;
    }

    temp_c = BSP_AppData_GetTemperature();
    humi = BSP_AppData_GetHumidity();
    adc_mv = BSP_AppData_GetAdcMv();

    n = snprintf(line, sizeof(line),
                 "%lu,%s,%s,%u,%u,%u,%s\r\n",
                 (unsigned long)HAL_GetTick(),
                 level,
                 code,
                 (unsigned)temp_c,
                 (unsigned)humi,
                 (unsigned)adc_mv,
                 message);
    if ((n <= 0) || (n >= (int)sizeof(line))) {
        return -3;
    }

    fs_lock();
    if (open_main_for_append_locked() != 0) {
        fs_unlock();
        return -4;
    }
    (void)rotate_if_needed_locked();

    fr = f_write(&s_write_fil, line, (UINT)n, &bw);
    if ((fr != FR_OK) || (bw != (UINT)n)) {
        fs_unlock();
        return -5;
    }
    (void)f_sync(&s_write_fil);
    fs_unlock();

    LOG_W(LOG_SD_TAG, "%s %s %s", level, code, message);
    return 0;
}

/**
 * @brief  列出 SD 日志文件
 */
int bsp_log_sd_list(bsp_log_sd_file_info_t *out, size_t max_out)
{
    DIR dir;
    FILINFO fno;
    FRESULT fr;
    size_t count = 0U;

    if ((out == NULL) || (max_out == 0U) || (s_mounted == 0U)) {
        return -1;
    }

    fs_lock();
    fr = f_opendir(&dir, BSP_LOG_SD_DIR);
    if (fr != FR_OK) {
        fs_unlock();
        return -2;
    }

    for (;;) {
        fr = f_readdir(&dir, &fno);
        if ((fr != FR_OK) || (fno.fname[0] == 0)) {
            break;
        }
        if ((fno.fattrib & AM_DIR) != 0U) {
            continue;
        }
        if (count >= max_out) {
            break;
        }
        (void)snprintf(out[count].name, sizeof(out[count].name), "%s", fno.fname);
        out[count].size = (uint32_t)fno.fsize;
        count++;
    }
    (void)f_closedir(&dir);
    fs_unlock();
    return (int)count;
}

/**
 * @brief  打开指定日志文件只读（供以太网导出）
 */
int bsp_log_sd_open_read(const char *name, uint32_t *out_size)
{
    char path[64];
    FRESULT fr;

    if ((name == NULL) || (out_size == NULL) || (s_mounted == 0U)) {
        return -1;
    }
    if ((strchr(name, '/') != NULL) || (strchr(name, '\\') != NULL) ||
        (strstr(name, "..") != NULL)) {
        return -2;
    }

    (void)snprintf(path, sizeof(path), "%s/%s", BSP_LOG_SD_DIR, name);

    fs_lock();
    if (s_read_open != 0U) {
        (void)f_close(&s_read_fil);
        s_read_open = 0U;
    }
    /*
     * _FS_LOCK>0 时，alarms.csv 若被写句柄长期占用，再 FA_READ 会 FR_LOCKED。
     * 下载前先关掉写文件；写操作会在下次 bsp_log_sd_write 时重新打开。
     */
    if (s_write_open != 0U) {
        (void)f_sync(&s_write_fil);
        (void)f_close(&s_write_fil);
        s_write_open = 0U;
        LOG_I(LOG_SD_TAG, "closed write handle for GET %s", name);
    }

    fr = f_open(&s_read_fil, path, FA_READ);
    if (fr != FR_OK) {
        LOG_E(LOG_SD_TAG, "f_open(%s) FA_READ -> %s (%d)", path, fr_str(fr), (int)fr);
        fs_unlock();
        return -3;
    }
    *out_size = (uint32_t)f_size(&s_read_fil);
    s_read_open = 1U;
    fs_unlock();
    LOG_I(LOG_SD_TAG, "GET open ok %s size=%lu", name, (unsigned long)*out_size);
    return 0;
}

/**
 * @brief  从已打开的日志文件读取一块数据
 */
int bsp_log_sd_read(void *buf, uint32_t len)
{
    FRESULT fr;
    UINT br = 0U;

    if ((buf == NULL) || (len == 0U) || (s_read_open == 0U)) {
        return -1;
    }

    fs_lock();
    fr = f_read(&s_read_fil, buf, (UINT)len, &br);
    fs_unlock();
    if (fr != FR_OK) {
        return -2;
    }
    return (int)br;
}

/**
 * @brief  关闭当前只读日志文件
 */
void bsp_log_sd_close_read(void)
{
    fs_lock();
    if (s_read_open != 0U) {
        (void)f_close(&s_read_fil);
        s_read_open = 0U;
    }
    fs_unlock();
}

/**
 * @brief  监测水浸/温湿度告警边沿并写 CSV
 */
static void monitor_alarms(void)
{
    static uint8_t wet_latched;
    static uint8_t temp_alarm;
    static uint8_t humi_alarm;
    static uint8_t inited;

    uint16_t adc_mv = BSP_AppData_GetAdcMv();
    uint16_t temp_c = BSP_AppData_GetTemperature();
    uint16_t humi = BSP_AppData_GetHumidity();
    uint8_t wet_now;
    uint8_t temp_now;
    uint8_t humi_now;
    char msg[64];

    wet_now = BSP_AppData_GetWet();
    temp_now = (temp_c > TEMP_HIGH_C) ? 1U : 0U;
    humi_now = ((humi < HUMI_LOW_RH) || (humi > HUMI_HIGH_RH)) ? 1U : 0U;

    if (inited == 0U) {
        wet_latched = wet_now;
        temp_alarm = temp_now;
        humi_alarm = humi_now;
        inited = 1U;
        return;
    }

    if (wet_now != wet_latched) {
        wet_latched = wet_now;
        (void)snprintf(msg, sizeof(msg), "water %s adc=%umV",
                       wet_now ? "ALARM" : "CLEAR", (unsigned)adc_mv);
        (void)bsp_log_sd_write(wet_now ? "ALARM" : "CLEAR", "WATER", msg);
    }
    if (temp_now != temp_alarm) {
        temp_alarm = temp_now;
        (void)snprintf(msg, sizeof(msg), "temp %s value=%uC",
                       temp_now ? "WARN" : "CLEAR", (unsigned)temp_c);
        (void)bsp_log_sd_write(temp_now ? "WARN" : "CLEAR", "TEMP", msg);
    }
    if (humi_now != humi_alarm) {
        humi_alarm = humi_now;
        (void)snprintf(msg, sizeof(msg), "humi %s value=%u%%",
                       humi_now ? "WARN" : "CLEAR", (unsigned)humi);
        (void)bsp_log_sd_write(humi_now ? "WARN" : "CLEAR", "HUMI", msg);
    }
}

/**
 * @brief  SD 日志后台任务：挂载文件系统并周期监测告警
 */
static void LogSdTask(void *argument)
{
    (void)argument;

    /* printf 不依赖日志互斥锁，确认任务真的被调度到 */
    printf("[LOG-SD] task RUNNING (prio=Normal)\r\n");
    LOG_I(LOG_SD_TAG, "task RUNNING, mount soon");
    osDelay(500);

    /* 挂载也可由 logXfer 触发；此处负责成功后的告警监测 */
    for (;;) {
        if (s_mounted == 0U) {
            LOG_I(LOG_SD_TAG, "calling ensure_mounted...");
            if (bsp_log_sd_ensure_mounted() != 0) {
                LOG_W(LOG_SD_TAG, "mount failed, retry in 5s");
                osDelay(5000);
                continue;
            }
            (void)bsp_log_sd_write("INFO", "BOOT", "log_sd ready");
            LOG_I(LOG_SD_TAG, "ready, monitor alarms");
        }
        monitor_alarms();
        osDelay(500);
    }
}

/**
 * @brief  启动 SD 日志任务（RTOS 调度后调用一次）
 */
void bsp_log_sd_start(void)
{
    const osThreadAttr_t attr = {
        .name = "logSd",
        /* try_mount 有 512B work[] + FatFs；再加日志缓冲，给足栈 */
        .stack_size = 2048U * 4U,
        /* 必须与 W5500/logXfer 同级：BelowNormal 会被阻塞式 WIZnet connect 饿死 */
        .priority = (osPriority_t)osPriorityNormal,
    };

    if (s_task != NULL) {
        return;
    }
    if (s_fs_mutex == NULL) {
        s_fs_mutex = osMutexNew(NULL);
    }
    s_task = osThreadNew(LogSdTask, NULL, &attr);
    if (s_task == NULL) {
        printf("[LOG-SD] osThreadNew FAILED (heap?)\r\n");
        LOG_E(LOG_SD_TAG, "osThreadNew(logSd) failed");
    } else {
        printf("[LOG-SD] logSd thread created (prio=Normal)\r\n");
        LOG_I(LOG_SD_TAG, "logSd thread created (prio=Normal)");
    }
}
