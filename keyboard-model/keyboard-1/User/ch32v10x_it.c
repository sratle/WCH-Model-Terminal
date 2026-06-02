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
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

extern volatile uint8_t g_scan_flag;
extern protocol_rx_ctx_t uart_core_rx_ctx;

void NMI_Handler(void)
{
    while(1)
    {
    }
}

void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while(1)
    {
    }
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        g_scan_flag = 1;
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
        Protocol_ParseByte(&uart_core_rx_ctx, byte);
    }
}
