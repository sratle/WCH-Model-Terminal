/********************************** (C) COPYRIGHT *******************************
 * File Name          : main_v3f.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body for V3F.
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
/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
	SystemInit();
	SystemAndCoreClockUpdate();
	Delay_Init();

	USART_Printf_Init(115200);
	Debug_EnableRxIRQ();
	Delay_Ms(1000);

	printf("SystemClk:%d\r\n", SystemClock);
	printf("V3F SystemCoreClk:%d\r\n", SystemCoreClock);
	Delay_Ms(500);

#if (Run_Core == Run_Core_V3FandV5F)
	printf("V3F wake up\r\n");
	NVIC_WakeUp_V5F(Core_V5F_StartAddr);//wake up V5
	HSEM_ITConfig(HSEM_ID0, ENABLE);
	NVIC->SCTLR |= 1<<4;
	RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
	PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
	HSEM_ClearFlag(HSEM_ID0);
#endif

	Hardware_V3F_Init();

	while(1)
	{
		/* 统一 UART 帧处理（主循环轮询，避免中断阻塞） */
		Keyboard_Process(&keyboard_g);
		Power_Process(&power_g);
		Submodels_Process(&submodels_g[0]);
		Submodels_Process(&submodels_g[1]);
		Submodels_Process(&submodels_g[2]);

		/* CH9350 HID 数据解析（含超时检测、应答发送、printf输出） */
		CH9350_Process(&ch9350_g);

		Delay_Ms(1);
	}
}
