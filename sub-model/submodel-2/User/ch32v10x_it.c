/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v10x_it.c
 * Description        : Main Interrupt Service Routines for Submodel-2.
*********************************************************************************/

#include "ch32v10x_it.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/Max30102/max30102.h"
#include "../Common/Common/hardware.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI15_10_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

extern protocol_rx_ctx_t uart_core_rx_ctx;

void NMI_Handler(void)
{
    while (1)
    {
    }
}

void HardFault_Handler(void)
{
    NVIC_SystemReset();
    while (1)
    {
    }
}

void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)(USART_ReceiveData(USART1) & 0xFF);
        Protocol_ParseByte(&uart_core_rx_ctx, byte);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }

    /* Handle overrun error: read DR to clear ORE flag */
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET)
    {
        USART_ReceiveData(USART1);
    }
}

void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(MAX30102_INT_EXTI_LINE) != RESET)
    {
        EXTI_ClearITPendingBit(MAX30102_INT_EXTI_LINE);
        g_max30102_int_flag = 1;
    }
}
