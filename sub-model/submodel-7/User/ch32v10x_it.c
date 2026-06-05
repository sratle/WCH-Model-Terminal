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
#include "APP/protocol.h"
#include "APP/uart_core.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));


/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
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
 */
void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while(1)
    {
    }
}

/*********************************************************************
 * @fn      USART1_IRQHandler
 *
 * @brief   USART1 receive interrupt: feed byte to protocol parser.
 *
 * @return  none
 */
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(UART_CORE_USART, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)(USART_ReceiveData(UART_CORE_USART) & 0xFF);
        Protocol_ParseByte(&uart_core_rx_ctx, byte);
        USART_ClearITPendingBit(UART_CORE_USART, USART_IT_RXNE);
    }

    /* Handle overrun error: read DR to clear ORE flag */
    if (USART_GetFlagStatus(UART_CORE_USART, USART_FLAG_ORE) != RESET)
    {
        USART_ReceiveData(UART_CORE_USART);
    }
}
