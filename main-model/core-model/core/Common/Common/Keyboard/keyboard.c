#include "keyboard.h"

void Keyboard_Init(keyboard_t *keyboard)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOE, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART5, ENABLE);

    GPIO_PinAFConfig(KEYBOARD_UART_TX_PORT, GPIO_PinSource10, KEYBOARD_UART_TX_AF);
    GPIO_PinAFConfig(KEYBOARD_UART_RX_PORT, GPIO_PinSource11, KEYBOARD_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = KEYBOARD_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(KEYBOARD_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = KEYBOARD_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(KEYBOARD_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = KEYBOARD_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(KEYBOARD_UART, &USART_InitStructure);
    USART_Cmd(KEYBOARD_UART, ENABLE);

    Keyboard_Get_Type(keyboard);
}

// 获取Power类型，入口参数是Power结构体指针
void Keyboard_Get_Type(keyboard_t *keyboard)
{
}

// 发送数据，入口参数是Power结构体指针，发送数据，发送数据长度
void Keyboard_Send_Data(keyboard_t *keyboard, uint8_t *data, uint16_t length)
{
    int i = 0;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(KEYBOARD_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(KEYBOARD_UART, *data++);
    }
}