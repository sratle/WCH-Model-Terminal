/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : This file contains all the functions prototypes for the 
*                      hardware.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#ifndef __HARDWARE_H
#define __HARDWARE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

	 
void Hardware_init(void);
void CS43131_init(void);
void Key_init(void);

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





