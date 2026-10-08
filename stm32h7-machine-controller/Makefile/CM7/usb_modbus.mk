# USB Device (CDC) + Modbus RTU over USB — keep this file when regenerating Makefile from CubeMX
# USB_DEVICE lives under CM7/ (CubeMX M7 output).

USB_MODBUS_INCLUDES := \
  -I../../CM7/USB_DEVICE/App \
  -I../../CM7/USB_DEVICE/Target \
  -I../../BSP/Modbus \
  -I../../Middlewares/Third_Party/libmodbus \
  -I../../Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
  -I../../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc

USB_MODBUS_SOURCES := \
  ../../CM7/USB_DEVICE/Target/usbd_conf.c \
  ../../CM7/USB_DEVICE/App/usb_device.c \
  ../../CM7/USB_DEVICE/App/usbd_desc.c \
  ../../CM7/USB_DEVICE/App/usbd_cdc_if.c \
  ../../BSP/Modbus/bsp_usb_modbus.c \
  ../../BSP/Modbus/uart_device.c \
  ../../BSP/Modbus/uart_device_usb.c \
  ../../BSP/Modbus/bsp_modbus_app.c \
  ../../BSP/Modbus/bsp_modbus_coil_nv.c \
  ../../Middlewares/Third_Party/libmodbus/modbus.c \
  ../../Middlewares/Third_Party/libmodbus/modbus-data.c \
  ../../Middlewares/Third_Party/libmodbus/modbus-st-rtu.c \
  ../../Middlewares/Third_Party/libmodbus/errno.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd_ex.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usb.c \
  ../../Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
  ../../Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
  ../../Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
  ../../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c
