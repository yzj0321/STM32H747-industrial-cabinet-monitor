/**
 * @file    gt9xxx.c
 * @brief   GT9xxx 电容触摸控制器驱动（软件 I2C 轮询扫描）
 */
#include "gt9xxx.h"
#include "ctiic.h"
#include "touch.h"

#include "lcd.h"
#include "bsp_delay.h"

#include <stdio.h>
#include <string.h>

/* Default point number for most GT9xxx chips in 5-point mode. */
static uint8_t g_gt_tnum = 5;

/**
 * @brief  向 GT9xxx 寄存器写入数据
 * @param  reg  16 位寄存器地址
 * @param  buf  写入数据缓冲区
 * @param  len  写入长度
 * @retval 0 成功；非 0 I2C 错误
 */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t ret = 0;

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_WR);
    ret = ct_iic_wait_ack();
    if (ret) { ct_iic_stop(); return ret; }

    ct_iic_send_byte((uint8_t)(reg >> 8));
    ret = ct_iic_wait_ack();
    if (ret) { ct_iic_stop(); return ret; }

    ct_iic_send_byte((uint8_t)(reg & 0xFF));
    ret = ct_iic_wait_ack();
    if (ret) { ct_iic_stop(); return ret; }

    for (uint8_t i = 0; i < len; i++)
    {
        ct_iic_send_byte(buf[i]);
        ret = ct_iic_wait_ack();
        if (ret) break;
    }

    ct_iic_stop();
    return ret;
}

/**
 * @brief  从 GT9xxx 寄存器读取数据
 * @param  reg  16 位寄存器地址
 * @param  buf  输出缓冲区
 * @param  len  读取长度
 */
void gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_WR);
    ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg >> 8));
    ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg & 0xFF));
    ct_iic_wait_ack();

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_RD);
    ct_iic_wait_ack();

    for (uint8_t i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);
    }

    ct_iic_stop();
}

/**
 * @brief  初始化 GT9xxx 触摸芯片
 * @retval 0 成功；1 PID 不匹配
 */
uint8_t gt9xxx_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    GT9XXX_RST_GPIO_CLK_ENABLE();
    GT9XXX_INT_GPIO_CLK_ENABLE();

    /* RST as push-pull output */
    gpio_init_struct.Pin = GT9XXX_RST_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GT9XXX_RST_GPIO_PORT, &gpio_init_struct);

    /* INT as input (polling scan does not strictly need it) */
    gpio_init_struct.Pin = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct);

    ct_iic_init();

    /* Hardware reset (same sequence as 实验24) */
    GT9XXX_RST(0);
    delay_ms(30);
    GT9XXX_RST(1);
    delay_ms(30);

    /* INT: floating input after reset */
    gpio_init_struct.Pin = GT9XXX_INT_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GT9XXX_INT_GPIO_PORT, &gpio_init_struct);

    delay_ms(100);

    uint8_t pid_raw[5] = {0};
    gt9xxx_rd_reg(GT9XXX_PID_REG, pid_raw, 4);
    pid_raw[4] = 0;

    printf("CTP ID:%s\r\n", pid_raw);

    g_gt_tnum = 5;
    if (strcmp((char *)pid_raw, "911") && strcmp((char *)pid_raw, "9147") &&
        strcmp((char *)pid_raw, "1158") && strcmp((char *)pid_raw, "9271") &&
        strcmp((char *)pid_raw, "967"))
    {
        return 1;
    }

    if (strcmp((char *)pid_raw, "9271") == 0)
    {
        g_gt_tnum = 10;
    }

    /* Soft reset: 0x02 then 0x00 */
    uint8_t temp[1];
    temp[0] = 0x02;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);
    delay_ms(10);

    temp[0] = 0x00;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);

    return 0;
}

static const uint16_t GT9XXX_TPX_TBL[10] = {
    GT9XXX_TP1_REG,  GT9XXX_TP2_REG,  GT9XXX_TP3_REG,  GT9XXX_TP4_REG,  GT9XXX_TP5_REG,
    GT9XXX_TP6_REG,  GT9XXX_TP7_REG,  GT9XXX_TP8_REG,  GT9XXX_TP9_REG,  GT9XXX_TP10_REG
};

/**
 * @brief  轮询扫描触摸点并更新 tp_dev
 * @param  mode  扫描模式（驱动内部读写状态寄存器）
 * @retval 非 0 检测到触摸；0 无触摸
 */
uint8_t gt9xxx_scan(uint8_t mode)
{
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    uint16_t tempsta;
    static uint8_t t = 0;

    t++;

    if ((t % 10) == 0 || t < 10)
    {
        gt9xxx_rd_reg(GT9XXX_GSTID_REG, &mode, 1);

        if ((mode & 0X80) && ((mode & 0XF) <= g_gt_tnum))
        {
            i = 0;
            gt9xxx_wr_reg(GT9XXX_GSTID_REG, &i, 1);
        }

        if ((mode & 0XF) && ((mode & 0XF) <= g_gt_tnum))
        {
            temp = (uint16_t)(0XFFFFU << (mode & 0XF));
            tempsta = tp_dev.sta;
            tp_dev.sta = (uint16_t)(~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            tp_dev.x[g_gt_tnum - 1] = tp_dev.x[0];
            tp_dev.y[g_gt_tnum - 1] = tp_dev.y[0];

            for (i = 0; i < g_gt_tnum; i++)
            {
                if (tp_dev.sta & (1U << i))
                {
                    gt9xxx_rd_reg(GT9XXX_TPX_TBL[i], buf, 4);

                    if (lcddev.id == 0X5510 || lcddev.id == 0X5310 ||
                        lcddev.id == 0X7796 || lcddev.id == 0X7789 ||
                        lcddev.id == 0X9806)
                    {
                        if (tp_dev.touchtype & 0X01)
                        {
                            tp_dev.x[i] = (uint16_t)(lcddev.width - (((uint16_t)buf[3] << 8) + buf[2]));
                            tp_dev.y[i] = (uint16_t)(((uint16_t)buf[1] << 8) + buf[0]);
                        }
                        else
                        {
                            tp_dev.x[i] = (uint16_t)(((uint16_t)buf[1] << 8) + buf[0]);
                            tp_dev.y[i] = (uint16_t)(((uint16_t)buf[3] << 8) + buf[2]);
                        }
                    }
                    else
                    {
                        if (tp_dev.touchtype & 0X01)
                        {
                            tp_dev.x[i] = (uint16_t)(((uint16_t)buf[1] << 8) + buf[0]);
                            tp_dev.y[i] = (uint16_t)(((uint16_t)buf[3] << 8) + buf[2]);
                        }
                        else
                        {
                            tp_dev.x[i] = (uint16_t)(lcddev.width - (((uint16_t)buf[3] << 8) + buf[2]));
                            tp_dev.y[i] = (uint16_t)(((uint16_t)buf[1] << 8) + buf[0]);
                        }
                    }
                }
            }

            res = 1;

            if (tp_dev.x[0] > lcddev.width || tp_dev.y[0] > lcddev.height)
            {
                if ((mode & 0XF) > 1)
                {
                    tp_dev.x[0] = tp_dev.x[1];
                    tp_dev.y[0] = tp_dev.y[1];
                    t = 0;
                }
                else
                {
                    tp_dev.x[0] = tp_dev.x[g_gt_tnum - 1];
                    tp_dev.y[0] = tp_dev.y[g_gt_tnum - 1];
                    mode = 0X80;
                    tp_dev.sta = tempsta;
                }
            }
            else
            {
                t = 0;
            }
        }
    }

    if ((mode & 0X8F) == 0X80)
    {
        if (tp_dev.sta & TP_PRES_DOWN)
        {
            tp_dev.sta &= ~TP_PRES_DOWN;
        }
        else
        {
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
            tp_dev.sta &= 0XE000;
        }
    }

    if (t > 240)
    {
        t = 10;
    }

    return res;
}
