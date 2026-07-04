/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v10x_it.c
 * Description        : Main Interrupt Service Routines for Submodel-3 (NFC).
*********************************************************************************/

#include "ch32v10x_it.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/Nfc/nfc.h"
#include "../Common/Common/hardware.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

extern protocol_rx_ctx_t uart_core_rx_ctx;

/* Diagnostic counters */
volatile uint32_t g_usart2_isr_count = 0;
volatile uint32_t g_usart2_byte_count = 0;

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

/* USART1 IRQ: Core protocol communication (PA9/PA10, 230400bps)
 * Feed each received byte into the protocol state machine */
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

/* USART2 IRQ: NFC module data (PA2/PA3, 9600bps)
 * Feed each received byte directly into the NFC frame parser */
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)(USART_ReceiveData(USART2) & 0xFF);
        g_usart2_isr_count++;
        Nfc_ParseByte(byte);
    }

    /* Handle overrun error: read DR to clear ORE flag */
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET)
    {
        USART_ReceiveData(USART2);
        g_usart2_byte_count++;
    }
}
