/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Description        : Display-2 E-paper test.
*                      Matches Arduino OKRA0583BNF686F0 sample flow.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
*******************************************************************************/

#include "debug.h"
#include "../Common/Common/Eink/epaper.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    USART_Printf_Init(921600);
    Delay_Ms(500);

    printf("\r\n=== Display-2 E-paper Test ===\r\n");
    printf("SystemClk: %d\r\n", SystemCoreClock);

    Epaper_Init();

    /* Clear white then update (matches Arduino flow) */
    printf("\r\n--- ClearWhite + Update ---\r\n");
    Epaper_ClearWhite();
    Epaper_Update();

    /* Enter deep sleep */
    Epaper_Sleep();

    printf("\r\n=== Test done ===\r\n");

    while (1)
    {
        Delay_Ms(10000);
    }
}
