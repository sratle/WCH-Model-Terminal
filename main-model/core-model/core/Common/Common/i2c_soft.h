/********************************** (C) COPYRIGHT *******************************
 * File Name          : i2c_soft.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Software I2C library for CH32H417.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/
#ifndef __I2C_SOFT_H
#define __I2C_SOFT_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_gpio.h"

/* Software I2C port configuration */
#define SOFT_I2C_SCL_PORT   GPIOB
#define SOFT_I2C_SCL_PIN    GPIO_Pin_6
#define SOFT_I2C_SDA_PORT   GPIOB
#define SOFT_I2C_SDA_PIN    GPIO_Pin_7

void SoftI2C_Init(void);
void SoftI2C_Start(void);
void SoftI2C_Stop(void);
uint8_t SoftI2C_WaitAck(void);
void SoftI2C_Ack(void);
void SoftI2C_NAck(void);
void SoftI2C_SendByte(uint8_t txd);
uint8_t SoftI2C_ReadByte(uint8_t ack);

uint8_t SoftI2C_WriteReg(uint8_t dev_addr, uint32_t reg_addr, uint8_t dat);
uint8_t SoftI2C_ReadReg(uint8_t dev_addr, uint32_t reg_addr, uint8_t *dat);

#ifdef __cplusplus
}
#endif

#endif
