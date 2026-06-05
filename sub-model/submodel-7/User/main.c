/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/01/05
 * Description        : Submodel-7 副屏显示模块主程序
 *                      初始化 ST7305 全反屏 + UART1 协议通信
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "debug.h"
#include "APP/app.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   Submodel-7 主循环
 *
 * @return  none
 */
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* Initialize all subsystems (LCD, UI, UART, protocol) */
    App_Init();

    /* Main loop */
    while (1)
    {
        App_Process();
    }
}
