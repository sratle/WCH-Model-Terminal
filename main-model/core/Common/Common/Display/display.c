#include "display.h"
#include "../Protocol/protocol_common.h"

void Display_Init(display_t *display)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOF, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART4, ENABLE);

    GPIO_PinAFConfig(DISPLAY_UART_TX_PORT, GPIO_PinSource4, DISPLAY_UART_TX_AF);
    GPIO_PinAFConfig(DISPLAY_UART_RX_PORT, GPIO_PinSource3, DISPLAY_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = DISPLAY_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(DISPLAY_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = DISPLAY_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DISPLAY_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = DISPLAY_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(DISPLAY_UART, &USART_InitStructure);
    USART_Cmd(DISPLAY_UART, ENABLE);

    // 使能接收中断并配置 NVIC
    USART_ITConfig(DISPLAY_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(DISPLAY_UART_IRQn, 1 << 4);
    NVIC_EnableIRQ(DISPLAY_UART_IRQn);

    // 初始化协议接收状态机
    Protocol_InitRxCtx(&display->rx_ctx);

    Display_Get_Type(display);
}

/*********************************************************************
 * @fn      Display_UART_IRQ_Handler
 *
 * @brief   Display UART interrupt handler.
 *
 * @return  none
 *********************************************************************/
void Display_UART_IRQ_Handler(display_t *display)
{
    if (display == NULL)
        return;

    if (USART_GetITStatus(DISPLAY_UART, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(DISPLAY_UART);
        Protocol_ParseByte(&display->rx_ctx, byte);
    }
}

// 获取Display类型，入口参数是Display结构体指针
void Display_Get_Type(display_t *display)
{
}

// 发送数据，入口参数是Display结构体指针，发送数据，发送数据长度
void Display_Send_Data(display_t *display, uint8_t *data, uint16_t length)
{
    int i = 0;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(DISPLAY_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(DISPLAY_UART, *data++);
    }
}

/* ============================================================================
 * 通用命令回调与主循环帧处理
 * ============================================================================ */

static uint8_t display_on_get_type(const protocol_frame_t *req,
                                   uint8_t *resp_buf, uint16_t resp_size,
                                   uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_DISPLAY;
    resp_buf[1] = MODULE_SUBTYPE_DISPLAY_LCD;
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t display_cb = {
    .on_get_type = display_on_get_type,
};

void Display_Process(display_t *display)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;

    if (display == NULL || !display->rx_ctx.frame_ready)
        return;

    if (ProtocolCommon_Dispatch(&display->rx_ctx.frame, &display_cb,
                                resp, sizeof(resp), &resp_len))
    {
        if (resp_len > 0)
            Display_Send_Data(display, resp, resp_len);
    }
    else
    {
        /* 模块专用命令处理（待扩展） */
    }

    Protocol_ResetRxCtx(&display->rx_ctx);
}
