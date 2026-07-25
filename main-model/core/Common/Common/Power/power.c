#include "power.h"
#include "../Protocol/protocol_common.h"
#include "../hardware.h"
#include "../Display/display.h"

power_t *power_ptr = NULL;

/* ============================================================================
 * 前向声明：Power 专用命令 handler
 * ============================================================================ */

/* 事件类 handler：Power 仅上报电量（简化协议） */
static void Power_HandleStatusReport(const protocol_frame_t *req);
static void power_update_status(uint8_t new_batt, uint8_t new_charge);

/* ============================================================================
 * UART 初始化与数据收发
 * ============================================================================ */

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
    power_ptr = power;
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
    (void)power;
}

// 发送数据，入口参数是Power结构体指针，发送数据，发送数据长度
void Power_Send_Data(power_t *power, uint8_t *data, uint16_t length)
{
    int i = 0;

    (void)power;

    if (length == 0) return;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(POWER_UART, USART_FLAG_TXE) == RESET)
            ;
        USART_SendData(POWER_UART, *data++);
    }
    while (USART_GetFlagStatus(POWER_UART, USART_FLAG_TC) == RESET)
        ;
}

/* ============================================================================
 * 通用命令回调
 * ============================================================================ */

static uint8_t power_on_get_type(const protocol_frame_t *req,
                                 uint8_t *resp_buf, uint16_t resp_size,
                                 uint8_t *resp_len)
{
    (void)req;
    (void)resp_size;
    resp_buf[0] = MODULE_TYPE_POWER;
    resp_buf[1] = MODULE_SUBTYPE_POWER_STANDARD;
    resp_buf[2] = 0x01; /* hw_ver */
    resp_buf[3] = 0x01; /* fw_major */
    resp_buf[4] = 0x00; /* fw_minor */
    *resp_len = 5;
    return 1;
}

static const protocol_common_cb_t power_common_cb = {
    .on_get_type = power_on_get_type,
};

/* ============================================================================
 * 主循环帧处理
 * ============================================================================ */

void Power_Process(power_t *power)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;
    uint8_t handled = 0;
    protocol_frame_t *req;

    if (power == NULL || !power->rx_ctx.frame_ready)
        return;

    req = &power->rx_ctx.frame;

    /* 0. 处理 Power 返回的 ACK 响应（心跳 GET_TYPE 的回复）。
     * Power 把电量/充电作为 ACK 扩展字段 DATA[5]/DATA[6] 单帧携带：
     * 模块链路是单帧 rx_ctx，ACK+STATUS 背靠背双帧会导致前帧被
     * 后帧帧头顶掉（err_frame_ready），所以不能分两帧发。 */
    if (req->cmd == CMD_ACK)
    {
        module_identity_t id;
        if (Protocol_ParseIdentity(req, &id))
        {
            Hardware_Hb_MarkOnline(MODULE_ID_POWER, id.type, id.subtype);
            power->type_id = id.subtype;
        }
        if (req->len >= 8)   /* CMD + 身份5 + 电量1 + 充电1 */
            power_update_status(req->data[5], req->data[6]);
        handled = 1;
    }
    /* 1. 通用命令（0x00~0x0F） */
    else if (ProtocolCommon_Dispatch(req, &power_common_cb,
                                resp, sizeof(resp), &resp_len))
    {
        handled = 1;
    }
    /* 2. Power 专用：电量状态上报（Power→Core，事件类，无需响应） */
    else if (req->cmd == CMD_PWR_STATUS_REPORT)
    {
        Power_HandleStatusReport(req);
        handled = 1;
    }

    /* 发送响应（如果有） */
    if (handled && resp_len > 0)
        Power_Send_Data(power, resp, resp_len);

    Protocol_ResetRxCtx(&power->rx_ctx);
}

/* ============================================================================
 * Power 专用命令 handler 实现（框架，待填充业务逻辑）
 * ============================================================================ */

/* ---- 电量/充电状态统一入口：缓存 + 仅变化时推送 Display ---- */
static void power_update_status(uint8_t new_batt, uint8_t new_charge)
{
    if (power_ptr == NULL)
        return;

    if (new_batt > 100) new_batt = 100;
    new_charge = new_charge ? 1 : 0;

    /* 仅在电量或充电状态变化时才主动推送（周期上报不重复推） */
    if (power_ptr->has_status &&
        power_ptr->battery_pct == new_batt &&
        power_ptr->charge_flags == new_charge)
        return;

    power_ptr->battery_pct = new_batt;
    power_ptr->charge_flags = new_charge;   /* bit0 = 充电中 */
    power_ptr->has_status  = 1;

    Power_ReportStatusToDisplay(power_ptr);
}

/* ---- 电量上报（Power→Core，事件类）: DATA=[电量:1][充电:1] ----
 * 兼容保留：当前 Power 固件电量随 ACK 扩展字段携带，不再单独发 0x36 */
static void Power_HandleStatusReport(const protocol_frame_t *req)
{
    if (req->len < 2)   /* CMD + 电量 */
        return;
    power_update_status(req->data[0],
                        (req->len >= 3) ? req->data[1] : 0);
}

/*********************************************************************
 * @fn      Power_ReportStatusToDisplay
 *
 * @brief   重发最新电量/充电状态给 Display（供进页拉取一次）。
 *          evt_data = [电量:1][状态位图:1]
 *********************************************************************/
void Power_ReportStatusToDisplay(power_t *power)
{
    uint8_t d[2];
    if (power == NULL || !power->has_status)
        return;
    d[0] = power->battery_pct;
    d[1] = power->charge_flags;
    Display_SendPowerEvent(&display_g, POWER_EVT_STATUS_CHANGE, d, 2);
}
