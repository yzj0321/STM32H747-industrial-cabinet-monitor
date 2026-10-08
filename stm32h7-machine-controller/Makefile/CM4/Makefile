##########################################################################################################################
# CM4 minimal wake-from-stop stub — keep only CM4/Core + required HAL.
# CubeMX regen may re-add CM7/FATFS/SDMMC/USB/LVGL sources; remove them again.
##########################################################################################################################

# ------------------------------------------------
# Generic Makefile (based on gcc)
# ------------------------------------------------

######################################
# target / stub + A-B slot (CM4 OTA)
######################################
CM4_IMAGE ?= APP
APP_SLOT ?= A
APP_SLOT_NAME := $(patsubst a,A,$(patsubst b,B,$(APP_SLOT)))

ifeq ($(CM4_IMAGE),STUB)
TARGET = GPIO_CM4_STUB
BUILD_DIR = build_stub
LDSCRIPT = stm32h747xx_flash_CM4_stub.ld
else ifeq ($(APP_SLOT_NAME),A)
TARGET = GPIO_CM4_A
BUILD_DIR = build_A
LDSCRIPT = stm32h747xx_flash_CM4.ld
APP_FLASH_ORIGIN = 0x08120000
else ifeq ($(APP_SLOT_NAME),B)
TARGET = GPIO_CM4_B
BUILD_DIR = build_B
LDSCRIPT = stm32h747xx_flash_CM4_slotB.ld
APP_FLASH_ORIGIN = 0x081A0000
else
$(error APP_SLOT must be A or B)
endif

######################################
# building variables
######################################
DEBUG = 1
OPT = -Og

include ../CM7/bsp_rs485.mk
include rs485_modbus.mk
include bsp_adc_immersion.mk

CM4_COMMON_SOURCES = \
../../Common/Src/cm4_ota_hold.c \
../../Common/Src/cm4_boot_handoff.c \
../../Common/Src/boot_slot.c

CM4_STUB_SOURCES = \
../../CM4/Core/Src/cm4_stub_main.c \
../../CM4/Core/Src/cm4_stub_it.c \
../../CM4/Core/Src/stm32h7xx_hal_msp.c \
../../CM4/Core/Src/stm32h7xx_hal_timebase_tim.c \
../../CM4/Core/Src/sysmem.c \
../../CM4/Core/Src/syscalls.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c \
../../Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c \
$(CM4_COMMON_SOURCES)

CM4_APP_SOURCES = \
../../CM4/Core/Src/main.c \
../../CM4/Core/Src/adc.c \
../../CM4/Core/Src/dma.c \
../../CM4/Core/Src/stm32h7xx_it.c \
../../CM4/Core/Src/stm32h7xx_hal_msp.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_adc.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_adc_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c \
../../Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c \
../../CM4/Core/Src/sysmem.c \
../../CM4/Core/Src/syscalls.c \
../../CM4/Core/Src/gpio.c \
../../CM4/Core/Src/freertos.c \
../../CM4/APP/IPC/app_ipc_cm4.c \
$(BSP_RS485_SOURCES) \
$(RS485_MODBUS_SOURCES) \
$(BSP_ADC_IMM_SOURCES) \
../../CM4/Core/Src/usart.c \
../../CM4/Core/Src/stm32h7xx_hal_timebase_tim.c \
../../CM4/OPENAMP/mbox_hsem.c \
../../CM4/OPENAMP/openamp.c \
../../CM4/OPENAMP/rsc_table.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c \
../../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c \
../../Middlewares/Third_Party/FreeRTOS/Source/portable/Common/mpu_wrappers_v2.c \
../../Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
../../Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
../../Middlewares/Third_Party/FreeRTOS/Source/list.c \
../../Middlewares/Third_Party/FreeRTOS/Source/queue.c \
../../Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
../../Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
../../Middlewares/Third_Party/FreeRTOS/Source/timers.c \
../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c \
../../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/device.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/init.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/io.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/log.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/shmem.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/condition.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/generic_device.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/generic_init.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/generic_io.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/generic_shmem.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/time.c \
../../Middlewares/Third_Party/OpenAMP/open-amp/lib/remoteproc/remoteproc_virtio.c \
../../Middlewares/Third_Party/OpenAMP/open-amp/lib/rpmsg/rpmsg.c \
../../Middlewares/Third_Party/OpenAMP/open-amp/lib/rpmsg/rpmsg_virtio.c \
../../Middlewares/Third_Party/OpenAMP/open-amp/lib/virtio/virtio.c \
../../Middlewares/Third_Party/OpenAMP/open-amp/lib/virtio/virtqueue.c \
../../Middlewares/Third_Party/OpenAMP/libmetal/lib/system/generic/cortexm/sys.c \
$(CM4_COMMON_SOURCES)

ifeq ($(CM4_IMAGE),STUB)
C_SOURCES = $(CM4_STUB_SOURCES)
ASM_SOURCES = startup_stm32h747xx_CM4.s
else
C_SOURCES = $(CM4_APP_SOURCES)
ASM_SOURCES = startup_stm32h747xx_CM4.s
endif

ASMM_SOURCES =

#######################################
# binaries
#######################################
PREFIX = arm-none-eabi-
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

#######################################
# CFLAGS
#######################################
CPU = -mcpu=cortex-m4
FPU = -mfpu=fpv4-sp-d16
FLOAT-ABI = -mfloat-abi=hard
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

AS_DEFS =

C_DEFS = \
-DCORE_CM4 \
-DUSE_HAL_DRIVER \
-DSTM32H747xx \
-DUSE_PWR_LDO_SUPPLY \
-DMETAL_INTERNAL \
-DMETAL_MAX_DEVICE_REGIONS=2 \
-DRPMSG_BUFFER_SIZE=512 \
-DVIRTIO_SLAVE_ONLY \
-DNO_ATOMIC_64_SUPPORT

ifeq ($(CM4_IMAGE),STUB)
C_DEFS += -DAPP_FLASH_ORIGIN=0x08100000
else ifeq ($(CM4_IMAGE),APP)
C_DEFS += -DAPP_FLASH_ORIGIN=$(APP_FLASH_ORIGIN)
C_DEFS += -DCM4_APP_IN_SLOT
ifeq ($(APP_SLOT_NAME),B)
C_DEFS += -DAPP_SLOT_B
endif
endif

AS_INCLUDES =

C_INCLUDES = \
-I../../CM4/Core/Inc \
-I../../Common/Inc \
-I../../CM4/APP/IPC \
$(BSP_RS485_INCLUDES) \
$(RS485_MODBUS_INCLUDES) \
$(BSP_ADC_IMM_INCLUDES) \
-I../../Drivers/STM32H7xx_HAL_Driver/Inc \
-I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy \
-I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include \
-I../../Drivers/CMSIS/Include \
-I../../CM4/OPENAMP \
-I../../Middlewares/Third_Party/OpenAMP/open-amp/lib/include \
-I../../Middlewares/Third_Party/OpenAMP/libmetal/lib/include \
-I../../Middlewares/Third_Party/OpenAMP/libmetal/lib/include/metal/compiler/gcc \
-I../../Middlewares/Third_Party/OpenAMP/open-amp/lib/rpmsg \
-I../../Middlewares/Third_Party/FreeRTOS/Source/include \
-I../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
-I../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
-I../../Drivers/CMSIS/RTOS2/Include

ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS += $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

#######################################
# LDFLAGS
#######################################
LIBS = -lc -lm -lnosys
LIBDIR =
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

.PHONY: all all-images stub app-a app-b clean

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

all-images:
	$(MAKE) CM4_IMAGE=STUB
	$(MAKE) CM4_IMAGE=APP APP_SLOT=A
	$(MAKE) CM4_IMAGE=APP APP_SLOT=B

stub:
	$(MAKE) CM4_IMAGE=STUB

app-a:
	$(MAKE) CM4_IMAGE=APP APP_SLOT=A

app-b:
	$(MAKE) CM4_IMAGE=APP APP_SLOT=B

#######################################
# build the application
#######################################
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASMM_SOURCES:.S=.o)))
vpath %.S $(sort $(dir $(ASMM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.S Makefile | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(HEX) $< $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	$(BIN) $< $@

$(BUILD_DIR):
	if not exist "$@" mkdir "$@"

clean:
	-rm -fR build_stub build_A build_B build

-include $(wildcard $(BUILD_DIR)/*.d)

# *** EOF ***
