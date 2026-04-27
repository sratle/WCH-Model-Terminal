/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body.
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
	SystemAndCoreClockUpdate();
	Delay_Init();
	USART_Printf_Init(115200);
	printf("V5F SystemCoreClk:%d\r\n", SystemCoreClock);

	Delay_Ms(500);

#if (Run_Core == Run_Core_V3FandV5F)
	HSEM_FastTake(HSEM_ID0);
	HSEM_ReleaseOneSem(HSEM_ID0, 0);
#endif

	printf("V5F Wake Up\r\n");

	Hardware_V5F_Init();

	CH378_Device_Select(&ch378_g, CH378_Device_TF);  // 先挂载 TF 卡
	CH378_List_Root_Files(&ch378_g);                  // 列出根目录

	while(1)
	{
		/* Display 统一 UART 帧处理（主循环轮询） */
		Display_Process(&display_g);

		printf("V5F is running\r\n");
		printf("SPI2_STATR=0x%04X, SPI2_I2SCFGR=0x%04X\r\n", SPI2->STATR, SPI2->I2SCFGR);
		printf("DMA_CNTR=%d, DMA_CFG=0x%08X\r\n", DMA1_Channel1->CNTR, DMA1_Channel1->CFGR);
		Delay_Ms(1);
	}
}
