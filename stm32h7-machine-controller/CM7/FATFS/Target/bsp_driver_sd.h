/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    bsp_driver_sd.h (based on stm32h743i_eval_sd.h)
  * @brief   This file contains the common defines and functions prototypes for
  *          the bsp_driver_sd.c driver.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32H7_SD_H
#define __STM32H7_SD_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Exported types --------------------------------------------------------*/
/**
  * @brief SD Card information structure
  */
#define BSP_SD_CardInfo HAL_SD_CardInfoTypeDef

/* Exported constants --------------------------------------------------------*/
/**
  * @brief  SD status structure definition
  */
#define   MSD_OK                        ((uint8_t)0x00)
#define   MSD_ERROR                     ((uint8_t)0x01)
#define   MSD_ERROR_SD_NOT_PRESENT      ((uint8_t)0x02)

/**
  * @brief  SD transfer state definition
  */
#define   SD_TRANSFER_OK                ((uint8_t)0x00)
#define   SD_TRANSFER_BUSY              ((uint8_t)0x01)

#define SD_PRESENT               ((uint8_t)0x01)
#define SD_NOT_PRESENT           ((uint8_t)0x00)
#define SD_DATATIMEOUT           ((uint32_t)100000000)

/* USER CODE BEGIN BSP_H_CODE */
#define SD_DetectIRQHandler()             HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8)

/* Exported functions --------------------------------------------------------*/
/** @brief 初始化 SDMMC/SDNAND */
uint8_t BSP_SD_Init(void);
/** @brief 配置 SD 相关中断 */
uint8_t BSP_SD_ITConfig(void);
/** @brief 轮询读多块 */
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout);
/** @brief 轮询写多块 */
uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout);
/** @brief DMA 读多块 */
uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks);
/** @brief DMA 写多块 */
uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks);
/** @brief 擦除地址范围 */
uint8_t BSP_SD_Erase(uint32_t StartAddr, uint32_t EndAddr);
/** @brief 查询卡传输状态 */
uint8_t BSP_SD_GetCardState(void);
/** @brief 获取卡信息 */
void    BSP_SD_GetCardInfo(BSP_SD_CardInfo *CardInfo);
/** @brief 检测卡是否在位（SDNAND 固定在位） */
uint8_t BSP_SD_IsDetected(void);

/* These functions can be modified in case the current settings (e.g. DMA stream)
   need to be changed for specific application needs */
/** @brief 传输中止回调 */
void    BSP_SD_AbortCallback(void);
/** @brief 写完成回调 */
void    BSP_SD_WriteCpltCallback(void);
/** @brief 读完成回调 */
void    BSP_SD_ReadCpltCallback(void);
/* USER CODE END BSP_H_CODE */

#ifdef __cplusplus
}
#endif

#endif /* __STM32H7_SD_H */
