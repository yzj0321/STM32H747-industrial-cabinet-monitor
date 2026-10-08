/**
 * @file    bsp_modbus_app.c
 * @brief   Modbus RTU 从站应用（USB 传输、线圈/寄存器映射、持久化）
 */
#include "bsp_modbus_app.h"

#include "bsp_app_data.h"
#include "bsp_modbus_coil_nv.h"
#include "bsp_usb_modbus.h"
#include "bsp_watchdog.h"
#include "bsp_ota_coop.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "errno.h"
#include "main.h"
#include "modbus.h"

#include <stdint.h>
#include <string.h>

#define MODBUS_TASK_STACK_WORDS       2048U
#define MODBUS_STORAGE_STACK_WORDS    1024U
#define MODBUS_COIL_SAVE_FLAG         0x01U
#define MODBUS_COIL_RESTORE_DELAY_MS  500U

#define MODBUS_COIL_START             0U

#define MODBUS_HOLDING_START          0U
#define MODBUS_HOLDING_COUNT          10U

/* PA3 LED: CubeMX init is high; board LED is typically active-low */
#define MODBUS_LED_PA3_GPIO_PORT      GPIOA
#define MODBUS_LED_PA3_GPIO_PIN       GPIO_PIN_3
#define MODBUS_LED_PA3_ACTIVE_LOW     1

static uint8_t s_coil_bits[MODBUS_COIL_COUNT];
static uint16_t s_hold_regs[MODBUS_HOLDING_COUNT];
static modbus_mapping_t s_mb_map = {
    .tab_bits = s_coil_bits,
    .start_bits = MODBUS_COIL_START,
    .nb_bits = MODBUS_COIL_COUNT,
    .tab_input_bits = NULL,
    .start_input_bits = 0U,
    .nb_input_bits = 0U,
    .tab_registers = s_hold_regs,
    .start_registers = MODBUS_HOLDING_START,
    .nb_registers = MODBUS_HOLDING_COUNT,
    .tab_input_registers = NULL,
    .start_input_registers = 0U,
    .nb_input_registers = 0U,
};
static modbus_mapping_t *s_mb_mapping;
static volatile uint8_t s_coil_save_pending;
static osThreadId_t s_coil_storage_task;
static osThreadId_t s_modbus_server_task;
static uint8_t s_modbus_data_ready;
static uint8_t s_modbus_server_started;

/**
 * @brief  将当前线圈状态写入 EEPROM
 */
static void BSP_Modbus_PersistCoils(void)
{
    if ((s_mb_mapping == NULL) || (s_mb_mapping->nb_bits == 0U)) {
        return;
    }

    (void)BSP_Modbus_CoilNv_Save(s_coil_bits, MODBUS_COIL_COUNT);
}

/**
 * @brief  请求异步持久化线圈（通知存储任务）
 */
static void BSP_Modbus_RequestPersistCoils(void)
{
    s_coil_save_pending = 1U;

    if (s_coil_storage_task != NULL) {
        (void)osThreadFlagsSet(s_coil_storage_task, MODBUS_COIL_SAVE_FLAG);
    }
}

/**
 * @brief  判断 Modbus 功能码是否为写线圈类
 * @param  function  Modbus 功能码
 * @retval true  写单/多线圈；false 其他
 */
static bool BSP_Modbus_IsCoilWriteFunction(uint8_t function)
{
    return (function == MODBUS_FC_WRITE_SINGLE_COIL) ||
           (function == MODBUS_FC_WRITE_MULTIPLE_COILS);
}

/**
 * @brief  根据 PA3 线圈状态驱动 LED GPIO
 */
static void BSP_Modbus_ApplyCoilToLedPa3(void)
{
    uint8_t coil_on = BSP_Modbus_GetCoil(MODBUS_COIL_LED_PA3);

#if MODBUS_LED_PA3_ACTIVE_LOW
    HAL_GPIO_WritePin(MODBUS_LED_PA3_GPIO_PORT,
                      MODBUS_LED_PA3_GPIO_PIN,
                      coil_on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(MODBUS_LED_PA3_GPIO_PORT,
                      MODBUS_LED_PA3_GPIO_PIN,
                      coil_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#endif
}

/**
 * @brief  从传感器应用数据刷新保持寄存器镜像
 */
static void BSP_Modbus_RefreshSensorHoldings(void)
{
    if (s_mb_mapping == NULL) {
        return;
    }

    s_mb_mapping->tab_registers[MODBUS_REG_HUMIDITY] = BSP_AppData_GetHumidity();
    s_mb_mapping->tab_registers[MODBUS_REG_TEMPERATURE] = BSP_AppData_GetTemperature();
    s_mb_mapping->tab_registers[MODBUS_REG_ADC_MV] = BSP_AppData_GetAdcMv();
    s_mb_mapping->tab_registers[MODBUS_REG_WET] = BSP_AppData_GetWet();
}

/**
 * @brief  设置保持寄存器值并同步到应用数据
 * @param  index  寄存器索引
 * @param  value  寄存器值
 */
void BSP_Modbus_SetHoldingRegister(uint16_t index, uint16_t value)
{
    switch (index) {
    case MODBUS_REG_HUMIDITY:
        BSP_AppData_SetHumidity(value);
        break;
    case MODBUS_REG_TEMPERATURE:
        BSP_AppData_SetTemperature(value);
        break;
    case MODBUS_REG_ADC_MV:
        BSP_AppData_SetAdcMv(value);
        break;
    case MODBUS_REG_WET:
        BSP_AppData_SetWet((uint8_t)(value != 0U));
        break;
    default:
        break;
    }

    if ((s_mb_mapping != NULL) && (index < s_mb_mapping->nb_registers)) {
        s_mb_mapping->tab_registers[index] = value;
    }
}

/**
 * @brief  读取保持寄存器值
 * @param  index  寄存器索引
 * @retval 寄存器值，无效索引返回 0
 */
uint16_t BSP_Modbus_GetHoldingRegister(uint16_t index)
{
    switch (index) {
    case MODBUS_REG_HUMIDITY:
        return BSP_AppData_GetHumidity();
    case MODBUS_REG_TEMPERATURE:
        return BSP_AppData_GetTemperature();
    case MODBUS_REG_ADC_MV:
        return BSP_AppData_GetAdcMv();
    case MODBUS_REG_WET:
        return BSP_AppData_GetWet();
    default:
        break;
    }

    return 0U;
}

/**
 * @brief  设置线圈状态并触发 LED 与持久化
 * @param  index  线圈索引
 * @param  value  非 0 为 ON
 */
void BSP_Modbus_SetCoil(uint16_t index, uint8_t value)
{
    if (index >= MODBUS_COIL_COUNT) {
        return;
    }

    s_coil_bits[index] = value ? 1U : 0U;

    if (index == MODBUS_COIL_LED_PA3) {
        BSP_Modbus_ApplyCoilToLedPa3();
    }

    BSP_Modbus_RequestPersistCoils();
}

/**
 * @brief  读取线圈状态
 * @param  index  线圈索引
 * @retval 线圈值（0 或 1），无效索引返回 0
 */
uint8_t BSP_Modbus_GetCoil(uint16_t index)
{
    if (index >= MODBUS_COIL_COUNT) {
        return 0U;
    }

    return s_coil_bits[index];
}

/**
 * @brief  初始化 Modbus 内存映射表
 * @retval 0 成功
 */
static int BSP_Modbus_MappingInit(void)
{
    if (s_mb_mapping != NULL) {
        return 0;
    }

    s_mb_mapping = &s_mb_map;
    memset(s_coil_bits, 0, sizeof(s_coil_bits));
    memset(s_hold_regs, 0, sizeof(s_hold_regs));
    BSP_Modbus_ApplyCoilToLedPa3();

    return 0;
}

/**
 * @brief  从 EEPROM 恢复线圈状态到映射表
 */
static void BSP_Modbus_RestoreCoilsFromStorage(void)
{
    if ((s_mb_mapping == NULL) || (s_mb_mapping->nb_bits == 0U)) {
        return;
    }

    if (!BSP_Modbus_CoilNv_MediaReady()) {
        return;
    }

    if (BSP_Modbus_CoilNv_Init()) {
        (void)BSP_Modbus_CoilNv_Load(s_coil_bits, MODBUS_COIL_COUNT);
    }

    BSP_Modbus_ApplyCoilToLedPa3();
}

/**
 * @brief  线圈持久化后台任务
 * @param  argument  未使用
 */
static void ModbusCoilStorageTask(void *argument)
{
    uint32_t flags;

    (void)argument;

    for (;;) {
        flags = osThreadFlagsWait(MODBUS_COIL_SAVE_FLAG,
                                  osFlagsWaitAny,
                                  osWaitForever);

        if ((flags & MODBUS_COIL_SAVE_FLAG) != 0U) {
            if (s_coil_save_pending != 0U) {
                s_coil_save_pending = 0U;
                BSP_Modbus_PersistCoils();
            }
        }
    }
}

/**
 * @brief  延迟后从存储恢复线圈的一次性任务
 * @param  argument  未使用
 */
static void ModbusCoilRestoreTask(void *argument)
{
    (void)argument;

    osDelay(MODBUS_COIL_RESTORE_DELAY_MS);

    BSP_Modbus_RestoreCoilsFromStorage();
    osThreadExit();
}

/**
 * @brief  初始化 Modbus 数据映射与线圈存储/恢复任务
 */
void BSP_Modbus_Data_Init(void)
{
    const osThreadAttr_t storage_attr = {
        .name = "mbCoilStore",
        .stack_size = MODBUS_STORAGE_STACK_WORDS * 4U,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };
    const osThreadAttr_t restore_attr = {
        .name = "mbCoilRestore",
        .stack_size = MODBUS_STORAGE_STACK_WORDS * 4U,
        .priority = (osPriority_t)osPriorityBelowNormal,
    };

    if (s_modbus_data_ready != 0U) {
        return;
    }

    if (BSP_Modbus_MappingInit() != 0) {
        return;
    }

    if (s_coil_storage_task == NULL) {
        s_coil_storage_task = osThreadNew(ModbusCoilStorageTask, NULL, &storage_attr);
        bsp_ota_coop_register(s_coil_storage_task);
    }

    {
        osThreadId_t restore_task = osThreadNew(ModbusCoilRestoreTask, NULL, &restore_attr);
        bsp_ota_coop_register(restore_task);
    }
    s_modbus_data_ready = 1U;
}

/**
 * @brief  Modbus RTU 从站主循环任务
 * @param  argument  未使用
 */
static void ModbusServerTask(void *argument)
{
    uint8_t *query;
    modbus_t *ctx;
    int rc;

    (void)argument;

    osDelay(200);

    query = (uint8_t *)pvPortMalloc(MODBUS_RTU_MAX_ADU_LENGTH);
    if (query == NULL) {
        vTaskDelete(NULL);
    }

    ctx = modbus_new_st_rtu("usb", 115200, 'N', 8, 1);
    if (ctx == NULL) {
        vPortFree(query);
        vTaskDelete(NULL);
    }

    modbus_set_slave(ctx, (int)MODBUS_SLAVE_ID);
    /* 从机等待主机请求：默认 indication_timeout=0 会导致 RecvByte 超时 0ms */
    modbus_set_indication_timeout(ctx, 0, 1000000U);
    modbus_set_byte_timeout(ctx, 0, 20000U);

    BSP_USB_Modbus_Flush();

    if (modbus_connect(ctx) == -1) {
        modbus_free(ctx);
        vPortFree(query);
        vTaskDelete(NULL);
    }

    for (;;) {
        /* 阻塞等待 USB CDC 收到一帧 Modbus RTU ADU */
        rc = modbus_receive(ctx, query);

        if (rc == 0) {
            /* 空帧/确认跳过，必须 delay 避免占满 CPU 饿死 ADC/LVGL */
            BSP_Watchdog_Kick(BSP_WDG_MODBUS);
            osDelay(1);
            continue;
        }

        if (rc == -1 && errno != EMBBADCRC) {
            /* 非 CRC 错误（如无数据）直接继续，CRC 错误仍尝试 reply 异常码 */
            BSP_Watchdog_Kick(BSP_WDG_MODBUS);
            continue;
        }

        BSP_Watchdog_Kick(BSP_WDG_MODBUS);
        /* 应答前刷新传感器寄存器，保证主机读到最新温湿度/电压 */
        BSP_Modbus_RefreshSensorHoldings();

        rc = modbus_reply(ctx, query, rc, s_mb_mapping);
        if (rc > 0) {
            if (BSP_Modbus_IsCoilWriteFunction(query[1])) {
                /* 写线圈后同步 PA3 LED 并异步落盘到 EEPROM */
                BSP_Modbus_ApplyCoilToLedPa3();
                BSP_Modbus_RequestPersistCoils();
            }
        }
    }
}

/**
 * @brief  启动 USB Modbus RTU 从站服务任务
 */
void BSP_Modbus_App_Start(void)
{
    const osThreadAttr_t attr = {
        .name = "modbusSrv",
        .stack_size = MODBUS_TASK_STACK_WORDS * 4U,
        .priority = (osPriority_t)osPriorityNormal,
    };

    if (s_modbus_server_started != 0U) {
        return;
    }

    if (s_modbus_data_ready == 0U) {
        BSP_Modbus_Data_Init();
    }

    if (s_mb_mapping == NULL) {
        return;
    }

    s_modbus_server_task = osThreadNew(ModbusServerTask, NULL, &attr);
    bsp_ota_coop_register(s_modbus_server_task);
    s_modbus_server_started = 1U;
}
