#include "submodels.h"
#include "protocol_common.h"

void Submodels_Init (submodels_t *submodels) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd (RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB, ENABLE);
    RCC_HB1PeriphClockCmd (RCC_HB1Periph_USART6 | RCC_HB1Periph_USART7 | RCC_HB1Periph_USART8, ENABLE);

    GPIO_PinAFConfig (SUBMODELS1_UART_TX_PORT, GPIO_PinSource0, SUBMODELS1_UART_TX_AF);
    GPIO_PinAFConfig (SUBMODELS1_UART_RX_PORT, GPIO_PinSource1, SUBMODELS1_UART_RX_AF);

    GPIO_PinAFConfig (SUBMODELS2_UART_TX_PORT, GPIO_PinSource6, SUBMODELS2_UART_TX_AF);
    GPIO_PinAFConfig (SUBMODELS2_UART_RX_PORT, GPIO_PinSource5, SUBMODELS2_UART_RX_AF);

    GPIO_PinAFConfig (SUBMODELS3_UART_TX_PORT, GPIO_PinSource7, SUBMODELS3_UART_TX_AF);
    GPIO_PinAFConfig (SUBMODELS3_UART_RX_PORT, GPIO_PinSource8, SUBMODELS3_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS1_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init (SUBMODELS1_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS1_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init (SUBMODELS1_UART_RX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS2_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init (SUBMODELS2_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS2_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init (SUBMODELS2_UART_RX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS3_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init (SUBMODELS3_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS3_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init (SUBMODELS3_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = SUBMODELS_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init (USART6, &USART_InitStructure);
    USART_Init (USART7, &USART_InitStructure);
    USART_Init (USART8, &USART_InitStructure);
    USART_Cmd (USART6, ENABLE);
    USART_Cmd (USART7, ENABLE);
    USART_Cmd (USART8, ENABLE);

    // 使能接收中断并配置 NVIC
    USART_ITConfig(USART6, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART6_IRQn, 2 << 4);
    NVIC_EnableIRQ(USART6_IRQn);

    USART_ITConfig(USART7, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART7_IRQn, 2 << 4);
    NVIC_EnableIRQ(USART7_IRQn);

    USART_ITConfig(USART8, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART8_IRQn, 2 << 4);
    NVIC_EnableIRQ(USART8_IRQn);

    // 初始化协议接收状态机
    Protocol_InitRxCtx(&submodels[0].rx_ctx);
    Protocol_InitRxCtx(&submodels[1].rx_ctx);
    Protocol_InitRxCtx(&submodels[2].rx_ctx);

    submodels[0].submodels_id = 1;
    Submodels_Get_Type (submodels + 0);
    submodels[1].submodels_id = 2;
    Submodels_Get_Type (submodels + 1);
    submodels[2].submodels_id = 3;
    Submodels_Get_Type (submodels + 2);
}

/*********************************************************************
 * @fn      Submodels_UART_IRQ_Handler
 *
 * @brief   Submodels UART interrupt handler.
 *
 * @return  none
 *********************************************************************/
void Submodels_UART_IRQ_Handler(submodels_t *submodel, USART_TypeDef *USARTx)
{
    if (submodel == NULL)
        return;

    if (USART_GetITStatus(USARTx, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USARTx);
        Protocol_ParseByte(&submodel->rx_ctx, byte);
    }
}

// 获取Submodels类型，入口参数是Submodels结构体指针
void Submodels_Get_Type (submodels_t *submodel) {
    char *data = "submodel[get]:type";
    Submodels_Send_Data (submodel, (uint8_t *)data, strlen (data));
}

// 发送数据，入口参数是Submodels结构体指针，发送数据，发送数据长度
void Submodels_Send_Data (submodels_t *submodel, uint8_t *data, uint16_t length) {
    int i = 0;

    switch (submodel->submodels_id) {
    case 1:
        for (i = 0; i < length; i++) {
            while (USART_GetFlagStatus (USART6, USART_FLAG_TC) == RESET);
            USART_SendData (SUBMODELS1_UART, *data++);
        }
        break;
    case 2:
        for (i = 0; i < length; i++) {
            while (USART_GetFlagStatus (USART7, USART_FLAG_TC) == RESET);
            USART_SendData (SUBMODELS2_UART, *data++);
        }
        break;
    case 3:
        for (i = 0; i < length; i++) {
            while (USART_GetFlagStatus (USART8, USART_FLAG_TC) == RESET);
            USART_SendData (SUBMODELS3_UART, *data++);
        }
        break;

    default:
        break;
    }
}

/* ============================================================================
 * 通用命令回调与主循环帧处理
 * ============================================================================ */

static uint8_t submodels_on_get_type(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size,
                                     uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_SUBMODEL;
    resp_buf[1] = MODULE_SUBTYPE_SUBMODEL_FINGERPRINT; /* 默认子类型，实际应由硬件决定 */
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t submodels_cb = {
    .on_get_type = submodels_on_get_type,
};

void Submodels_Process(submodels_t *submodel)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;

    if (submodel == NULL || !submodel->rx_ctx.frame_ready)
        return;

    if (ProtocolCommon_Dispatch(&submodel->rx_ctx.frame, &submodels_cb,
                                resp, sizeof(resp), &resp_len))
    {
        if (resp_len > 0)
            Submodels_Send_Data(submodel, resp, resp_len);
    }
    else
    {
        /* 模块专用命令处理（待扩展） */
    }

    Protocol_ResetRxCtx(&submodel->rx_ctx);
}
