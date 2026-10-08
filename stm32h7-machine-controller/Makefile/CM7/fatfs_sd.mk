# SDMMC2 + FatFs (SDNAND) — keep this file when regenerating Makefile from CubeMX
# CubeMX must not re-append these sources into Makefile/CM7/Makefile C_SOURCES.

FATFS_SD_INCLUDES := \
  -I../../CM7/FATFS/App \
  -I../../CM7/FATFS/Target \
  -I../../Middlewares/Third_Party/FatFs/src

FATFS_SD_SOURCES := \
  ../../CM7/Core/Src/sdmmc.c \
  ../../CM7/FATFS/App/fatfs.c \
  ../../CM7/FATFS/Target/bsp_driver_sd.c \
  ../../CM7/FATFS/Target/sd_diskio.c \
  ../../Middlewares/Third_Party/FatFs/src/diskio.c \
  ../../Middlewares/Third_Party/FatFs/src/ff.c \
  ../../Middlewares/Third_Party/FatFs/src/ff_gen_drv.c \
  ../../Middlewares/Third_Party/FatFs/src/option/syscall.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_sdmmc.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_delayblock.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sd.c \
  ../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sd_ex.c
