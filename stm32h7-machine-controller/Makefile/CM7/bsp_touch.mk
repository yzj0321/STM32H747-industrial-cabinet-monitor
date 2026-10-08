# BSP touch module (GT9xxx / FT5206 via software I2C)

BSP_TOUCH_INCLUDES := \
  -I../../BSP/TOUCH

BSP_TOUCH_SOURCES := \
  ../../BSP/TOUCH/touch.c \
  ../../BSP/TOUCH/ctiic.c \
  ../../BSP/TOUCH/gt9xxx.c \
  ../../BSP/TOUCH/ft5206.c
