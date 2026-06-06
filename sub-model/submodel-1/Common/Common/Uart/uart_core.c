#include "uart_core.h"

protocol_rx_ctx_t uart_core_rx_ctx;

static void UartCore_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    gpio.GPIO_Pin   = UART_CORE_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(UART_CORE_TX_PORT, &gpio);

    gpio.GPIO_Pin  = UART_CORE_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(UART_CORE_RX_PORT, &gpio);
}

void UartCore_Init(void)
{
    USART_InitTypeDef usart;

    UartCore_GPIO_Init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    usart.USART_BaudRate            = UART_CORE_BAUDRATE;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(UART_CORE_USART, &usart);

    USART_ITConfig(UART_CORE_USART, USART_IT_RXNE, ENABLE);

    NVIC_SetPriority(UART_CORE_IRQn, 1);
    NVIC_EnableIRQ(UART_CORE_IRQn);

    USART_Cmd(UART_CORE_USART, ENABLE);

    Protocol_InitRxCtx(&uart_core_rx_ctx);
}

void UartCore_SendData(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == NULL || len == 0)
        return;

    for (i = 0; i < len; i++)
    {
        while (USART_GetFlagStatus(UART_CORE_USART, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(UART_CORE_USART, data[i]);
    }
}

uint16_t UartCore_PackAndSend(uint8_t dst, uint8_t cmd,
                              const uint8_t *data, uint8_t data_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;

    len = Protocol_PackFrame(MODULE_ID_SUBMODEL_1, dst, cmd,
                             data, data_len, buf, sizeof(buf));
    if (len == 0)
        return 0;

    UartCore_SendData(buf, len);
    return len;
}
