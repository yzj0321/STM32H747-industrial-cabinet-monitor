/**
 * @file    bsp_usb_modbus.c
 * @brief   USB CDC 与 Modbus RTU 之间的收发队列桥接
 */
#include "bsp_usb_modbus.h"

#include "FreeRTOS.h"
#include "main.h"
#include <string.h>
#include "queue.h"
#include "task.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

#define USB_MODBUS_RX_QUEUE_LEN 1024U
#define USB_MODBUS_TX_BUF_SIZE  260U

static QueueHandle_t s_rx_queue;
static uint8_t s_tx_buf[USB_MODBUS_TX_BUF_SIZE];

/**
 * @brief  等待 USB CDC 发送空闲
 * @param  timeout_ms  超时时间（毫秒）
 * @retval 0 就绪；-1 超时或错误
 */
static int usb_wait_tx_idle(uint32_t timeout_ms)
{
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
  uint32_t start = HAL_GetTick();

  if (hcdc == NULL)
  {
    return -1;
  }

  while (hcdc->TxState != 0U)
  {
    if ((HAL_GetTick() - start) >= timeout_ms)
    {
      return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(1U));
  }

  return 0;
}

/**
 * @brief  初始化 USB Modbus 接收队列
 */
void BSP_USB_Modbus_Init(void)
{
  s_rx_queue = xQueueCreate(USB_MODBUS_RX_QUEUE_LEN, sizeof(uint8_t));
}

/**
 * @brief  USB CDC 接收回调（ISR 上下文入队）
 * @param  buf  接收数据指针
 * @param  len  数据长度
 */
void BSP_USB_Modbus_OnRx(uint8_t *buf, uint32_t len)
{
  uint32_t i;
  BaseType_t woken = pdFALSE;

  if ((buf == NULL) || (len == 0U) || (s_rx_queue == NULL))
  {
    return;
  }

  for (i = 0U; i < len; i++)
  {
    if (xQueueSendFromISR(s_rx_queue, &buf[i], &woken) != pdTRUE)
    {
      break;
    }
  }

  portYIELD_FROM_ISR(woken);
}

/**
 * @brief  USB CDC 发送完成回调（预留扩展）
 */
void BSP_USB_Modbus_OnTxComplete(void)
{
  /* 发送完成由 TxState 轮询，此处保留供扩展 */
}

/**
 * @brief  通过 USB CDC 发送数据
 * @param  data        待发送数据
 * @param  len         数据长度
 * @param  timeout_ms  超时时间（毫秒）
 * @retval 0 成功；-1 失败
 */
int BSP_USB_Modbus_Send(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
  uint32_t start_tick;

  if ((data == NULL) || (len == 0U) || (len > USB_MODBUS_TX_BUF_SIZE))
  {
    return -1;
  }

  if (usb_wait_tx_idle(timeout_ms) != 0)
  {
    return -1;
  }

  memcpy(s_tx_buf, data, len);

  start_tick = HAL_GetTick();
  while ((HAL_GetTick() - start_tick) < timeout_ms)
  {
    if (CDC_Transmit_FS(s_tx_buf, (uint16_t)len) == USBD_OK)
    {
      if (usb_wait_tx_idle(timeout_ms) == 0)
      {
        return 0;
      }
      return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(1U));
  }

  return -1;
}

/**
 * @brief  从接收队列读取一字节
 * @param  data        输出字节指针
 * @param  timeout_ms  超时时间（毫秒）
 * @retval 0 成功；-1 超时或错误
 */
int BSP_USB_Modbus_RecvByte(uint8_t *data, int timeout_ms)
{
  if ((data == NULL) || (s_rx_queue == NULL))
  {
    return -1;
  }

  if (xQueueReceive(s_rx_queue, data, pdMS_TO_TICKS((uint32_t)timeout_ms)) == pdTRUE)
  {
    return 0;
  }

  return -1;
}

/**
 * @brief  清空接收队列
 * @retval 丢弃的字节数
 */
int BSP_USB_Modbus_Flush(void)
{
  uint8_t data;
  int cnt = 0;

  if (s_rx_queue == NULL)
  {
    return 0;
  }

  while (xQueueReceive(s_rx_queue, &data, 0) == pdTRUE)
  {
    cnt++;
  }

  return cnt;
}
