#include "uart_fp.h"

volatile uint8_t  uart_fp_rx_buf[UART_FP_RX_BUF_SIZE];
volatile uint16_t uart_fp_rx_len;
volatile uint8_t  uart_fp_rx_ready;

static void UartFp_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    gpio.GPIO_Pin   = UART_FP_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(UART_FP_TX_PORT, &gpio);

    gpio.GPIO_Pin  = UART_FP_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(UART_FP_RX_PORT, &gpio);
}

void UartFp_Init(void)
{
    USART_InitTypeDef usart;

    UartFp_GPIO_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    usart.USART_BaudRate            = UART_FP_BAUDRATE;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(UART_FP_USART, &usart);

    USART_ITConfig(UART_FP_USART, USART_IT_RXNE, ENABLE);
    USART_ITConfig(UART_FP_USART, USART_IT_IDLE, ENABLE);

    NVIC_SetPriority(UART_FP_IRQn, 2);
    NVIC_EnableIRQ(UART_FP_IRQn);

    USART_Cmd(UART_FP_USART, ENABLE);

    uart_fp_rx_len = 0;
    uart_fp_rx_ready = 0;
}

void UartFp_SendData(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == NULL || len == 0)
        return;

    for (i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(UART_FP_USART, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(UART_FP_USART, data[i]);
    }
}
