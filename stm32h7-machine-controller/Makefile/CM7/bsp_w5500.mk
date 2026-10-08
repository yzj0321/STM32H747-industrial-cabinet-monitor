# BSP W5500 Ethernet — keep this file when regenerating Makefile from CubeMX

BSP_W5500_INCLUDES := \
  -I../../BSP/W5500 \
  -I../../BSP/W5500/driver

BSP_W5500_SOURCES := \
  ../../BSP/W5500/driver/w5500.c \
  ../../BSP/W5500/driver/wizchip_conf.c \
  ../../BSP/W5500/driver/socket.c \
  ../../BSP/W5500/bsp_w5500_net.c \
  ../../BSP/W5500/bsp_w5500_tcp.c \
  ../../BSP/W5500/bsp_w5500_app.c
