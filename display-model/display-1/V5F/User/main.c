/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 polling transceiver mode, master/slave transceiver routine:
 Master:USART2_Tx(PD5)\USART2_Rx(PD6).
 Slave:USART3_Tx(PB10)\USART3_Rx(PB11).
 This example demonstrates sending from USART2 and receiving from USART3.

   Hardware connection:
			   PD5 -- PB11
               PD6 -- PB10

*/

#include "debug.h"
#include "hardware.h"
#include "../Common/SSD1963/ssd1963.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
	SystemAndCoreClockUpdate();
	Delay_Init();
	USART_Printf_Init(115200);
	printf("V5F SystemCoreClk:%d (HCLK)\r\n", SystemCoreClock);
	printf("V5F HCLKClock:%d\r\n", HCLKClock);

	Delay_Ms(500);

	HSEM_FastTake(HSEM_ID0);
	HSEM_ReleaseOneSem(HSEM_ID0, 0);

	Hardware_V5F_Init();

	/* Initialize TE (Tearing Effect) sync: PB8 connected to SSD1963 TE pin */
	SSD1963_TE_Init();

	/* Frame rate limiter: target 30 FPS (33.333ms per frame) */
	uint32_t cycles_per_ms = SystemCoreClock / 1000;
	uint32_t frame_cycles = cycles_per_ms * 33;  /* ~33.333ms = 30fps */
	uint32_t last_frame = __get_UCYCLE();

	while(1)
	{
		/* Scan touch first (I2C communication, no GRAM writes).
		 * Do this BEFORE VSYNC wait so drawing starts right at
		 * the blanking period, not after I2C delay. */
		Touch_Scan();

		/* Wait for VSYNC right before drawing to prevent tearing.
		 * TE goes high at start of vertical blanking period.
		 * Drawing starts immediately in blanking, so LCD scan never
		 * catches a partially-updated frame. */
		SSD1963_WaitVSync();

		UI_Tick();

		/* Frame rate limiting to ~30 FPS */
		uint32_t now = __get_UCYCLE();
		uint32_t elapsed = now - last_frame;

		if (elapsed < frame_cycles) {
			/* Frame finished early, delay to hit 30fps */
			uint32_t remaining_ms = (frame_cycles - elapsed) / cycles_per_ms;
			if (remaining_ms > 0) {
				Delay_Ms(remaining_ms);
			}
		} else {
			/* Frame took longer than 33.333ms, just delay 1ms minimum */
			Delay_Ms(1);
		}

		last_frame = __get_UCYCLE();
	}
}
