/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v10x_it.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/01/05
 * Description        : Main Interrupt Service Routines.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for 
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "ch32v10x_it.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/* Defined in main.c */
extern volatile uint8_t g_scan_flag;

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 *********************************************************************/
void NMI_Handler(void)
{
    while(1)
    {
    }
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 *********************************************************************/
void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while(1)
    {
    }
}

/*********************************************************************
 * @fn      TIM2_IRQHandler
 *
 * @brief   TIM2 update interrupt handler.
 *          Sets g_scan_flag to trigger key matrix read in main loop.
 *          Period: 2ms (72MHz / 72 / 2000).
 *
 * @return  none
 *********************************************************************/
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        g_scan_flag = 1;
    }
}
