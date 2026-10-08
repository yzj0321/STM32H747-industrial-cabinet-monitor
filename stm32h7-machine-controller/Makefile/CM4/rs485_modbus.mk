# Modbus RTU master over RS485 (libmodbus) — CM4 only

RS485_MODBUS_INCLUDES := \
  -I../../BSP/Modbus \
  -I../../Middlewares/Third_Party/libmodbus

RS485_MODBUS_SOURCES := \
  ../../BSP/Modbus/bsp_modbus_th.c \
  ../../BSP/Modbus/uart_device.c \
  ../../BSP/Modbus/uart_device_rs485.c \
  ../../Middlewares/Third_Party/libmodbus/modbus.c \
  ../../Middlewares/Third_Party/libmodbus/modbus-data.c \
  ../../Middlewares/Third_Party/libmodbus/modbus-st-rtu.c \
  ../../Middlewares/Third_Party/libmodbus/errno.c
