/**
 * @file    ctiic.h
 * @brief   电容触摸屏软件 I2C（GPIO 位bang）接口
 */
#ifndef __CTIIC_H
#define __CTIIC_H

#include "main.h"

/* Touch controller I2C pins (from your wiring diagram) */
#define CT_IIC_SCL_GPIO_PORT            GPIOB
#define CT_IIC_SCL_GPIO_PIN             GPIO_PIN_10
#define CT_IIC_SCL_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

#define CT_IIC_SDA_GPIO_PORT            GPIOB
#define CT_IIC_SDA_GPIO_PIN             GPIO_PIN_11
#define CT_IIC_SDA_GPIO_CLK_ENABLE()   do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)

/* I2C line operations.
 * SDA is configured as open-drain, so writing 1 releases the line.
 */
#define CT_IIC_SCL(x) do{ \
    if ((x) != 0) HAL_GPIO_WritePin(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN, GPIO_PIN_SET); \
    else           HAL_GPIO_WritePin(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN, GPIO_PIN_RESET); \
}while(0)

#define CT_IIC_SDA(x) do{ \
    if ((x) != 0) HAL_GPIO_WritePin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN, GPIO_PIN_SET); \
    else           HAL_GPIO_WritePin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN, GPIO_PIN_RESET); \
}while(0)

#define CT_READ_SDA  HAL_GPIO_ReadPin(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN)

/**
 * @brief  初始化软件 I2C GPIO
 */
void ct_iic_init(void);

/**
 * @brief  产生 I2C 起始条件
 */
void ct_iic_start(void);

/**
 * @brief  产生 I2C 停止条件
 */
void ct_iic_stop(void);

/**
 * @brief  等待从机 ACK
 * @retval 0 收到 ACK；1 超时无 ACK
 */
uint8_t ct_iic_wait_ack(void);

/**
 * @brief  主机发送 ACK
 */
void ct_iic_ack(void);

/**
 * @brief  主机发送 NACK
 */
void ct_iic_nack(void);

/**
 * @brief  发送一个字节（MSB 先发）
 * @param  data  待发送字节
 */
void ct_iic_send_byte(uint8_t data);

/**
 * @brief  读取一个字节
 * @param  ack  非 0 读完后发 ACK，0 发 NACK
 * @retval 读取的字节
 */
uint8_t ct_iic_read_byte(uint8_t ack);

#endif /* __CTIIC_H */
