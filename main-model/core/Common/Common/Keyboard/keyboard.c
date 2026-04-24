#include "keyboard.h"
#include "../Protocol/protocol_common.h"

void Keyboard_Init(keyboard_t *keyboard)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOE, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART3, ENABLE);

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

    // 使能接收中断并配置 NVIC
    USART_ITConfig(KEYBOARD_UART, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(KEYBOARD_UART_IRQn, 1 << 4);
    NVIC_EnableIRQ(KEYBOARD_UART_IRQn);

    // 初始化协议接收状态机
    Protocol_InitRxCtx(&keyboard->rx_ctx);

    Keyboard_Get_Type(keyboard);
}

/*********************************************************************
 * @fn      Keyboard_UART_IRQ_Handler
 *
 * @brief   Keyboard UART interrupt handler.
 *
 * @return  none
 *********************************************************************/
void Keyboard_UART_IRQ_Handler(keyboard_t *keyboard)
{
    if (keyboard == NULL)
        return;

    if (USART_GetITStatus(KEYBOARD_UART, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(KEYBOARD_UART);
        Protocol_ParseByte(&keyboard->rx_ctx, byte);
    }
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
/* ============================================================================
 * 通用命令回调与主循环帧处理
 * ============================================================================ */

static uint8_t keyboard_on_get_type(const protocol_frame_t *req,
                                    uint8_t *resp_buf, uint16_t resp_size,
                                    uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_KEYBOARD;
    resp_buf[1] = MODULE_SUBTYPE_KEYBOARD_MAIN;
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t keyboard_cb = {
    .on_get_type = keyboard_on_get_type,
};

void Keyboard_Process(keyboard_t *keyboard)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;

    if (keyboard == NULL || !keyboard->rx_ctx.frame_ready)
        return;

    if (ProtocolCommon_Dispatch(&keyboard->rx_ctx.frame, &keyboard_cb,
                                resp, sizeof(resp), &resp_len))
    {
        if (resp_len > 0)
            Keyboard_Send_Data(keyboard, resp, resp_len);
    }
    else
    {
        /* 模块专用命令处理（待扩展） */
    }

    Protocol_ResetRxCtx(&keyboard->rx_ctx);
}
