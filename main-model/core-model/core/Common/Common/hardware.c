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

#include "CS43131/cs43131.h"
#include "SubModels/submodels.h"
#include "Power/power.h"
#include "Keyboard/keyboard.h"
#include "Display/display.h"
#include "CH9329/ch9329.h"
#include "CH378/ch378.h"
#include "CH585F/ch585f.h"
#include "Key/key.h"

/*********************************************************************
 * @fn      Hardware_init
 *
 * @brief   Hardware entry.
 *
 * @return  none
 *********************************************************************/
void Hardware_V3F_init(void)
{
    Key_init();
}

void Hardware_V5F_init(void)
{
    CS43131_init();
}