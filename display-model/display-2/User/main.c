/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Description        : Display-2 E-paper demo.
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

    Epaper_Init();

    Epaper_ClearWhite();
    Epaper_Update();

    Epaper_Sleep();

    while (1)
    {
        Delay_Ms(10000);
    }
}
