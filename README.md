# STM32H747 工业机柜环境监护系统

本仓库包含工业机柜环境监护系统的三个主要工程：

- `stm32h7-machine-controller/`：STM32H747 双核应用，CM4 负责 RS485/ADC 采集，CM7 负责 LVGL、USB Modbus、SD 日志、W5500 和 OTA 业务。
- `Bootloader/`：CM7/CM4 启动与 A/B 槽位管理工程。
- `OTA/`：Qt/C++ 上位机，支持 TCP/串口 OTA、Modbus 监控和日志下载。

## 主要架构

CM4 采集温湿度与水浸数据，经 OpenAMP/RPMsg 通道 `cabinet_ipc` 发送给 CM7；CM7 写入 `BSP_AppData`，再提供给 LCD/LVGL、USB Modbus、SD 日志和网络模块。

## 固件布局

| 区域 | 地址 |
|---|---|
| CM7 Bootloader | `0x08000000` |
| CM7 App A/B | `0x08020000` / `0x080A0000` |
| CM4 Stub | `0x08100000` |
| CM4 App A/B | `0x08120000` / `0x081A0000` |

## 说明

仓库只提交源码、工程配置和必要文档。编译输出、第三方静态库、IDE 用户配置和临时文件已排除；这些文件可以通过 Makefile 或 Qt 工程重新生成。
