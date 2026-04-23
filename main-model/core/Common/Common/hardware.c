/********************************** (C) COPYRIGHT  *******************************
 * File Name          : hardware.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : This file provides all the hardware firmware functions.
 *******************************************************************************/
#include "hardware.h"

#include "CS43131/cs43131.h"
#include "Submodels/submodels.h"
#include "Power/power.h"
#include "Keyboard/keyboard.h"
#include "Display/display.h"
#include "CH9350/ch9350.h"
#include "CH378/ch378.h"
#include "CH585F/ch585f.h"
#include "Key/key.h"

/**
 * Global variables
 */
// V5F hardware
cs43131_t CS43131_g;
ch378_t ch378_g;
ch585f_t ch585f_g;
display_t display_g;
// V3F hardware
power_t power_g;
keyboard_t keyboard_g;
ch9350_t ch9350_g;
submodels_t submodels_g[3];

volatile hardware_t hardware_g; // Hardware global variable

/*********************************************************************
 * @fn      Hardware_init
 *
 * @brief   Hardware entry.
 *
 * @return  none
 *********************************************************************/
void Hardware_V3F_Init(void)
{
    Key_Init();
    Power_Init(&power_g);
    hardware_g.hardware_init_flag |= 0x10;

    Keyboard_Init(&keyboard_g);
    hardware_g.hardware_init_flag |= 0x20;

    CH9350_Init(&ch9350_g);
    hardware_g.hardware_init_flag |= 0x40;

    Submodels_Init(submodels_g);
    hardware_g.hardware_init_flag |= 0x80;

    while (hardware_g.hardware_init_flag != 0xFF);
}

void Hardware_V5F_Init(void)
{
    CS43131_init(&CS43131_g);
    hardware_g.hardware_init_flag |= 0x01;

    CH378_Init(&ch378_g);
    hardware_g.hardware_init_flag |= 0x02;

    CH585F_Init(&ch585f_g);
    hardware_g.hardware_init_flag |= 0x04;

    Display_Init(&display_g);
    hardware_g.hardware_init_flag |= 0x08;

    while (hardware_g.hardware_init_flag != 0xFF);
}