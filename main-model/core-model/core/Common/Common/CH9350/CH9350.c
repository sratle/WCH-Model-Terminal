#include "CH9350.h"

void CH9350_Init(ch9350_t *ch9350)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB2Periph_USART1, ENABLE);

    GPIO_PinAFConfig(CH9350_UART_TX_PORT, GPIO_PinSource9, CH9350_UART_TX_AF);
    GPIO_PinAFConfig(CH9350_UART_RX_PORT, GPIO_PinSource10, CH9350_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = CH9350_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH9350_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = CH9350_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH9350_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = CH9350_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(CH9350_UART, &USART_InitStructure);
    USART_Cmd(CH9350_UART, ENABLE);

    ch9350->enable = 1;
}

// 发送数据，入口参数是CH9350结构体指针，发送数据，发送数据长度
void CH9350_Send_Data(ch9350_t *ch9350, uint8_t *data, uint16_t length)
{
    int i = 0;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(CH9350_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(CH9350_UART, *data++);
    }
}