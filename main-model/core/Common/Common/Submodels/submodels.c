#include "submodels.h"
#include "../Protocol/protocol_common.h"
#include "../hardware.h"

/* ============================================================================
 * 前向声明：各子类型分发函数
 * 返回值：handled 标志（1=已处理），resp_len 由指针传出
 * ============================================================================ */
static uint8_t submodels_fp_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                     uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);
static uint8_t submodels_health_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                         uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);
static uint8_t submodels_nfc_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                      uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);
static uint8_t submodels_rgb_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                      uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);
static uint8_t submodels_touch_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                        uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);
static uint8_t submodels_ir_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                     uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);
static uint8_t submodels_subdisp_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                          uint8_t *resp, uint16_t resp_size, uint8_t *resp_len);

/* ============================================================================
 * UART 初始化与数据收发
 * ============================================================================ */

void Submodels_Init(submodels_t *submodels)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_GPIOB, ENABLE);
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART6 | RCC_HB1Periph_USART7 | RCC_HB1Periph_USART8, ENABLE);

    GPIO_PinAFConfig(SUBMODELS1_UART_TX_PORT, GPIO_PinSource0, SUBMODELS1_UART_TX_AF);
    GPIO_PinAFConfig(SUBMODELS1_UART_RX_PORT, GPIO_PinSource1, SUBMODELS1_UART_RX_AF);

    GPIO_PinAFConfig(SUBMODELS2_UART_TX_PORT, GPIO_PinSource6, SUBMODELS2_UART_TX_AF);
    GPIO_PinAFConfig(SUBMODELS2_UART_RX_PORT, GPIO_PinSource5, SUBMODELS2_UART_RX_AF);

    GPIO_PinAFConfig(SUBMODELS3_UART_TX_PORT, GPIO_PinSource15, SUBMODELS3_UART_TX_AF);
    GPIO_PinAFConfig(SUBMODELS3_UART_RX_PORT, GPIO_PinSource8, SUBMODELS3_UART_RX_AF);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS1_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(SUBMODELS1_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS1_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SUBMODELS1_UART_RX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS2_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(SUBMODELS2_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS2_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SUBMODELS2_UART_RX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS3_UART_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(SUBMODELS3_UART_TX_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SUBMODELS3_UART_RX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SUBMODELS3_UART_RX_PORT, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = SUBMODELS_UART_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART6, &USART_InitStructure);
    USART_Init(USART7, &USART_InitStructure);
    USART_Init(USART8, &USART_InitStructure);
    USART_Cmd(USART6, ENABLE);
    USART_Cmd(USART7, ENABLE);
    USART_Cmd(USART8, ENABLE);

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
    Submodels_Get_Type(submodels + 0);
    submodels[1].submodels_id = 2;
    Submodels_Get_Type(submodels + 1);
    submodels[2].submodels_id = 3;
    Submodels_Get_Type(submodels + 2);
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
void Submodels_Get_Type(submodels_t *submodel)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint8_t frame_len;
    uint8_t dst;
    uint32_t timeout;

    if (submodel == NULL)
        return;

    /* 根据 submodels_id 确定目标模块 ID */
    dst = MODULE_ID_SUBMODEL_1 + (submodel->submodels_id - 1);

    /* 发送 GET_TYPE 命令 */
    frame_len = Protocol_PackFrame(MODULE_ID_CORE, dst, CMD_GET_TYPE,
                                   NULL, 0, buf, sizeof(buf));
    if (frame_len > 0)
        Submodels_Send_Data(submodel, buf, frame_len);

    /* 轮询等待响应，超时约 50ms @ 100MHz V3F */
    timeout = 500000;
    while (timeout-- > 0)
    {
        if (submodel->rx_ctx.frame_ready)
        {
            /* 必须是 ACK 帧，且数据域至少包含 type + subtype */
            if (submodel->rx_ctx.frame.cmd == CMD_ACK &&
                submodel->rx_ctx.frame.len >= 2 &&
                submodel->rx_ctx.frame.data[0] == MODULE_TYPE_SUBMODEL)
            {
                submodel->type_id = submodel->rx_ctx.frame.data[1];
                Protocol_ResetRxCtx(&submodel->rx_ctx);
                return;
            }
            /* 收到非预期帧，重置继续等待 */
            Protocol_ResetRxCtx(&submodel->rx_ctx);
        }
    }

    /* 超时：type_id 保持 RESERVED，由主循环继续探测 */
}

// 发送数据，入口参数是Submodels结构体指针，发送数据，发送数据长度
void Submodels_Send_Data(submodels_t *submodel, uint8_t *data, uint16_t length)
{
    int i = 0;

    switch (submodel->submodels_id)
    {
        case 1:
            for (i = 0; i < length; i++)
            {
                while (USART_GetFlagStatus(USART6, USART_FLAG_TC) == RESET);
                USART_SendData(SUBMODELS1_UART, *data++);
            }
            break;
        case 2:
            for (i = 0; i < length; i++)
            {
                while (USART_GetFlagStatus(USART7, USART_FLAG_TC) == RESET);
                USART_SendData(SUBMODELS2_UART, *data++);
            }
            break;
        case 3:
            for (i = 0; i < length; i++)
            {
                while (USART_GetFlagStatus(USART8, USART_FLAG_TC) == RESET);
                USART_SendData(SUBMODELS3_UART, *data++);
            }
            break;

        default:
            break;
    }
}

/* ============================================================================
 * 通用命令回调
 * ============================================================================ */

static uint8_t submodels_on_get_type(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size,
                                     uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_SUBMODEL;
    resp_buf[1] = MODULE_SUBTYPE_SUBMODEL_RESERVED;
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t submodels_cb = {
    .on_get_type = submodels_on_get_type,
};

/* ============================================================================
 * 专用命令分发辅助函数
 * ============================================================================ */

__attribute__((unused)) static uint8_t submodels_dispatch_req(const protocol_frame_t *req,
                                      uint8_t (*handler)(const protocol_frame_t *,
                                                         uint8_t *, uint16_t, uint8_t *),
                                      uint8_t *resp_buf, uint16_t resp_size)
{
    uint8_t data_len = 0;

    if (handler == NULL)
        return ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                   resp_buf, resp_size);

    if (!handler(req, resp_buf, resp_size, &data_len))
        return ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                   resp_buf, resp_size);

    if (data_len > 0)
        return Protocol_PackFrame(req->dst, req->src, CMD_ACK,
                                  resp_buf, data_len, resp_buf, resp_size);

    return ProtocolCommon_Ack(req->dst, req->src, resp_buf, resp_size);
}

/* ============================================================================
 * 主循环帧处理
 * ============================================================================ */

void Submodels_Process(submodels_t *submodel)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;
    uint8_t handled = 0;
    protocol_frame_t *req;

    if (submodel == NULL || !submodel->rx_ctx.frame_ready)
        return;

    req = &submodel->rx_ctx.frame;

    /* 0. 处理 ACK 响应（心跳 GET_TYPE 的回复） */
    if (req->cmd == CMD_ACK && req->len >= 2 &&
        req->data[0] == MODULE_TYPE_SUBMODEL)
    {
        uint8_t module_id = MODULE_ID_SUBMODEL_1 + (submodel->submodels_id - 1);
        submodel->type_id = req->data[1];
        Hardware_Hb_MarkOnline(module_id, req->data[0], req->data[1]);

        /* type_id 尚未确定时，静默返回 */
        Protocol_ResetRxCtx(&submodel->rx_ctx);
        return;
    }

    /* type_id 尚未确定时，静默丢弃所有帧（不回复 NACK） */
    if (submodel->type_id == MODULE_SUBTYPE_SUBMODEL_RESERVED)
    {
        Protocol_ResetRxCtx(&submodel->rx_ctx);
        return;
    }

    /* 1. 通用命令（0x00~0x0F） */
    if (ProtocolCommon_Dispatch(req, &submodels_cb, resp, sizeof(resp), &resp_len))
    {
        handled = 1;
    }
    /* 2. Submodel 专用命令：0x40 ~ 0x46 */
    else if (req->cmd >= CMD_SUB_EVT_NOTIFY && req->cmd <= CMD_SUB_BULK_TRANSFER)
    {
        switch (submodel->type_id)
        {
            case MODULE_SUBTYPE_SUBMODEL_FINGERPRINT:
                handled = submodels_fp_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            case MODULE_SUBTYPE_SUBMODEL_HEALTH:
                handled = submodels_health_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            case MODULE_SUBTYPE_SUBMODEL_NFC:
                handled = submodels_nfc_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            case MODULE_SUBTYPE_SUBMODEL_TOUCH_RING:
                handled = submodels_touch_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            case MODULE_SUBTYPE_SUBMODEL_RGB:
                handled = submodels_rgb_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            case MODULE_SUBTYPE_SUBMODEL_INFRARED:
                handled = submodels_ir_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            case MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY:
                handled = submodels_subdisp_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
                break;
            default:
                resp_len = ProtocolCommon_Nack(req->dst, req->src,
                                               PROTO_ERR_UNSUPPORTED_CMD,
                                               resp, sizeof(resp));
                handled = 1;
                break;
        }
    }

    /* 发送响应（如果有） */
    if (handled && resp_len > 0)
        Submodels_Send_Data(submodel, resp, resp_len);

    Protocol_ResetRxCtx(&submodel->rx_ctx);
}

/* ============================================================================
 * 各子类型分发函数实现（框架，待填充业务逻辑）
 * ============================================================================
 * 所有子类型共用以下通用子命令约定：
 *   DATA[0] = 子命令（子类型内统一编号）
 *   DATA[1..N] = 子命令专用数据
 * ============================================================================ */

/* ---- 1. 指纹识别 (type = 0x01) ---- */
static uint8_t submodels_fp_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                     uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        /* 请求类：Core → Fingerprint */
        case CMD_SUB_SET_MODE:
            switch (subcmd)
            {
                case 0x01: /* 开始注册新指纹 */
                    /* TODO: 进入指纹注册模式，返回 ACK */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 取消注册 */
                    /* TODO: 退出注册模式 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_SET_CONFIG:
            switch (subcmd)
            {
                case 0x01: /* 删除指纹 */
                    /* TODO: req->data[1] = 指纹ID */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 清空所有指纹 */
                    /* TODO */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_GET_STATUS:
            switch (subcmd)
            {
                case 0x00: /* 查询指纹列表 */
                    /* TODO: resp_buf = [数量:1][[ID:1][名字:16字节]...] */
                    *resp_len = 0;
                    return 0;
            }
            break;

        /* 事件类：Fingerprint → Core */
        case CMD_SUB_EVT_NOTIFY:
            switch (subcmd)
            {
                case 0x01: /* 识别成功 */
                    /* TODO: req->data[1]=指纹ID, data[2..17]=名字(16字节) */
                    return 1;
                case 0x02: /* 识别失败 */
                    /* TODO: req->data[1]=错误码 */
                    return 1;
            }
            break;

        case CMD_SUB_ACTION_RESULT:
            switch (subcmd)
            {
                case 0x01: /* 注册成功 */
                    /* TODO: req->data[1]=指纹ID, data[2..17]=名字 */
                    return 1;
                case 0x02: /* 注册失败 */
                    /* TODO: req->data[1]=错误码 */
                    return 1;
            }
            break;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ---- 2. 健康监测 (type = 0x02) ---- */
static uint8_t submodels_health_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                         uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        case CMD_SUB_SET_CONFIG:
            switch (subcmd)
            {
                case 0x01: /* 设置监测间隔（秒） */
                    /* TODO: req->data[1..2] = 间隔(uint16大端) */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 开始监测 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x03: /* 停止监测 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_GET_STATUS:
            switch (subcmd)
            {
                case 0x00: /* 查询当前数据 */
                    /* TODO: resp = [心跳:1][血氧:1][体温:1] */
                    *resp_len = 0;
                    return 0;
            }
            break;

        case CMD_SUB_DATA_REPORT:
            switch (subcmd)
            {
                case 0x01: /* 健康数据上报 */
                    /* TODO: req->data[1]=心跳, data[2]=血氧, data[3]=体温 */
                    return 1;
            }
            break;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ---- 3. NFC (type = 0x03) ---- */
static uint8_t submodels_nfc_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                      uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        case CMD_SUB_SET_CONFIG:
            switch (subcmd)
            {
                case 0x01: /* 删除标签 */
                    /* TODO: req->data[1] = 标签ID */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 清空所有标签 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_GET_STATUS:
            switch (subcmd)
            {
                case 0x00: /* 查询标签列表 */
                    /* TODO: resp = [数量:1][[ID:1][UID:7字节]...] */
                    *resp_len = 0;
                    return 0;
            }
            break;

        case CMD_SUB_EVT_NOTIFY:
            switch (subcmd)
            {
                case 0x01: /* 检测到标签 */
                    /* TODO: req->data[1]=标签ID, data[2..8]=UID(7字节) */
                    return 1;
            }
            break;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ---- 4. 触摸圆环/旋钮 (type = 0x04) ---- */
static uint8_t submodels_touch_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                        uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        case CMD_SUB_DATA_REPORT:
            switch (subcmd)
            {
                case 0x01: /* 旋钮状态 */
                    /* TODO: req->data[1..2] = 旋钮值(uint16大端) */
                    return 1;
                case 0x02: /* 触摸事件 */
                    /* TODO: req->data[1..2]=X, data[3..4]=Y, data[5]=状态(按下/移动/释放) */
                    return 1;
            }
            break;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ---- 5. RGB 灯效 (type = 0x05) ----
 *
 * Core → RGB 方向的命令（SET_MODE/GET_STATUS）由专用 API 函数发送，
 * 不经过 dispatch。此函数仅处理 RGB → Core 方向的帧：
 *   - CMD_SUB_DATA_REPORT: RGB 主动上报当前模式/状态
 *   - CMD_SUB_EVT_NOTIFY:  RGB 事件通知（预留）
 */
static uint8_t submodels_rgb_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                      uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;
    (void)resp;
    (void)resp_size;

    switch (cmd)
    {
        case CMD_SUB_DATA_REPORT:
            switch (subcmd)
            {
                case 0x01: /* 当前模式上报
                            * data[1]=mode, data[2]=R, data[3]=G, data[4]=B,
                            * data[5]=brightness, data[6]=speed */
                    if (req->len >= 7)
                    {
                        printf("[RGB] mode=%d R=%d G=%d B=%d bright=%d speed=%d\r\n",
                               req->data[1], req->data[2], req->data[3],
                               req->data[4], req->data[5], req->data[6]);
                    }
                    *resp_len = 0;
                    return 1;   /* 事件帧无需回复 */

                default:
                    break;
            }
            break;

        case CMD_SUB_EVT_NOTIFY:
            /* RGB 事件通知（预留扩展） */
            *resp_len = 0;
            return 1;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ---- 6. 红外测距 (type = 0x06) ---- */
static uint8_t submodels_ir_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                     uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        case CMD_SUB_SET_MODE:
            switch (subcmd)
            {
                case 0x01: /* 开始测距 */
                    /* TODO: 启动红外测距 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 停止测距 */
                    /* TODO: 停止测距 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_ACTION_RESULT:
            switch (subcmd)
            {
                case 0x01: /* 测距结果 */
                    /* TODO: data[1..2]=距离(uint16大端), data[3]=单位, data[4]=精度 */
                    return 1;
                case 0x02: /* 测距失败 */
                    /* TODO: data[1]=错误码 */
                    return 1;
            }
            break;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ---- 7. 副屏 (type = 0x07) ---- */
static uint8_t submodels_subdisp_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                          uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        case CMD_SUB_SET_MODE:
            switch (subcmd)
            {
                case 0x01: /* 传输模块状态 */
                    /* TODO: data[1..N] = 模块状态数据（变长） */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 传输自定义内容 */
                    /* TODO: data[1]=内容类型, data[2..N]=内容数据 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x03: /* 清屏 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_BULK_TRANSFER:
            switch (subcmd)
            {
                case 0x01: /* 图片传输握手 */
                    /* TODO: 配置 Bulk 参数 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case 0x02: /* 传输完成确认 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        default:
            break;
    }

    *resp_len = ProtocolCommon_Nack(req->dst, req->src, PROTO_ERR_UNSUPPORTED_CMD,
                                    resp, resp_size);
    return 1;
}

/* ============================================================================
 * RGB Submodel Control API Implementation
 * ============================================================================
 * Core → RGB submodel-5 命令发送函数。
 * 所有函数均为 fire-and-forget：打包协议帧并通过 UART 发送，不等待响应。
 * 响应由 Submodels_Process 在主循环中异步处理。
 * ============================================================================ */

/* 计算 submodel 对应的协议模块 ID */
static uint8_t submodel_module_id(const submodels_t *submodel)
{
    return MODULE_ID_SUBMODEL_1 + (submodel->submodels_id - 1);
}

/*********************************************************************
 * @fn      Submodels_SendCommand
 *
 * @brief   向 submodel 发送通用命令（fire-and-forget）。
 *
 * @param   submodel  - 目标 submodel 实例
 * @param   cmd       - 操作码
 * @param   data      - 数据域指针（可为 NULL）
 * @param   data_len  - 数据域长度
 *
 * @return  1=发送成功, 0=参数错误或打包失败
 *********************************************************************/
uint8_t Submodels_SendCommand(submodels_t *submodel, uint8_t cmd,
                               const uint8_t *data, uint8_t data_len)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t frame_len;

    if (submodel == NULL)
        return 0;

    frame_len = Protocol_PackFrame(MODULE_ID_CORE,
                                   submodel_module_id(submodel),
                                   cmd, data, data_len,
                                   buf, sizeof(buf));
    if (frame_len == 0)
        return 0;

    Submodels_Send_Data(submodel, buf, frame_len);
    return 1;
}

/*********************************************************************
 * @fn      Submodels_RGB_SetMode
 *
 * @brief   设置 RGB 灯效模式。
 *          发送 CMD_SUB_SET_MODE SUB=0x01。
 *
 * @param   submodel   - 目标 RGB submodel
 * @param   mode       - 0=自定义, 1=常亮, 2=呼吸, 3=跑马灯
 * @param   r, g, b    - 基础颜色 RGB888
 * @param   brightness - 全局亮度 0-255
 * @param   speed      - 动画速度 0-255
 *
 * @return  1=发送成功, 0=失败
 *********************************************************************/
uint8_t Submodels_RGB_SetMode(submodels_t *submodel, uint8_t mode,
                               uint8_t r, uint8_t g, uint8_t b,
                               uint8_t brightness, uint8_t speed)
{
    /* DATA: [SUB=0x01][mode:1][R:1][G:1][B:1][brightness:1][speed:1] */
    uint8_t data[7] = { 0x01, mode, r, g, b, brightness, speed };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 7);
}

/*********************************************************************
 * @fn      Submodels_RGB_SendFrame
 *
 * @brief   传输一帧自定义 RGB888 动画数据。
 *          发送 CMD_SUB_SET_MODE SUB=0x02。
 *          每帧 DATA 共 149 字节: [SUB:1][frame_idx:1][RGB888:147]
 *
 * @param   submodel   - 目标 RGB submodel
 * @param   frame_idx  - 帧序号 (0 ~ RGB_MAX_CUSTOM_FRAMES-1)
 * @param   rgb_data   - RGB888 数据, 49*3=147 字节
 *
 * @return  1=发送成功, 0=失败
 *********************************************************************/
uint8_t Submodels_RGB_SendFrame(submodels_t *submodel, uint8_t frame_idx,
                                 const uint8_t *rgb_data)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t frame_len;
    uint8_t data[2 + RGB_FRAME_DATA_SIZE]; /* SUB(1) + idx(1) + RGB888(147) = 149 */

    if (submodel == NULL || rgb_data == NULL)
        return 0;

    if (frame_idx >= RGB_MAX_CUSTOM_FRAMES)
        return 0;

    data[0] = 0x02; /* sub-command: transfer frame */
    data[1] = frame_idx;
    memcpy(&data[2], rgb_data, RGB_FRAME_DATA_SIZE);

    frame_len = Protocol_PackFrame(MODULE_ID_CORE,
                                   submodel_module_id(submodel),
                                   CMD_SUB_SET_MODE,
                                   data, (uint8_t)(2 + RGB_FRAME_DATA_SIZE),
                                   buf, sizeof(buf));
    if (frame_len == 0)
        return 0;

    Submodels_Send_Data(submodel, buf, frame_len);
    return 1;
}

/*********************************************************************
 * @fn      Submodels_RGB_PlayAnimation
 *
 * @brief   启动自定义帧动画播放。
 *          发送 CMD_SUB_SET_MODE SUB=0x03。
 *
 * @param   submodel       - 目标 RGB submodel
 * @param   frame_count    - 总帧数 (1 ~ RGB_MAX_CUSTOM_FRAMES)
 * @param   frame_interval - 帧间隔 (ms, 50~1000)
 *
 * @return  1=发送成功, 0=失败
 *********************************************************************/
uint8_t Submodels_RGB_PlayAnimation(submodels_t *submodel, uint8_t frame_count,
                                     uint16_t frame_interval)
{
    /* DATA: [SUB=0x03][frame_count:1][frame_interval:2(big-endian)] */
    uint8_t data[4];

    if (submodel == NULL)
        return 0;

    data[0] = 0x03;
    data[1] = frame_count;
    data[2] = (uint8_t)(frame_interval >> 8);   /* high byte */
    data[3] = (uint8_t)(frame_interval & 0xFF); /* low byte */

    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 4);
}

/*********************************************************************
 * @fn      Submodels_RGB_QueryStatus
 *
 * @brief   查询 RGB 当前状态（异步）。
 *          发送 CMD_SUB_GET_STATUS SUB=0x00。
 *          响应由 Submodels_Process 异步处理。
 *
 * @param   submodel - 目标 RGB submodel
 *
 * @return  1=发送成功, 0=失败
 *********************************************************************/
uint8_t Submodels_RGB_QueryStatus(submodels_t *submodel)
{
    /* DATA: [SUB=0x00] */
    uint8_t data[1] = { 0x00 };
    return Submodels_SendCommand(submodel, CMD_SUB_GET_STATUS, data, 1);
}

/*********************************************************************
 * @fn      Submodels_FindRgbSlot
 *
 * @brief   在 submodels_g[0..2] 中查找已识别为 RGB 类型的 submodel。
 *
 * @return  RGB submodel 指针, 未找到返回 NULL
 *********************************************************************/
submodels_t *Submodels_FindRgbSlot(void)
{
    extern submodels_t submodels_g[3];
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        if (submodels_g[i].type_id == MODULE_SUBTYPE_SUBMODEL_RGB)
            return &submodels_g[i];
    }
    return NULL;
}
