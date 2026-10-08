/**
 * @file    bsp_modbus_th.c
 * @brief   通过 Modbus RTU 读取温湿度（功能码 04，输入寄存器 0000H/0001H）
 */
#include "bsp_modbus_th.h"

#include "bsp_rs485.h"
#include "bsp_rs485_conf.h"
#include "modbus.h"
#include "app_ipc_proto.h"

#include "cmsis_os2.h"

#include <errno.h>

#if defined(CORE_CM4)
#include "app_ipc_cm4.h"
#endif

#define MODBUS_TH_TASK_STACK_WORDS 512U

static modbus_t *s_mb_ctx;

/**
 * @brief  解析温湿度模块温度寄存器
 */
static int16_t th_parse_temperature_x10(uint16_t raw)
{
    if (raw >= 10000U) {
        return (int16_t)(-((int32_t)(raw - 10000U)));
    }

    return (int16_t)raw;
}

/**
 * @brief  解析温湿度模块湿度寄存器
 */
static uint16_t th_parse_humidity_x10(uint16_t raw)
{
    return raw;
}

/**
 * @brief  初始化温湿度 Modbus
 */
int BSP_Modbus_TH_Init(void)
{
    BSP_Rs485_Init();

    s_mb_ctx = modbus_new_st_rtu("rs485",
                                 BSP_RS485_MODBUS_BAUD,
                                 BSP_RS485_MODBUS_PARITY,
                                 BSP_RS485_MODBUS_DATA_BITS,
                                 BSP_RS485_MODBUS_STOP_BITS);
    if (s_mb_ctx == NULL) {
        return -1;
    }

    modbus_set_slave(s_mb_ctx, (int)BSP_RS485_MODBUS_SLAVE_ID);
    modbus_set_response_timeout(s_mb_ctx, 0, 500000);
    modbus_set_byte_timeout(s_mb_ctx, 0, 20000);

    if (modbus_connect(s_mb_ctx) == -1) {
        (void)app_ipc_cm4_send_sensor_status(APP_IPC_SENSOR_STATUS_READ_FAIL,
                                             (uint32_t)errno, 0U, 0U);
        modbus_free(s_mb_ctx);
        s_mb_ctx = NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief  读取温湿度采样
 */
int BSP_Modbus_TH_Read(bsp_th_sample_t *sample)
{
    uint16_t regs[2];
    int rc;

    if ((sample == NULL) || (s_mb_ctx == NULL)) {
        return -1;
    }

    sample->valid = 0U;

    rc = modbus_read_input_registers(s_mb_ctx,
                                     BSP_RS485_MODBUS_REG_TEMP,
                                     2,
                                     regs);
    if (rc != 2) {
        (void)app_ipc_cm4_send_sensor_status(APP_IPC_SENSOR_STATUS_READ_FAIL,
                                             (uint32_t)errno, 0U, 0U);
        return -1;
    }

    sample->temperature_x10 = th_parse_temperature_x10(regs[0]);
    sample->humidity_x10 = th_parse_humidity_x10(regs[1]);
    sample->valid = 1U;
    (void)app_ipc_cm4_send_sensor_status(APP_IPC_SENSOR_STATUS_READ_OK,
                                         0U, regs[0], regs[1]);
    return 0;
}

#if defined(CORE_CM4)
/**
 * @brief  温湿度周期采集任务
 */
static void ModbusThTask(void *argument)
{
    uint32_t last_poll_ms = 0U;

    (void)argument;

    if (BSP_Modbus_TH_Init() != 0) {
        for (;;) {
            osDelay(1000);
        }
    }

    for (;;) {
        uint32_t now = osKernelGetTickCount();

        if ((now - last_poll_ms) >= BSP_RS485_POLL_INTERVAL_MS) {
            bsp_th_sample_t sample;

            last_poll_ms = now;
            if (BSP_Modbus_TH_Read(&sample) == 0) {
                int16_t t_x10 = sample.temperature_x10;
                uint16_t h_x10 = sample.humidity_x10;
                uint16_t t_abs = (uint16_t)((t_x10 < 0) ? -t_x10 : t_x10);

                (void)app_ipc_cm4_send_sensor((uint16_t)(t_abs / 10U),
                                              (uint16_t)(h_x10 / 10U));
            }
        }

        osDelay(10);
    }
}
#endif

/**
 * @brief  启动温湿度采集任务
 */
void BSP_Modbus_TH_Start(void)
{
#if defined(CORE_CM4)
    const osThreadAttr_t attr = {
        .name = "modbus_th",
        .stack_size = MODBUS_TH_TASK_STACK_WORDS * sizeof(uint32_t),
        .priority = osPriorityNormal,
    };

    (void)osThreadNew(ModbusThTask, NULL, &attr);
#else
    (void)0;
#endif
}
