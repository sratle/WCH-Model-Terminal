#include "keyboard.h"
#include "../Protocol/protocol_common.h"
#include "../Display/display.h"
#include "../hardware.h"

keyboard_t *keyboard_ptr = NULL;

/* ============================================================================
 * 前向声明：Keyboard 专用命令 handler
 * ============================================================================ */

/* 请求类 handler：返回 1=成功（resp_buf 写入 DATA 域，*resp_len=DATA 长度），0=失败 */
static uint8_t Keyboard_HandleSetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len);
static uint8_t Keyboard_HandleGetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len);
static uint8_t Keyboard_HandleSetConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len);
static uint8_t Keyboard_HandleGetStatus(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len);

/* 事件类 handler：无返回值 */
static void Keyboard_HandleHidReport(const protocol_frame_t *req);
static void Keyboard_HandleMusicKeys(const protocol_frame_t *req);
static void Keyboard_HandleMusicKnobs(const protocol_frame_t *req);
static void Keyboard_HandleMusicSlider(const protocol_frame_t *req);
static void Keyboard_HandleGameInput(const protocol_frame_t *req);

/* ============================================================================
 * UART 初始化与数据收发
 * ============================================================================ */

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
    keyboard_ptr = keyboard;
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

// 获取Keyboard类型，入口参数是Keyboard结构体指针
void Keyboard_Get_Type(keyboard_t *keyboard)
{
    (void)keyboard;
}

// 发送数据，入口参数是Keyboard结构体指针，发送数据，发送数据长度
void Keyboard_Send_Data(keyboard_t *keyboard, uint8_t *data, uint16_t length)
{
    int i = 0;

    (void)keyboard;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(KEYBOARD_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(KEYBOARD_UART, *data++);
    }
}

/* ============================================================================
 * 通用命令回调
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

static const protocol_common_cb_t keyboard_common_cb = {
    .on_get_type = keyboard_on_get_type,
};

/* ============================================================================
 * 专用命令分发辅助函数
 * ============================================================================ */

/**
 * @brief  请求类 handler 的统一分发与 ACK/NACK 打包
 * @param  req       请求帧
 * @param  handler   handler 函数指针（NULL 则直接 NACK）
 * @param  resp_buf  响应缓冲区
 * @param  resp_size 缓冲区大小
 * @return 实际写入 resp_buf 的字节数（完整响应帧）
 */
static uint8_t keyboard_dispatch_req(const protocol_frame_t *req,
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

void Keyboard_Process(keyboard_t *keyboard)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;
    uint8_t handled = 0;
    protocol_frame_t *req;

    if (keyboard == NULL || !keyboard->rx_ctx.frame_ready)
        return;

    req = &keyboard->rx_ctx.frame;

    /* 0. 处理 Keyboard 返回的 ACK 响应（心跳 GET_TYPE 的回复） */
    if (req->cmd == CMD_ACK)
    {
        module_identity_t id;
        if (Protocol_ParseIdentity(req, &id))
        {
            Hardware_Hb_MarkOnline(MODULE_ID_KEYBOARD, id.type, id.subtype);
            keyboard->type_id = id.subtype;
        }
        handled = 1;
    }
    /* 1. 通用命令（0x00~0x0F） */
    else if (ProtocolCommon_Dispatch(req, &keyboard_common_cb,
                                resp, sizeof(resp), &resp_len))
    {
        handled = 1;
    }
    /* 2. Keyboard 专用基础操作码：0x21 ~ 0x29 */
    else if (req->cmd >= CMD_KBD_SET_BACKLIGHT && req->cmd <= CMD_KBD_GAME_INPUT)
    {
        switch (req->cmd)
        {
            /* ---- Core→Kbd 请求类 ---- */
            case CMD_KBD_SET_BACKLIGHT:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleSetBacklight,
                                                 resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_KBD_GET_BACKLIGHT:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleGetBacklight,
                                                 resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_KBD_SET_CONFIG:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleSetConfig,
                                                 resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_KBD_GET_STATUS:
                resp_len = keyboard_dispatch_req(req, Keyboard_HandleGetStatus,
                                                 resp, sizeof(resp));
                handled = 1;
                break;

            /* ---- Kbd→Core 事件类（不需要 ACK） ---- */
            case CMD_KBD_HID_REPORT:
                Keyboard_HandleHidReport(req);
                handled = 1;
                break;
            case CMD_KBD_MUSIC_KEYS:
                Keyboard_HandleMusicKeys(req);
                handled = 1;
                break;
            case CMD_KBD_MUSIC_KNOBS:
                Keyboard_HandleMusicKnobs(req);
                handled = 1;
                break;
            case CMD_KBD_MUSIC_SLIDER:
                Keyboard_HandleMusicSlider(req);
                handled = 1;
                break;
            case CMD_KBD_GAME_INPUT:
                Keyboard_HandleGameInput(req);
                handled = 1;
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
        Keyboard_Send_Data(keyboard, resp, resp_len);

    Protocol_ResetRxCtx(&keyboard->rx_ctx);
}

/* ============================================================================
 * Keyboard 专用命令 handler 实现（框架，待填充业务逻辑）
 * ============================================================================ */

/* ---- 请求类 handler ---- */
static uint8_t Keyboard_HandleSetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 解析 DATA[0]=模式, DATA[1]=亮度，设置键盘背光 */
    return 0; /* 暂不支持 */
}

static uint8_t Keyboard_HandleGetBacklight(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 返回当前背光模式和亮度 */
    return 0;
}

static uint8_t Keyboard_HandleSetConfig(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 解析配置项和值，保存到键盘 */
    return 0;
}

static uint8_t Keyboard_HandleGetStatus(const protocol_frame_t *req,
                                        uint8_t *resp_buf, uint16_t resp_size,
                                        uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 根据 req->data[0] 状态类型返回对应状态 */
    return 0;
}

/* ---- 事件类 handler ---- */
static void Keyboard_HandleHidReport(const protocol_frame_t *req)
{
    uint8_t report_type;
    uint8_t report_len;
    const uint8_t *report_data;

    if (req->len < 2)
        return;

    report_type = req->data[0];
    report_data = &req->data[1];
    report_len = req->len - 1;

    if (report_type == 0x00)
    {
        if (report_len < 8)
            return;

        printf("[KBD] HID KB | Mod: 0x%02X | Keys:", report_data[0]);
        for (uint8_t i = 2; i < 8; i++)
        {
            if (report_data[i])
                printf(" 0x%02X", report_data[i]);
        }
        printf("\r\n");

        Display_SendInputEvent(&display_g, INPUT_DEV_KEYBOARD,
                               report_data, 8);
    }
    else if (report_type == 0x01)
    {
        if (report_len < 4)
            return;

        printf("[KBD] HID MS | Btn: 0x%02X | X: %+4d | Y: %+4d | W: %+4d\r\n",
               report_data[0],
               (int8_t)report_data[1],
               (int8_t)report_data[2],
               (int8_t)report_data[3]);

        Display_SendInputEvent(&display_g, INPUT_DEV_MOUSE,
                               report_data, 4);
    }
}

static void Keyboard_HandleMusicKeys(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析琴键列表 [[键ID][力度]...]，转发到音频合成器或 Core */
}

static void Keyboard_HandleMusicKnobs(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析两个旋钮值（uint16 大端），转发到音频合成器或 Core */
}

static void Keyboard_HandleMusicSlider(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析推杆值（uint16 大端），转发到音频合成器或 Core */
}

static void Keyboard_HandleGameInput(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析摇杆 X/Y（int8）和按键状态 bitmask */
}
