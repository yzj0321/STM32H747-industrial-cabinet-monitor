# BSP OTA (SOIP v2 over Ethernet/TCP)

BSP_OTA_INCLUDES := \
  -I../../BSP/OTA \
  -I../../Common/Inc

BSP_OTA_SOURCES := \
  ../../BSP/OTA/bsp_ota_app.c \
  ../../BSP/OTA/bsp_ota_crc.c \
  ../../BSP/OTA/bsp_ota_flash.c \
  ../../BSP/OTA/bsp_ota_coop.c \
  ../../BSP/OTA/bsp_ota_manager.c \
  ../../BSP/OTA/bsp_ota_port.c \
  ../../BSP/OTA/bsp_ota_protocol.c \
  ../../BSP/OTA/bsp_ota_transport.c \
  ../../Common/Src/bsp_eeprom.c \
  ../../Common/Src/bsp_flash_ram.c \
  ../../Common/Src/boot_slot.c \
  ../../Common/Src/boot_eeprom.c \
  ../../Common/Src/cm4_ota_hold.c \
  ../../Common/Src/cm4_boot_handoff.c
