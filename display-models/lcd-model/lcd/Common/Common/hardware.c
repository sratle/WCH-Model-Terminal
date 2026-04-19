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
    /* Initialize LCD display subsystem:
     *   - FMC Bank1 NORSRAM (8080 mode for SSD1963)
     *   - LCD panel control signals (PA4 MODE, PA5 L/R, PA6 U/D, PA7 RESET)
     *   - SSD1963 display controller (PLL, timing, GRAM)
     *   - MiniUI lightweight UI framework
     */
    UI_Init();
}
