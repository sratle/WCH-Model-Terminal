/********************************** (C) COPYRIGHT *******************************
 * File Name          : ch32v10x_it.c
 * Description        : Main Interrupt Service Routines for Submodel-1.
*********************************************************************************/

#include "ch32v10x_it.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Uart/uart_fp.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/Fingerprint/fingerprint.h"
#include "../Common/Common/hardware.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void EXTI4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

extern protocol_rx_ctx_t uart_core_rx_ctx;
extern volatile uint8_t  uart_fp_rx_buf[UART_FP_RX_BUF_SIZE];
extern volatile uint16_t uart_fp_rx_len;
extern volatile uint8_t  uart_fp_rx_ready;

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

void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART2);

        if (uart_fp_rx_len < UART_FP_RX_BUF_SIZE)
        {
            uart_fp_rx_buf[uart_fp_rx_len++] = byte;
        }
    }

    if (USART_GetITStatus(USART2, USART_IT_IDLE) != RESET)
    {
        (void)USART_ReceiveData(USART2);
        uart_fp_rx_ready = 1;
    }
}

void EXTI4_IRQHandler(void)
{
    if (EXTI_GetITStatus(TOUCHOUT_EXTI_LINE) != RESET)
    {
        EXTI_ClearITPendingBit(TOUCHOUT_EXTI_LINE);
        g_touchout_flag = 1;
    }
}
