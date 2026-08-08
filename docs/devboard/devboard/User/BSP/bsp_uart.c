/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_uart.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : USART1 full-duplex driver with interrupt-driven RX
*                      ring buffer. Overflow policy: drop the newest byte
*                      (oldest data is usually the start of a frame).
********************************************************************************/
#include "bsp_uart.h"
#include "ch32v30x.h"

/*=============================================================================
 *  Ring Buffer State
 *=============================================================================*/

#if (UART_RX_BUF_SIZE & (UART_RX_BUF_SIZE - 1)) != 0
#error "UART_RX_BUF_SIZE must be a power of two"
#endif

#define UART_RX_MASK    (UART_RX_BUF_SIZE - 1)

static volatile uint8_t  s_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head = 0;     /* Written by the ISR        */
static volatile uint16_t s_rx_tail = 0;     /* Read by the application   */

/*=============================================================================
 *  Interrupt Handler
 *=============================================================================*/

/*********************************************************************
 * @fn      USART1_IRQHandler
 *
 * @brief   Push every received byte into the ring buffer.
 *
 * @return  none
 */
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) != RESET) {
        uint8_t  b    = (uint8_t)USART_ReceiveData(USART1);
        uint16_t next = (uint16_t)((s_rx_head + 1) & UART_RX_MASK);
        if (next != s_rx_tail) {            /* Not full: store          */
            s_rx_buf[s_rx_head] = b;
            s_rx_head = next;
        }
        /* Full: byte is dropped silently */
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void UART_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef uart;
    NVIC_InitTypeDef  nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA,
                           ENABLE);

    /* PA9 = TX (alternate function push-pull) */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA10 = RX (floating input) */
    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* 8N1, TX + RX */
    uart.USART_BaudRate            = baudrate;
    uart.USART_WordLength          = USART_WordLength_8b;
    uart.USART_StopBits            = USART_StopBits_1;
    uart.USART_Parity              = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &uart);

    USART_ClearFlag(USART1, USART_FLAG_RXNE);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);

    nvic.NVIC_IRQChannel                   = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

void UART_SendByte(uint8_t b)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) { }
    USART_SendData(USART1, b);
}

void UART_SendBytes(const uint8_t *data, uint16_t len)
{
    uint16_t i;
    if (!data) return;
    for (i = 0; i < len; i++) {
        UART_SendByte(data[i]);
    }
}

void UART_SendString(const char *str)
{
    if (!str) return;
    while (*str) {
        UART_SendByte((uint8_t)*str++);
    }
}

uint16_t UART_Available(void)
{
    return (uint16_t)((s_rx_head - s_rx_tail) & UART_RX_MASK);
}

int16_t UART_ReadByte(void)
{
    uint8_t b;
    if (s_rx_head == s_rx_tail) return -1;      /* Empty */
    b = s_rx_buf[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1) & UART_RX_MASK);
    return (int16_t)b;
}

uint16_t UART_ReadBytes(uint8_t *buf, uint16_t max_len)
{
    uint16_t n = 0;
    int16_t  ch;
    if (!buf) return 0;
    while (n < max_len) {
        ch = UART_ReadByte();
        if (ch < 0) break;
        buf[n++] = (uint8_t)ch;
    }
    return n;
}

void UART_RxFlush(void)
{
    s_rx_tail = s_rx_head;
}
