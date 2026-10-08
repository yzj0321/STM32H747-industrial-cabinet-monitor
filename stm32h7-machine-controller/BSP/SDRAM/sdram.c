/**
 * @file    sdram.c
 * @brief   SDRAM 驱动（CubeMX FMC + 上电初始化时序）
 */
#include "sdram.h"
#include <stdio.h>

/**
 * @brief  SDRAM 初始化（上电时序 + 刷新率配置，在 MX_FMC_Init 之后调用）
 */
void sdram_init(void)
{
    sdram_initialization_sequence();
    HAL_SDRAM_ProgramRefreshRate(&hsdram2, SDRAM_REFRESH_COUNT);
}

/**
 * @brief  SDRAM 控制器模式寄存器初始化命令序列
 */
void sdram_initialization_sequence(void)
{
    uint32_t temp = 0;

    sdram_send_cmd(1, FMC_SDRAM_CMD_CLK_ENABLE, 1, 0);
    HAL_Delay(1);

    sdram_send_cmd(1, FMC_SDRAM_CMD_PALL, 1, 0);
    sdram_send_cmd(1, FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0);

    temp = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
           SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
           SDRAM_MODEREG_CAS_LATENCY_2 |
           SDRAM_MODEREG_OPERATING_MODE_STANDARD |
           SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    sdram_send_cmd(1, FMC_SDRAM_CMD_LOAD_MODE, 1, (uint16_t)temp);
}

/**
 * @brief  向 SDRAM 发送 FMC 命令
 * @param  bankx    0: Bank1，1: Bank2
 * @param  cmd      命令码
 * @param  refresh  自动刷新次数
 * @param  regval   模式寄存器值
 * @retval 0 成功；1 失败
 */
uint8_t sdram_send_cmd(uint8_t bankx, uint8_t cmd, uint8_t refresh, uint16_t regval)
{
    uint32_t target_bank;
    FMC_SDRAM_CommandTypeDef command;

    if (bankx == 0)
    {
        target_bank = FMC_SDRAM_CMD_TARGET_BANK1;
    }
    else
    {
        target_bank = FMC_SDRAM_CMD_TARGET_BANK2;
    }

    command.CommandMode = cmd;
    command.CommandTarget = target_bank;
    command.AutoRefreshNumber = refresh;
    command.ModeRegisterDefinition = regval;

    if (HAL_SDRAM_SendCommand(&hsdram2, &command, 0x1000U) == HAL_OK)
    {
        return 0;
    }

    return 1;
}

/**
 * @brief  探测 SDRAM 容量（调试用，写入递增模式）
 */
void sdram_test(void)
{
    uint32_t i;
    uint32_t temp = 0;
    uint32_t sval = 0;

    for (i = 0; i < 32U * 1024U * 1024U; i += 16U * 1024U)
    {
        *(volatile uint32_t *)(BANK6_SDRAM_ADDR + i) = temp;
        temp++;
    }

    for (i = 0; i < 32U * 1024U * 1024U; i += 16U * 1024U)
    {
        temp = *(volatile uint32_t *)(BANK6_SDRAM_ADDR + i);

        if (i == 0U)
        {
            sval = temp;
        }
        else if (temp <= sval)
        {
            break;
        }

        printf("SDRAM Capacity: %lu KB\r\n", (unsigned long)((temp - sval + 1U) * 16U));
    }
}
