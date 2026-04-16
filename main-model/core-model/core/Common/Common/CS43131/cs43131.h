/********************************** (C) COPYRIGHT *******************************
 * File Name          : cs43131.h
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : CS43131 DAC driver header.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 *******************************************************************************/
#ifndef __CS43131_H
#define __CS43131_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

#define AUDIO_BUFFER_SIZE   256
#define SPI2_TX_DMA_REQUEST 65
#define CS43131_I2C_ADDR    0x30

void CS43131_init(void);
void CS43131_I2C_Config(void);

void I2S2_DMA_DoubleBufferInit(void);
void Audio_FillBuffer(uint16_t *buffer);
void Audio_PlayStart(const uint16_t *data, uint32_t length);
void Audio_GenerateTriangleWave(uint16_t *buffer, uint32_t length, int16_t amplitude, uint32_t period);
void Audio_PlayTriangleWave(int16_t amplitude, uint32_t period);

void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

#ifdef __cplusplus
}
#endif

#endif
