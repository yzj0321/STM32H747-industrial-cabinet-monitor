/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    freertos.c
 * @brief   CM7 FreeRTOS 应用任务：启动各 BSP 模块与 LVGL
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_log_conf.h"
#include "bsp_log.h"
#include "lvgl_demo.h"
#include "bsp_modbus_app.h"
#include "bsp_usb_modbus.h"
#include "bsp_watchdog.h"
#include "bsp_app_data.h"
#include "bsp_w5500_app.h"
#include "bsp_ota_app.h"
#include "bsp_ota_coop.h"
#include "bsp_log_sd.h"
#include "bsp_log_xfer.h"
#include "app_ipc_cm7.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartIpcTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  bsp_log_rtos_init();
  BSP_AppData_RtosInit();
  lvgl_app_init();
  lvgl_start_task();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  {
    const osThreadAttr_t ipcTask_attributes = {
      .name = "ipcTask",
      .stack_size = 2048U * 4U,
      .priority = (osPriority_t)osPriorityNormal,
    };
    (void)osThreadNew(StartIpcTask, NULL, &ipcTask_attributes);
  }
  BSP_Watchdog_Start();
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  默认任务：启动 USB/Modbus、传感器与 OTA 子系统并周期性喂狗
 * @param  argument  未使用
 *
 * 启动顺序要点：
 *   - 先起 W5500（建以太网 + OTA TCP），再 BSP_Ota_Start（SOIP 收发任务）
 *   - bsp_ota_coop_register：OTA 擦写 Flash 时挂起本任务等 peer，避免并发访问
 *   - 主循环仅喂狗；业务在各自任务中运行
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  BSP_Modbus_Data_Init();
  /* 网络与 OTA：W5500 维护链路，OTA 任务解析 SOIP */
  bsp_w5500_app_start();
  BSP_Ota_Start();
  /* SDNAND 异常日志 + 以太网文件拉取（QT :5001） */
  bsp_log_sd_start();
  bsp_log_xfer_start();

  MX_USB_DEVICE_Init();
  osDelay(500);
  BSP_USB_Modbus_Init();
  BSP_Modbus_App_Start();

  /* 注册为 OTA 协作 peer：擦写期间可被挂起 */
  bsp_ota_coop_register(defaultTaskHandle);

  for (;;)
  {
    BSP_Ota_PollCm4BootAlive();
    BSP_Watchdog_Kick(BSP_WDG_DEFAULT);
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U)
#endif

/**
 * @brief  OpenAMP Master：接收 CM4 温湿度数据
 */
void StartIpcTask(void *argument)
{
  (void)argument;

  if (app_ipc_cm7_init_master() != 0) {
    LOG_E("IPC", "master init failed");
    for (;;) {
      osDelay(1000);
    }
  }

  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  (void)HAL_HSEM_FastTake(HSEM_ID_0);
  HAL_HSEM_Release(HSEM_ID_0, 0);

  if (app_ipc_cm7_wait_endpoint() != 0) {
    LOG_W("IPC", "CM4 endpoint not ready, keep polling");
  }

  for (;;) {
    app_ipc_cm7_poll();
    osDelay(10);
  }
}

/* USER CODE END Application */

