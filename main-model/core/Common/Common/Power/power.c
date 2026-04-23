#include "power.h"

void Power_Init(power_t *power)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOE, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART5, ENABLE);

    GPIO_PinAFConfig(POWER_UART_TX_PORT, GPIO_PinSource3, POWER_UART_TX_AF);
    GPIO_PinAFConfig(POWER_UART_RX_PORT, GPIO_PinSource2, POWER_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = POWER_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(POWER_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = POWER_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(POWER_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = POWER_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(POWER_UART, &USART_InitStructure);
    USART_Cmd(POWER_UART, ENABLE);

    // 使能接收中断并配置 NVIC
    USART_ITConfig(POWER_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(POWER_UART_IRQn, 2 << 4);
    NVIC_EnableIRQ(POWER_UART_IRQn);

    // 初始化协议接收状态机
    Protocol_InitRxCtx(&power->rx_ctx);

    Power_Get_Type(power);
}

/*********************************************************************
 * @fn      Power_UART_IRQ_Handler
 *
 * @brief   Power UART interrupt handler.
 *
 * @return  none
 *********************************************************************/
void Power_UART_IRQ_Handler(power_t *power)
{
    if (power == NULL)
        return;

    if (USART_GetITStatus(POWER_UART, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(POWER_UART);
        Protocol_ParseByte(&power->rx_ctx, byte);
    }
}

// 获取Power类型，入口参数是Power结构体指针
void Power_Get_Type(power_t *power)
{
}

// 发送数据，入口参数是Power结构体指针，发送数据，发送数据长度
void Power_Send_Data(power_t *power, uint8_t *data, uint16_t length)
{
    int i = 0;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(POWER_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(POWER_UART, *data++);
    }
}