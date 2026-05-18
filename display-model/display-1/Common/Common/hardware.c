/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : This file provides all the hardware firmware functions.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "hardware.h"

/*********************************************************************
 * @fn      Hardware
 *
 * @brief   Hardware entry.
 *
 * @return  none
 */
void Hardware_V3F_Init(void)
{
    while (1)
        ;
}

void Hardware_V5F_Init(void)
{
    printf("[Hardware_V5F_Init] start\r\n");
    Touch_Init();
    UI_Init();
    printf("[Hardware_V5F_Init] done\r\n");
}
