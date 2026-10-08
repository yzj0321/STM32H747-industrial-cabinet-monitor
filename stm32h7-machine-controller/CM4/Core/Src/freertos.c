/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    freertos.c
 * @brief   CM4 FreeRTOS：ipcTask 轮询 OpenAMP + OTA hold + 传感器任务
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
#include "app_ipc_cm4.h"
#include "bsp_adc_immersion.h"
#include "bsp_modbus_th.h"
#include "cm4_ota_hold.h"
#include "cm4_boot_handoff.h"
#include "boot_slot.h"
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
osThreadId_t ipcTaskHandle;
const osThreadAttr_t ipcTask_attributes = {
  .name = "ipcTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartIpcTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

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
  ipcTaskHandle = osThreadNew(StartIpcTask, NULL, &ipcTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  (void)argument;

  BSP_Modbus_TH_Start();
  BSP_AdcImmersion_Start();
  cm4_ipc_diag_set(CM4_IPC_DIAG_DEFAULT_READY);

#if defined(CM4_APP_IN_SLOT)
  cm4_boot_alive_signal(cm4_boot_slot_detect_running());
#endif

  for (;;) {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
/**
 * @brief  OpenAMP Remote + OTA hold（对齐 CamView ipcTask）
 */
void StartIpcTask(void *argument)
{
  (void)argument;

  cm4_ipc_diag_set(CM4_IPC_DIAG_IPC_START);

  if (app_ipc_cm4_init() != 0) {
    cm4_ipc_diag_set(CM4_IPC_DIAG_IPC_FAIL);
    for (;;) {
      (void)cm4_ota_hold_service();
      osDelay(50);
    }
  }

  cm4_ipc_diag_set(CM4_IPC_DIAG_IPC_READY);

  for (;;) {
    (void)cm4_ota_hold_service();
    app_ipc_cm4_poll();
    osDelay(1);
  }
}
/* USER CODE END Application */

