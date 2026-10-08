/**
 * @file    uart_device.c
 * @brief   UART 设备注册表与按名查找
 */
#include "uart_device.h"

#include <string.h>

#if defined(CORE_CM4)
extern struct UART_Device g_rs485_dev;
static struct UART_Device *g_uart_devices[] = {&g_rs485_dev};
#elif defined(CORE_CM7)
extern struct UART_Device g_usbserial_dev;
static struct UART_Device *g_uart_devices[] = {&g_usbserial_dev};
#endif

/**
 * @brief  按名称查找 UART 设备
 * @param  name  设备名称字符串
 * @retval 设备指针，未找到返回 NULL
 */
struct UART_Device *GetUARTDevice(char *name)
{
  size_t i;

  for (i = 0U; i < (sizeof(g_uart_devices) / sizeof(g_uart_devices[0])); i++)
  {
    if (strcmp(name, g_uart_devices[i]->name) == 0)
    {
      return g_uart_devices[i];
    }
  }

  return NULL;
}
