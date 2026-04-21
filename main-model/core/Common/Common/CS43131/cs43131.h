/********************************** (C) COPYRIGHT *******************************
 * File Name          : cs43131.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : CS43131 DAC driver header.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/
#ifndef __CS43131_H__
#define __CS43131_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_gpio.h"

#define AUDIO_BUFFER_SIZE   256
#define SPI2_TX_DMA_REQUEST 65
#define CS43131_I2C_ADDR    0x30

/* CS43131 GPIO definitions NOTE: SPI2==I2S1*/
#define CS43131_RST_GPIO_PORT   GPIOB
#define CS43131_RST_PIN         GPIO_Pin_3
#define CS43131_OE_GPIO_PORT    GPIOB
#define CS43131_OE_PIN          GPIO_Pin_4

/* I2S GPIO definitions I2S1 */
#define I2S_WS_GPIO_PORT        GPIOB
#define I2S_WS_PIN              GPIO_Pin_12
#define I2S_WS_AF               GPIO_AF5
#define I2S_CK_GPIO_PORT        GPIOB
#define I2S_CK_PIN              GPIO_Pin_13
#define I2S_CK_AF               GPIO_AF5
#define I2S_SDO_GPIO_PORT       GPIOB
#define I2S_SDO_PIN             GPIO_Pin_15
#define I2S_SDO_AF              GPIO_AF5

typedef struct {
    uint8_t enable; // cs43131 enable
}cs43131_t;


void CS43131_init(cs43131_t *cs43131);
void CS43131_I2C_Config(void);

void I2S1_DMA_DoubleBufferInit(void);
void Audio_FillBuffer(uint16_t *buffer);
void Audio_PlayStart(const uint16_t *data, uint32_t length);
void Audio_GenerateTriangleWave(uint16_t *buffer, uint32_t length, int16_t amplitude, uint32_t period);
void Audio_PlayTriangleWave(int16_t amplitude, uint32_t period);

void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

#ifdef __cplusplus
}
#endif

#endif
