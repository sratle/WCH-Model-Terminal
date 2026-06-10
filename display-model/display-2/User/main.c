/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Description        : Display-2 E-paper demo (JD79686AB 648×480 B/W).
*                      Based on manufacturer STM32 reference code.
*
* UART2 (PA2-TX, PA3-RX) – DEBUG printf
* SPI1  software SPI (PA4-CS, PA5-SCK, PA7-MOSI)
* GPIO  (PB3-RES, PB4-DC, PB5-BUSY)
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
*******************************************************************************/

#include "debug.h"
#include "../Common/Common/Eink/epaper.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   Matches manufacturer main.c demo flow:
 *          Init → Display_Clear → Update → DeepSleep (loop)
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    USART_Printf_Init(921600);
    Delay_Ms(500);

    printf("\r\n=== Display-2 E-paper (JD79686AB 648x480) ===\r\n");
    printf("SystemClk: %d\r\n", SystemCoreClock);
    printf("ChipID: %08x\r\n", DBGMCU_GetCHIPID());

    /* GPIO init is handled by Epaper_Init → Epaper_Hw_Init */

    Epaper_Init();

    while (1)
    {
        /* Step 1: Clear to white (DTM1=0xFF, DTM2=0x00) */
        Epaper_ClearWhite();

        /* Step 2: Update display (PON → DRF) */
        Epaper_Update();

        /* Step 3: Deep sleep (POF → DSLP) */
        Epaper_Sleep();

        printf("EPD: Cycle complete, sleeping 5s...\r\n");
        Delay_Ms(5000);

        /* Wake up for next cycle */
        Epaper_WakeUp();
    }
}
