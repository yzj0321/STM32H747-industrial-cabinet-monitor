/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CM4 Boot Stub — 固定入口，跳转 Slot A/B
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cm4_boot_handoff.h"
#include "boot_slot.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U)
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  CM4 Stub：读 D3 握手字，跳转到 App Slot A/B（成功不返回）
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* 不进 STOP：CM7 已在 EnableBootCore 前写好 handoff；
   * App 自己会再进 STOP 等 HSEM。 */
  if (cm4_boot_handoff_jump_from_d3() != 0) {
    if (cm4_boot_slot_is_valid(CM4_APP_SLOT_A_INDEX) != 0) {
      (void)cm4_boot_handoff_jump(CM4_APP_SLOT_A_BASE);
    }
    if (cm4_boot_slot_is_valid(CM4_APP_SLOT_B_INDEX) != 0) {
      (void)cm4_boot_handoff_jump(CM4_APP_SLOT_B_BASE);
    }
  }

  /* 跳转失败：停在此，便于调试 */
  __disable_irq();
  while (1)
  {
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
