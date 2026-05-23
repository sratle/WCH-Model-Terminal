#include "power.h"
#include "../Protocol/protocol_common.h"
#include "../hardware.h"

power_t *power_ptr = NULL;

/* ============================================================================
 * 前向声明：Power 专用命令 handler
 * ============================================================================ */

/* 请求类 handler：返回 1=成功（resp_buf 写入 DATA 域，*resp_len=DATA 长度），0=失败 */
static uint8_t Power_HandleGetStatus(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size,
                                     uint8_t *resp_len);
static uint8_t Power_HandleGetBatteryInfo(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size,
                                          uint8_t *resp_len);
static uint8_t Power_HandleSetChargePolicy(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len);
static uint8_t Power_HandleSetOutputPolicy(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len);
static uint8_t Power_HandleSetAlarmThreshold(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len);

/* 事件类 handler：无返回值 */
static void Power_HandleStatusReport(const protocol_frame_t *req);
static void Power_HandleChargeEvent(const protocol_frame_t *req);
static void Power_HandleAlarmEvent(const protocol_frame_t *req);

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

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(POWER_UART, USART_FLAG_TC) == RESET)
            ;
        USART_SendData(POWER_UART, *data++);
    }
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
static uint8_t power_dispatch_req(const protocol_frame_t *req,
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

void Power_Process(power_t *power)
{
    uint8_t resp[PROTO_MAX_FRAME_LEN];
    uint8_t resp_len = 0;
    uint8_t handled = 0;
    protocol_frame_t *req;

    if (power == NULL || !power->rx_ctx.frame_ready)
        return;

    req = &power->rx_ctx.frame;

    /* 0. 处理 Power 返回的 ACK 响应（心跳 GET_TYPE 的回复） */
    if (req->cmd == CMD_ACK)
    {
        module_identity_t id;
        if (Protocol_ParseIdentity(req, &id))
        {
            Hardware_Hb_MarkOnline(MODULE_ID_POWER, id.type, id.subtype);
            power->type_id = id.subtype;
        }
        handled = 1;
    }
    /* 1. 通用命令（0x00~0x0F） */
    else if (ProtocolCommon_Dispatch(req, &power_common_cb,
                                resp, sizeof(resp), &resp_len))
    {
        handled = 1;
    }
    /* 2. Power 专用基础操作码：0x31 ~ 0x38 */
    else if (req->cmd >= CMD_PWR_GET_STATUS && req->cmd <= CMD_PWR_ALARM_EVENT)
    {
        switch (req->cmd)
        {
            /* ---- Core→Power 查询类 ---- */
            case CMD_PWR_GET_STATUS:
                resp_len = power_dispatch_req(req, Power_HandleGetStatus,
                                              resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_PWR_GET_BATTERY_INFO:
                resp_len = power_dispatch_req(req, Power_HandleGetBatteryInfo,
                                              resp, sizeof(resp));
                handled = 1;
                break;

            /* ---- Core→Power 设置类（发送即忘，但防御性处理） ---- */
            case CMD_PWR_SET_CHARGE_POLICY:
                resp_len = power_dispatch_req(req, Power_HandleSetChargePolicy,
                                              resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_PWR_SET_OUTPUT_POLICY:
                resp_len = power_dispatch_req(req, Power_HandleSetOutputPolicy,
                                              resp, sizeof(resp));
                handled = 1;
                break;
            case CMD_PWR_SET_ALARM_THRESHOLD:
                resp_len = power_dispatch_req(req, Power_HandleSetAlarmThreshold,
                                              resp, sizeof(resp));
                handled = 1;
                break;

            /* ---- Power→Core 事件类（不需要 ACK） ---- */
            case CMD_PWR_STATUS_REPORT:
                Power_HandleStatusReport(req);
                handled = 1;
                break;
            case CMD_PWR_CHARGE_EVENT:
                Power_HandleChargeEvent(req);
                handled = 1;
                break;
            case CMD_PWR_ALARM_EVENT:
                Power_HandleAlarmEvent(req);
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
        Power_Send_Data(power, resp, resp_len);

    Protocol_ResetRxCtx(&power->rx_ctx);
}

/* ============================================================================
 * Power 专用命令 handler 实现（框架，待填充业务逻辑）
 * ============================================================================ */

/* ---- 查询类 handler ---- */
static uint8_t Power_HandleGetStatus(const protocol_frame_t *req,
                                     uint8_t *resp_buf, uint16_t resp_size,
                                     uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 返回当前电源完整状态
     * resp_buf[0] = 状态位图
     * resp_buf[1] = 电量百分比
     * resp_buf[2..3] = 电压(mV, uint16大端)
     * resp_buf[4..5] = 电流(mA, int16大端)
     * resp_buf[6] = 温度(°C, int8)
     * resp_buf[7..8] = 功率(mW, uint16大端)
     * *resp_len = 9;
     */
    return 0;
}

static uint8_t Power_HandleGetBatteryInfo(const protocol_frame_t *req,
                                          uint8_t *resp_buf, uint16_t resp_size,
                                          uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 返回电池详细信息
     * resp_buf[0..1] = 设计容量(mAh, uint16大端)
     * resp_buf[2..3] = 实际容量(mAh, uint16大端)
     * resp_buf[4..5] = 循环次数(uint16大端)
     * resp_buf[6] = 健康度(%)
     * resp_buf[7] = 电池类型
     * *resp_len = 8;
     */
    return 0;
}

/* ---- 设置类 handler ---- */
static uint8_t Power_HandleSetChargePolicy(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 解析 req->data[0]=充电策略，设置充电模式 */
    return 0;
}

static uint8_t Power_HandleSetOutputPolicy(const protocol_frame_t *req,
                                           uint8_t *resp_buf, uint16_t resp_size,
                                           uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 解析 req->data[0]=输出开关, data[1..2]=限制功率，设置充电宝输出策略 */
    return 0;
}

static uint8_t Power_HandleSetAlarmThreshold(const protocol_frame_t *req,
                                             uint8_t *resp_buf, uint16_t resp_size,
                                             uint8_t *resp_len)
{
    (void)req; (void)resp_buf; (void)resp_size;
    *resp_len = 0;
    /* TODO: 解析 req->data[0]=低电量阈值, data[1]=过温阈值, data[2]=关机阈值 */
    return 0;
}

/* ---- 事件类 handler ---- */
static void Power_HandleStatusReport(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析周期性状态上报，更新 Core 侧电源状态 */
}

static void Power_HandleChargeEvent(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析充电状态变化事件（插入/拔出/充满/切换快充等） */
}

static void Power_HandleAlarmEvent(const protocol_frame_t *req)
{
    (void)req;
    /* TODO: 解析告警事件（低电量/过温/过流/电池健康度低） */
}
