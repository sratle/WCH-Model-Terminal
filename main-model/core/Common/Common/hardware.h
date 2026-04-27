/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.h
* Author             : 
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : This file contains all the functions prototypes for the 
*                      hardware.
*******************************************************************************/
#ifndef __HARDWARE_H
#define __HARDWARE_H

#ifdef __cplusplus
 extern "C" {
#endif
#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

/* 模块全局变量声明（供中断服务函数等访问） */
#include "Power/power.h"
#include "Keyboard/keyboard.h"
#include "CH9350/ch9350.h"
#include "Submodels/submodels.h"
extern power_t power_g;
extern keyboard_t keyboard_g;
extern ch9350_t ch9350_g;
extern submodels_t submodels_g[3];

#include "CS43131/cs43131.h"
#include "CH378/CH378.h"
#include "CH585F/ch585f.h"
#include "Display/display.h"
extern cs43131_t CS43131_g;
extern ch378_t ch378_g;
extern ch585f_t ch585f_g;
extern display_t display_g;

typedef struct
{
    uint8_t hardware_state; // Hardware state
    uint8_t hardware_init_flag; // Hardware init flag
} hardware_t;

void Hardware_V5F_Init(void);
void Hardware_V3F_Init(void);

#ifdef __cplusplus
}
#endif

#endif 





