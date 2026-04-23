/********************************** (C) COPYRIGHT  *******************************
 * File Name          : debug.c
 * Author             : WCH
 * Version            : V1.0.1
 * Date               : 2025/07/16
 * Description        : This file contains all the functions prototypes for UART
 *                      Printf , Delay functions.
 ******************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/
#include "debug.h"
#include "protocol.h"

static uint16_t p_us = 0;
static uint32_t p_ms = 0;

/*********************************************************************
 * @fn      Delay_Init
 *
 * @brief   Initializes Delay Funcation.
 *
 * @return  none
 */
void Delay_Init(void)
{
    p_us = HCLKClock / 1000000;
    p_ms = (uint16_t)p_us * 1000;
}

/*********************************************************************
 * @fn      Delay_Us
 *
 * @brief   Microsecond Delay Time.
 *
 * @param   n - Microsecond number.
 *
 * @return  none
 */
void Delay_Us(uint32_t n)
{
    uint32_t i;
#ifdef Core_V3F
    SysTick0->ISR &= ~(1 << 0);
    i = (uint32_t)n * p_us;

    SysTick0->CNT = 0;
    SysTick0->CMP = i;
    SysTick0->CTLR = (1 << 2);
    SysTick0->CTLR |= (1 << 0);

    while ((SysTick0->ISR & (1 << 0)) != (1 << 0))
        ;
    SysTick0->CTLR &= ~(1 << 0);

#elif defined(Core_V5F)
    SysTick0->ISR &= ~(1 << 1);
    i = (uint32_t)n * p_us;

    SysTick1->CNT = 0;
    SysTick1->CMP = i;
    SysTick1->CTLR = (1 << 2);
    SysTick1->CTLR |= (1 << 0);

    while ((SysTick0->ISR & (1 << 1)) != (1 << 1))
        ;
    SysTick1->CTLR &= ~(1 << 0);
#endif
}

/*********************************************************************
 * @fn      Delay_Ms
 *
 * @brief   Millisecond Delay Time.
 *
 * @param   n - Millisecond number.
 *
 * @return  none
 */
void Delay_Ms(uint32_t n)
{
    uint32_t i;
#ifdef Core_V3F
    SysTick0->ISR &= ~(1 << 0);
    i = (uint32_t)n * p_ms;

    SysTick0->CNT = 0;
    SysTick0->CMP = i;
    SysTick0->CTLR = (1 << 2);
    SysTick0->CTLR |= (1 << 0);

    while ((SysTick0->ISR & (1 << 0)) != (1 << 0))
        ;
    SysTick0->CTLR &= ~(1 << 0);
#elif defined(Core_V5F)
    SysTick0->ISR &= ~(1 << 1);
    i = (uint32_t)n * p_ms;

    SysTick1->CNT = 0;
    SysTick1->CMP = i;
    SysTick1->CTLR = (1 << 2);
    SysTick1->CTLR |= (1 << 0);

    while ((SysTick0->ISR & (1 << 1)) != (1 << 1))
        ;
    SysTick1->CTLR &= ~(1 << 0);
#endif
}

/*********************************************************************
 * @fn      USART_Printf_Init
 *
 * @brief   Initializes the USARTx peripheral.
 *
 * @param   baudrate - USART communication baud rate.
 *
 * @return  none
 */
void USART_Printf_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART2, ENABLE);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF7);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF7);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &USART_InitStructure);
    USART_Cmd(USART2, ENABLE);
}

/*********************************************************************
 * @fn      _write
 *
 * @brief   Support Printf Function
 *
 * @param   *buf - UART send Data.
 *          size - Data length
 *
 * @return  size: Data length
 */
__attribute__((used)) int _write(int fd, char *buf, int size)
{
    int i = 0;

    for (i = 0; i < size; i++)
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(USART2, *buf++);
    }

    return size;
}

/*********************************************************************
 * @fn      _sbrk
 *
 * @brief   Change the spatial position of data segment.
 *
 * @return  size: Data length
 */
__attribute__((used)) void *_sbrk(ptrdiff_t incr)
{
    extern char _end[];
    extern char _heap_end[];
    static char *curbrk = _end;

    if ((curbrk + incr < _end) || (curbrk + incr > _heap_end))
        return NULL - 1;

    curbrk += incr;
    return curbrk - incr;
}

/*********************************************************************
 * @fn      Debug_EnableRxIRQ
 *
 * @brief   Enable USART2 RX interrupt for debug channel.
 *
 * @return  none
 *********************************************************************/
static protocol_rx_ctx_t debug_rx_ctx;

void Debug_EnableRxIRQ(void)
{
    Protocol_InitRxCtx(&debug_rx_ctx);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART2_IRQn, 3 << 4);
    NVIC_EnableIRQ(USART2_IRQn);
}

/*********************************************************************
 * @fn      Debug_UART_IRQ_Handler
 *
 * @brief   Debug UART interrupt handler.
 *
 * @return  none
 *********************************************************************/
void Debug_UART_IRQ_Handler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART2);
        Protocol_ParseByte(&debug_rx_ctx, byte);
    }
}
