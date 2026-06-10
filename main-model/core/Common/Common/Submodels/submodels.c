#include "submodels.h"
#include "../Protocol/protocol_common.h"
#include "../hardware.h"
#include "../Display/display.h"
#include "../CS43131/cs43131.h"
#include "../Config/config.h"
#include "../CH378/CH378.h"
#include "../CH9350/CH9350.h"
#include "../CH585F/CH585F.h"

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

    /* 0. 处理 GET_TYPE ACK 响应
     * GET_TYPE ACK 格式: [type:1][subtype:1][hw_ver:1][fw_major:1][fw_minor:1]
     * type == MODULE_TYPE_SUBMODEL(0x05) 且 len == 6 (CMD+5数据字节) */
    if (req->cmd == CMD_ACK && req->len == 6 &&
        req->data[0] == MODULE_TYPE_SUBMODEL)
    {
        uint8_t module_id = MODULE_ID_SUBMODEL_1 + (submodel->submodels_id - 1);

        /* type_id 未确定时，记录类型 */
        if (submodel->type_id == MODULE_SUBTYPE_SUBMODEL_RESERVED)
            submodel->type_id = req->data[1];

        Hardware_Hb_MarkOnline(module_id, req->data[0], req->data[1]);

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
    /* 3. 指纹模块的 ACK / ACTION_RESULT 响应 */
    else if (submodel->type_id == MODULE_SUBTYPE_SUBMODEL_FINGERPRINT &&
             (req->cmd == CMD_ACK || req->cmd == CMD_SUB_ACTION_RESULT))
    {
        handled = submodels_fp_dispatch(submodel, req, resp, sizeof(resp), &resp_len);
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
        /* ---- ACK 响应：Submodel → Core（查询结果） ---- */
        case CMD_ACK:
        {
            /* 区分查询类型：
             * QUERY_LIST ACK: [count:1][id0:1][id1:1]...] (len-1 字节数据)
             * QUERY_COUNT ACK: [count:1] (仅 1 字节) */
            if (req->len == 2)
            {
                /* QUERY_COUNT 响应: data[0] = count */
                printf("[FP] Fingerprint count: %d\r\n", req->data[0]);
            }
            else
            {
                /* QUERY_LIST 响应: data[0] = count, data[1..N] = IDs */
                uint8_t count = req->data[0];
                uint8_t i;
                printf("[FP] Fingerprint list: count=%d\r\n", count);
                for (i = 0; i < count && (1 + i) < (req->len - 1); i++)
                {
                    printf("  ID=%d\r\n", req->data[1 + i]);
                }
            }
            return 1;
        }

        /* ---- 注册结果：Submodel → Core ---- */
        case CMD_SUB_ACTION_RESULT:
        {
            switch (subcmd)
            {
                case FP_SUB_ENROLL_OK:
                {
                    uint8_t fp_id = (req->len >= 2) ? req->data[1] : 0;
                    printf("[FP] Enroll OK: ID=%d\r\n", fp_id);
                    return 1;
                }
                case FP_SUB_ENROLL_FAIL:
                {
                    uint8_t err = (req->len >= 2) ? req->data[1] : 0;
                    printf("[FP] Enroll FAIL: err=%d\r\n", err);
                    return 1;
                }
            }
            break;
        }

        /* ---- 请求类：Core → Fingerprint (发送即忘，不响应) ---- */
        case CMD_SUB_SET_MODE:
        case CMD_SUB_SET_CONFIG:
            /* 协议规定 SET_MODE / SET_CONFIG 为发送即忘，不生成响应 */
            *resp_len = 0;
            return 1;

        /* ---- 事件类：Fingerprint → Core (Submodel 主动上报) ---- */
        case CMD_SUB_EVT_NOTIFY:
            switch (subcmd)
            {
                case FP_SUB_IDENTIFY_OK:
                {
                    uint8_t fp_id = (req->len >= 2) ? req->data[1] : 0;
                    printf("[FP] Identify OK: ID=%d\r\n", fp_id);

                    /* Forward to Display if online */
                    {
                        uint8_t i;
                        for (i = 0; i < HB_MAX_SLOTS; i++)
                        {
                            if (hardware_g.hb_slots[i].module_id == MODULE_ID_DISPLAY &&
                                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
                            {
                                uint8_t evt[2];
                                evt[0] = FP_SUB_IDENTIFY_OK;
                                evt[1] = fp_id;
                                Display_SendSubmodelEvent(&display_g,
                                    MODULE_SUBTYPE_SUBMODEL_FINGERPRINT,
                                    FP_SUB_IDENTIFY_OK,
                                    evt, 2);
                                break;
                            }
                        }
                    }
                    return 1;
                }
                case FP_SUB_IDENTIFY_FAIL:
                    printf("[FP] Identify FAIL\r\n");

                    /* Forward to Display if online */
                    {
                        uint8_t i;
                        for (i = 0; i < HB_MAX_SLOTS; i++)
                        {
                            if (hardware_g.hb_slots[i].module_id == MODULE_ID_DISPLAY &&
                                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
                            {
                                uint8_t evt[1];
                                evt[0] = FP_SUB_IDENTIFY_FAIL;
                                Display_SendSubmodelEvent(&display_g,
                                    MODULE_SUBTYPE_SUBMODEL_FINGERPRINT,
                                    FP_SUB_IDENTIFY_FAIL,
                                    evt, 1);
                                break;
                            }
                        }
                    }
                    return 1;
                case FP_SUB_ENROLL_PROGRESS:
                {
                    /* 新格式: [subcmd:1][step:1][sub_step:1][ack:1] */
                    uint8_t step = (req->len >= 2) ? req->data[1] : 0;
                    uint8_t sub_step = (req->len >= 3) ? req->data[2] : 0;
                    uint8_t ack = (req->len >= 4) ? req->data[3] : 0;
                    const char *step_name;

                    switch (step)
                    {
                        case 0x00: step_name = "started"; break;
                        case 0x01: step_name = "capture"; break;
                        case 0x02: step_name = "extract"; break;
                        case 0x03: step_name = "lift_finger"; break;
                        case 0x04: step_name = "merge"; break;
                        case 0x05: step_name = "verify"; break;
                        case 0x06: step_name = "store"; break;
                        default:   step_name = "unknown"; break;
                    }

                    if (ack == 0x00)
                        printf("[FP] Enroll: %s (#%d)\r\n", step_name, sub_step);
                    else
                        printf("[FP] Enroll: %s err=0x%02X\r\n", step_name, ack);
                    return 1;
                }
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
                case HEALTH_SUB_SET_INTERVAL: /* 设置监测间隔（秒） */
                    /* req->data[1..2] = 间隔(uint16大端) */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case HEALTH_SUB_START_MONITOR: /* 开始监测 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case HEALTH_SUB_STOP_MONITOR: /* 停止监测 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_GET_STATUS:
            switch (subcmd)
            {
                case HEALTH_SUB_QUERY_DATA: /* 查询当前数据 */
                    /* ACK + [心跳:1][血氧:1][HRV:2(uint16大端)] */
                    *resp_len = 0;
                    return 0;
            }
            break;

        case CMD_SUB_DATA_REPORT:
            switch (subcmd)
            {
                case HEALTH_SUB_DATA_REPORT: /* 健康数据上报 */
                {
                    /* DATA: [子命令:1][心跳:1][血氧:1][HRV:2(BE)] */
                    if (req->len >= 5)
                    {
                        uint8_t hr = req->data[1];
                        uint8_t spo2 = req->data[2];
                        uint16_t hrv = ((uint16_t)req->data[3] << 8) | req->data[4];

                        printf("[Health] HR=%d BPM, SpO2=%d%%, HRV=%d ms\r\n",
                               hr, spo2, hrv);

                        /* Forward to Display if online */
                        {
                            uint8_t i;
                            for (i = 0; i < HB_MAX_SLOTS; i++)
                            {
                                if (hardware_g.hb_slots[i].module_id == MODULE_ID_DISPLAY &&
                                    hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
                                {
                                    /* Re-pack: [HR:1][SpO2:1][HRV:2(BE)] */
                                    uint8_t evt[4];
                                    evt[0] = hr;
                                    evt[1] = spo2;
                                    evt[2] = (uint8_t)(hrv >> 8);
                                    evt[3] = (uint8_t)(hrv & 0xFF);
                                    Display_SendSubmodelEvent(&display_g,
                                        MODULE_SUBTYPE_SUBMODEL_HEALTH,
                                        HEALTH_SUB_DATA_REPORT,
                                        evt, 4);
                                    break;
                                }
                            }
                        }
                    }
                    return 1;
                }
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

/* ---- 4. 触摸圆环 (type = 0x04, submodel-4) ----
 *
 * 协议见 Protocol_Submodels.md §4.4
 *
 * Touch → Core:
 *   CMD_SUB_DATA_REPORT(0x43) SUB=0x01  触摸位图上报  [subcmd:1][bitmap_hi:1][bitmap_lo:1]
 *   CMD_SUB_ACTION_RESULT(0x45)          预留
 *
 * Core → Touch:
 *   CMD_SUB_GET_STATUS(0x42) SUB=0x00    查询触摸状态 → ACK [bitmap_hi:1][bitmap_lo:1]
 *   CMD_SUB_SET_MODE(0x41)   SUB=0x01    重新校准基线（发送即忘）
 *
 * bitmap: bit0=Key1(TP9,方阵左上) … bit15=Key16(TP12,圆环11点钟), 1=touched
 */
static uint8_t submodels_touch_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                        uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;
    (void)submodel;

    switch (cmd)
    {
        /* ---- Touch → Core: 触摸位图上报 ---- */
        case CMD_SUB_DATA_REPORT:
            switch (subcmd)
            {
                case 0x01: /* 触摸位图 [bitmap_hi:1][bitmap_lo:1] */
                    if (req->len >= 3)
                    {
                        uint16_t bitmap = ((uint16_t)req->data[1] << 8) | req->data[2];
                        printf("[Touch] bitmap=0x%04X\r\n", bitmap);
                    }
                    *resp_len = 0;
                    return 1;   /* 事件帧无需回复 */

                default:
                    break;
            }
            break;

        /* ---- Core → Touch: 查询触摸状态 ---- */
        case CMD_SUB_GET_STATUS:
            switch (subcmd)
            {
                case 0x00: /* 查询触摸状态 → ACK [bitmap_hi:1][bitmap_lo:1] */
                    /* Core 侧不缓存触摸状态，返回 0；实际触摸数据由 DATA_REPORT 主动上报 */
                    resp[0] = 0x00;
                    resp[1] = 0x00;
                    *resp_len = 2;
                    return 1;

                default:
                    break;
            }
            break;

        /* ---- Core → Touch: 重新校准基线（发送即忘） ---- */
        case CMD_SUB_SET_MODE:
            switch (subcmd)
            {
                case 0x01: /* 重新校准基线 */
                    *resp_len = 0;
                    return 1;   /* 发送即忘，不响应 */

                default:
                    break;
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

/* Forward declaration */
static uint8_t submodel_module_id(const submodels_t *submodel);

/* BMP 根目录 */
#define BMP_ROOT_DIR  "\\BMP"

/* BMP 文件最大尺寸（1bit 位图 122×250 ≈ 3813 字节，留余量） */
#define BMP_MAX_RAW_SIZE  8192

/* 多帧 payload 最大值：PackFrame data_len 最大 254 - subcmd(1) - flags(1) = 252 */
#define SUBDISP_FRAME_PAYLOAD  252  /* PackFrame data_len 最大 254，扣除 subcmd(1)+flags(1)=2 */
/* 首帧额外头(8字节)：宽(2)+高(2)+总大小(4) */
#define SUBDISP_FIRST_PAYLOAD (SUBDISP_FRAME_PAYLOAD - 8)

/**
 * @brief  构建 BMP 目录下文件的完整路径
 */
static void BMP_MakePath(const char *filename, char *out, uint16_t out_len)
{
    snprintf(out, out_len, "%s\\%s.BMP", BMP_ROOT_DIR, filename);
}

/**
 * @brief  读取 BMP 文件并解析为 1bit 位图原始数据
 * @param  filename   BMP 文件名（不含路径和后缀）
 * @param  out_buf    输出缓冲区
 * @param  out_size   缓冲区大小
 * @param  out_width  输出图片宽度
 * @param  out_height 输出图片高度
 * @return 实际位图数据字节数，0=失败
 */
static uint32_t BMP_ReadAndParse(const char *filename, uint8_t *out_buf,
                                  uint32_t out_size,
                                  uint16_t *out_width, uint16_t *out_height)
{
    char path[CH378_MAX_PATH_LEN];
    uint8_t status;
    uint16_t real_len;
    uint32_t total = 0;
    uint32_t file_size;
    uint8_t header_buf[62]; /* BMP header 最大读取 */
    uint32_t data_offset;
    uint32_t img_width, img_height;
    uint16_t bpp;
    uint32_t compression;
    uint32_t row_size, raw_row_size;
    uint32_t i, row;

    BMP_MakePath(filename, path, sizeof(path));

    /* 打开文件 */
    status = CH378FileOpen((uint8_t *)path);
    if (status != ERR_SUCCESS)
        return 0;

    file_size = CH378GetFileSize();
    if (file_size < 14 || file_size > 256 * 1024)
    {
        CH378FileClose(0);
        return 0;
    }

    /* 读取 BMP 文件头（至少 54 字节） */
    {
        uint32_t to_read = (file_size < 62) ? file_size : 62;
        uint32_t hdr_total = 0;
        while (hdr_total < to_read)
        {
            uint16_t chunk = (uint16_t)((to_read - hdr_total) > 255 ? 255 : (to_read - hdr_total));
            status = CH378ByteRead(header_buf + hdr_total, chunk, &real_len);
            if (status != ERR_SUCCESS || real_len == 0)
            {
                CH378FileClose(0);
                return 0;
            }
            hdr_total += real_len;
        }
    }

    /* 验证 BMP 签名 "BM" */
    if (header_buf[0] != 'B' || header_buf[1] != 'M')
    {
        CH378FileClose(0);
        return 0;
    }

    /* 解析 BMP 头 */
    data_offset = ((uint32_t)header_buf[13] << 24) | ((uint32_t)header_buf[12] << 16) |
                  ((uint32_t)header_buf[11] << 8) | header_buf[10];
    img_width   = ((uint32_t)header_buf[21] << 24) | ((uint32_t)header_buf[20] << 16) |
                  ((uint32_t)header_buf[19] << 8) | header_buf[18];
    img_height  = ((uint32_t)header_buf[25] << 24) | ((uint32_t)header_buf[24] << 16) |
                  ((uint32_t)header_buf[23] << 8) | header_buf[22];
    bpp         = ((uint16_t)header_buf[29] << 8) | header_buf[28];
    compression = ((uint32_t)header_buf[33] << 24) | ((uint32_t)header_buf[32] << 16) |
                  ((uint32_t)header_buf[31] << 8) | header_buf[30];

    /* 仅支持 1bit 无压缩 BMP */
    if (bpp != 1 || compression != 0)
    {
        CH378FileClose(0);
        return 0;
    }

    /* 限制尺寸 */
    if (img_width > 250 || img_height > 250 || img_width == 0 || img_height == 0)
    {
        CH378FileClose(0);
        return 0;
    }

    /* 计算行字节数（BMP 行 4 字节对齐） */
    raw_row_size = ((img_width + 31) / 32) * 4;  /* 源文件行字节数（含对齐） */
    row_size = (img_width + 7) / 8;               /* 目标行字节数（紧凑） */
    total = row_size * img_height;

    if (total > out_size)
    {
        CH378FileClose(0);
        return 0;
    }

    /* 读取像素数据（从 data_offset 开始） */
    /* 先 seek 到 data_offset：关闭重开再跳过 */
    CH378FileClose(0);
    status = CH378FileOpen((uint8_t *)path);
    if (status != ERR_SUCCESS)
        return 0;

    /* 跳过头部字节 */
    {
        uint32_t skip = data_offset;
        uint8_t dummy[255];
        while (skip > 0)
        {
            uint16_t chunk = (skip > 255) ? 255 : (uint16_t)skip;
            status = CH378ByteRead(dummy, chunk, &real_len);
            if (status != ERR_SUCCESS || real_len == 0)
            {
                CH378FileClose(0);
                return 0;
            }
            skip -= real_len;
        }
    }

    /* 逐行读取并转换（BMP 是倒序存储的） */
    {
        uint8_t row_buf[128]; /* 250/8 = 32, 对齐后最大 32 字节，留余量 */
        for (row = 0; row < img_height; row++)
        {
            uint32_t target_row = img_height - 1 - row; /* BMP 倒序 → 正序 */
            uint32_t to_read = raw_row_size;
            uint32_t row_read = 0;

            while (row_read < to_read)
            {
                uint16_t chunk = (uint16_t)((to_read - row_read) > 255 ? 255 : (to_read - row_read));
                status = CH378ByteRead(row_buf + row_read, chunk, &real_len);
                if (status != ERR_SUCCESS || real_len == 0)
                {
                    CH378FileClose(0);
                    return 0;
                }
                row_read += real_len;
            }

            /* 复制紧凑行数据到输出缓冲区（去除对齐填充） */
            for (i = 0; i < row_size; i++)
                out_buf[target_row * row_size + i] = row_buf[i];
        }
    }

    CH378FileClose(0);

    *out_width = (uint16_t)img_width;
    *out_height = (uint16_t)img_height;
    return total;
}

/**
 * @brief  发送多帧数据到副屏（通用多帧发送）
 * @param  submodel    目标 submodel
 * @param  subcmd      子命令
 * @param  header      首帧额外头部数据（如图片宽高大小等），可为 NULL
 * @param  header_len  首帧头部长度
 * @param  data        数据缓冲区
 * @param  data_len    数据长度
 * @param  cmd         协议命令字
 * @return 1=成功, 0=失败
 */
static uint8_t subdisp_send_multiframe(submodels_t *submodel, uint8_t subcmd,
                                        const uint8_t *header, uint8_t header_len,
                                        const uint8_t *data, uint32_t data_len,
                                        uint8_t cmd)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t frame_len;
    uint8_t frame_data[255];
    uint8_t frame_data_len;
    uint32_t offset = 0;
    uint8_t first_frame = 1;
    uint8_t flags;

    if (subcmd == SUBDISP_SUBCMD_BMP_TRANS){
        printf("[SubDisp] MF: subcmd=0x%02X data=%lu hdr=%u\r\n",
               subcmd, (unsigned long)data_len, header_len);
    }

    while (offset < data_len || first_frame)
    {
        uint16_t payload_max;
        uint32_t remaining = data_len - offset;
        uint16_t chunk_len;

        if (first_frame)
        {
            /* 首帧：subcmd(1) + flags(1) + header + data */
            payload_max = SUBDISP_FRAME_PAYLOAD - header_len;
            chunk_len = (remaining > payload_max) ? payload_max : (uint16_t)remaining;

            flags = SUBDISP_FLAG_SOF;
            if (offset + chunk_len >= data_len)
                flags |= SUBDISP_FLAG_EOF;

            frame_data[0] = subcmd;
            frame_data[1] = flags;
            if (header_len > 0)
                memcpy(&frame_data[2], header, header_len);
            memcpy(&frame_data[2 + header_len], data + offset, chunk_len);
            frame_data_len = 2 + header_len + chunk_len;

            first_frame = 0;
        }
        else
        {
            /* 后续帧：subcmd(1) + flags(1) + data */
            chunk_len = (remaining > SUBDISP_FRAME_PAYLOAD) ? SUBDISP_FRAME_PAYLOAD : (uint16_t)remaining;

            flags = 0x00;
            if (offset + chunk_len >= data_len)
                flags |= SUBDISP_FLAG_EOF;

            frame_data[0] = subcmd;
            frame_data[1] = flags;
            memcpy(&frame_data[2], data + offset, chunk_len);
            frame_data_len = 2 + chunk_len;
        }

        frame_len = Protocol_PackFrame(MODULE_ID_CORE,
                                       submodel_module_id(submodel),
                                       cmd, frame_data, frame_data_len,
                                       buf, sizeof(buf));
        if (frame_len == 0)
        {
            printf("[SubDisp] MF PackFrame FAIL: data_len=%u\r\n", frame_data_len);
            return 0;
        }

        Submodels_Send_Data(submodel, buf, frame_len);

        offset += chunk_len;

        if (flags & SUBDISP_FLAG_EOF)
            break;

        /* 帧间延迟，避免 submodel-7 接收不过来 */
        Delay_Ms(3);
    }

    return 1;
}

static uint8_t submodels_subdisp_dispatch(submodels_t *submodel, const protocol_frame_t *req,
                                          uint8_t *resp, uint16_t resp_size, uint8_t *resp_len)
{
    uint8_t cmd = req->cmd;
    uint8_t subcmd = (req->len >= 2) ? req->data[0] : 0;

    switch (cmd)
    {
        case CMD_SUB_SET_MODE:
            switch (subcmd)
            {
                case SUBDISP_SUBCMD_SET_STATUS: /* 0x01 传输模块状态 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case SUBDISP_SUBCMD_SET_CONTENT: /* 0x02 传输自定义内容 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case SUBDISP_SUBCMD_CLEAR_SCREEN: /* 0x03 清屏 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                case SUBDISP_SUBCMD_SET_DISPLAY_MODE: /* 0x04 切换显示模式 */
                    return ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
            }
            break;

        case CMD_SUB_GET_STATUS:
            switch (subcmd)
            {
                case SUBDISP_SUBCMD_GET_SYS_STATUS: /* 0x00 请求系统状态 */
                    /* Core 收到请求后主动发送系统状态，此处先 ACK */
                    *resp_len = ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                    /* 异步发送系统状态 */
                    Submodels_SubDisp_SendSysStatus(submodel);
                    return 1;

                case SUBDISP_SUBCMD_GET_LS_DEV: /* 0x01 请求设备列表 */
                    *resp_len = ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                    Submodels_SubDisp_SendLsDev(submodel);
                    return 1;

                case SUBDISP_SUBCMD_GET_BMP: /* 0x02 请求 BMP 图片 */
                {
                    /* data[1..N] = 文件名（不含路径和后缀） */
                    char filename[64] = {0};
                    uint8_t fn_len = req->len - 1;
                    if (fn_len == 0 || fn_len >= sizeof(filename))
                    {
                        *resp_len = ProtocolCommon_Nack(req->dst, req->src,
                                                        PROTO_ERR_INVALID_PARAM,
                                                        resp, resp_size);
                        return 1;
                    }
                    memcpy(filename, &req->data[1], fn_len);
                    filename[fn_len] = '\0';

                    *resp_len = ProtocolCommon_Ack(req->dst, req->src, resp, resp_size);
                    Submodels_SubDisp_SendBMP(submodel, filename);
                    return 1;
                }
            }
            break;

        case CMD_SUB_BULK_TRANSFER:
            switch (subcmd)
            {
                case 0x01: /* 图片传输握手 */
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

/* ============================================================================
 * Health (健康监测, type=0x02) Control API Implementation
 * ============================================================================ */

submodels_t *Submodels_FindHealthSlot(void)
{
    extern submodels_t submodels_g[3];
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        if (submodels_g[i].type_id == MODULE_SUBTYPE_SUBMODEL_HEALTH)
            return &submodels_g[i];
    }
    return NULL;
}

uint8_t Submodels_Health_StartMonitor(submodels_t *submodel)
{
    uint8_t data[1] = { HEALTH_SUB_START_MONITOR };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_CONFIG, data, 1);
}

uint8_t Submodels_Health_StopMonitor(submodels_t *submodel)
{
    uint8_t data[1] = { HEALTH_SUB_STOP_MONITOR };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_CONFIG, data, 1);
}

uint8_t Submodels_Health_SetInterval(submodels_t *submodel, uint16_t seconds)
{
    uint8_t data[3];
    data[0] = HEALTH_SUB_SET_INTERVAL;
    data[1] = (uint8_t)(seconds >> 8);
    data[2] = (uint8_t)(seconds & 0xFF);
    return Submodels_SendCommand(submodel, CMD_SUB_SET_CONFIG, data, 3);
}

uint8_t Submodels_Health_QueryData(submodels_t *submodel)
{
    uint8_t data[1] = { HEALTH_SUB_QUERY_DATA };
    return Submodels_SendCommand(submodel, CMD_SUB_GET_STATUS, data, 1);
}

/* ============================================================================
 * SubDisplay (副屏) Control API Implementation
 * ============================================================================ */

submodels_t *Submodels_FindSubDispSlot(void)
{
    extern submodels_t submodels_g[3];
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        if (submodels_g[i].type_id == MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY)
            return &submodels_g[i];
    }
    return NULL;
}

/* ============================================================================
 * Fingerprint (指纹识别, type=0x01) Control API Implementation
 * ============================================================================ */

submodels_t *Submodels_FindFpSlot(void)
{
    extern submodels_t submodels_g[3];
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        if (submodels_g[i].type_id == MODULE_SUBTYPE_SUBMODEL_FINGERPRINT)
            return &submodels_g[i];
    }
    return NULL;
}

submodels_t *Submodels_FindTouchSlot(void)
{
    extern submodels_t submodels_g[3];
    uint8_t i;

    for (i = 0; i < 3; i++)
    {
        if (submodels_g[i].type_id == MODULE_SUBTYPE_SUBMODEL_TOUCH_RING)
            return &submodels_g[i];
    }
    return NULL;
}

uint8_t Submodels_FP_EnrollStart(submodels_t *submodel)
{
    uint8_t data[1] = { FP_SUB_ENROLL_START };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 1);
}

uint8_t Submodels_FP_EnrollCancel(submodels_t *submodel)
{
    uint8_t data[1] = { FP_SUB_ENROLL_CANCEL };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 1);
}

uint8_t Submodels_FP_Delete(submodels_t *submodel, uint8_t fp_id)
{
    uint8_t data[2] = { FP_SUB_DELETE, fp_id };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_CONFIG, data, 2);
}

uint8_t Submodels_FP_DeleteAll(submodels_t *submodel)
{
    uint8_t data[1] = { FP_SUB_DELETE_ALL };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_CONFIG, data, 1);
}

uint8_t Submodels_FP_QueryCount(submodels_t *submodel)
{
    uint8_t data[1] = { FP_SUB_QUERY_COUNT };
    return Submodels_SendCommand(submodel, CMD_SUB_GET_STATUS, data, 1);
}

uint8_t Submodels_FP_QueryList(submodels_t *submodel)
{
    uint8_t data[1] = { FP_SUB_QUERY_LIST };
    return Submodels_SendCommand(submodel, CMD_SUB_GET_STATUS, data, 1);
}

uint8_t Submodels_FP_SetLED(submodels_t *submodel, uint8_t func,
                             uint8_t color, uint8_t speed)
{
    uint8_t data[4] = { FP_SUB_SET_LED, func, color, speed };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 4);
}

uint8_t Submodels_FP_SetSecurity(submodels_t *submodel, uint8_t level)
{
    uint8_t data[2] = { FP_SUB_SET_SECURITY, level };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 2);
}

uint8_t Submodels_FP_Sleep(submodels_t *submodel)
{
    uint8_t data[1] = { FP_SUB_SLEEP };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_CONFIG, data, 1);
}

uint8_t Submodels_SubDisp_SendBMP(submodels_t *submodel, const char *filename)
{
    static uint8_t bmp_buf[BMP_MAX_RAW_SIZE];
    uint16_t width = 0, height = 0;
    uint32_t total;
    uint8_t header[8];

    if (submodel == NULL || filename == NULL)
        return 0;

    total = BMP_ReadAndParse(filename, bmp_buf, sizeof(bmp_buf), &width, &height);
    if (total == 0)
    {
        printf("[SubDisp] BMP parse FAIL: '%s'\r\n", filename);
        /* 发送失败帧：FLAGS=SOF|EOF, 宽度=0(2字节) */
        uint8_t fail_data[4] = { SUBDISP_SUBCMD_BMP_TRANS,
                                  SUBDISP_FLAG_SOF | SUBDISP_FLAG_EOF,
                                  0x00, 0x00 };
        return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, fail_data, 4);
    }

    /* 先发送文件名，再发送图片数据 */
    Submodels_SubDisp_SendBmpName(submodel, filename);

    /* 构建首帧头：宽(2) + 高(2) + 总大小(4) */
    header[0] = (uint8_t)(width >> 8);
    header[1] = (uint8_t)(width & 0xFF);
    header[2] = (uint8_t)(height >> 8);
    header[3] = (uint8_t)(height & 0xFF);
    header[4] = (uint8_t)(total >> 24);
    header[5] = (uint8_t)(total >> 16);
    header[6] = (uint8_t)(total >> 8);
    header[7] = (uint8_t)(total & 0xFF);

    return subdisp_send_multiframe(submodel, SUBDISP_SUBCMD_BMP_TRANS,
                                   header, 8,
                                   bmp_buf, total,
                                   CMD_SUB_SET_MODE);
}


uint8_t Submodels_SubDisp_SendBmpName(submodels_t *submodel, const char *filename)
{
    uint8_t data[2 + 20]; /* subcmd(1) + filename(max 20) + null */
    uint8_t len = 0;
    uint8_t i;

    if (submodel == NULL || filename == NULL)
        return 0;

    data[0] = SUBDISP_SUBCMD_SET_BMP_NAME;
    for (i = 0; filename[i] && i < 19; i++)
        data[1 + i] = (uint8_t)filename[i];
    data[1 + i] = '\0';
    len = 2 + i;

    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, len);
}

uint8_t Submodels_SubDisp_SendLsDev(submodels_t *submodel)
{
    uint8_t data[2 + HB_MAX_SLOTS * 5];
    uint8_t offset = 0;
    uint8_t i;

    if (submodel == NULL)
        return 0;

    /* 模块总数 */
    data[offset++] = HB_MAX_SLOTS;

    /* 各模块条目：[模块ID:1][类型:1][子类型:1][在线状态:1][miss_count:1] */
    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        const hb_slot_t *slot = &hardware_g.hb_slots[i];
        data[offset++] = slot->module_id;
        data[offset++] = slot->type;
        data[offset++] = slot->subtype;
        data[offset++] = (uint8_t)slot->status;
        data[offset++] = slot->miss_count;
    }

    return subdisp_send_multiframe(submodel, SUBDISP_SUBCMD_LS_DEV,
                                   data, offset,
                                   NULL, 0,
                                   CMD_SUB_SET_MODE);
}

uint8_t Submodels_SubDisp_SendSysStatus(submodels_t *submodel)
{
    /* 协议 v2 布局:
     *  [0]     version          = 0x02
     *  [1]     audio_state
     *  [2]     audio_volume
     *  [3..6]  audio_play_time_ms (BE)
     *  [7]     battery_pct
     *  [8]     charging
     *  [9]     ble_connections
     *  [10]    ble_discoverable
     *  [11]    display_brightness
     *  [12]    keyboard_backlight
     *  [13]    ch378_device
     *  [14]    ch378_busy
     *  [15]    ch9350_hid_count
     *  [16]    rgb_mode
     *  [17]    rgb_brightness
     *  [18]    config_loaded
     *  [19]    module_count
     *  ---- v2 新增 ----
     *  [20]    display_type      (0=无, 1=LCD, 2=EInk)
     *  [21]    keyboard_type     (0=无, 1=Main, 2=Game, 3=Music)
     *  [22]    ch9350_dev_type   (0=无, 1=键盘, 2=相对鼠标, 3=绝对鼠标, 4=多媒体, 5=扫描枪)
     *  [23]    audio_track_name_len
     *  [24..24+N-1] audio_track_name (最多 31 字节)
     *  [24+N..24+N+3] keyboard_key_count (BE, 总按键次数)
     *  模块条目 (每条 5 字节: module_id, type, subtype, status, miss_count)
     */
    uint8_t status_buf[64 + HB_MAX_SLOTS * 5];
    uint8_t offset = 0;
    uint8_t i;

    if (submodel == NULL)
        return 0;

    /* 版本 */
    status_buf[offset++] = 0x02;

    /* ---- 音频状态：从 CS43131_g 直接读取 ---- */
    status_buf[offset++] = (uint8_t)CS43131_g.audio_state;
    status_buf[offset++] = CS43131_g.volume;

    /* 播放时长 (uint32 大端) */
    {
        uint32_t play_time = Audio_GetPlayTime_ms();
        status_buf[offset++] = (uint8_t)(play_time >> 24);
        status_buf[offset++] = (uint8_t)(play_time >> 16);
        status_buf[offset++] = (uint8_t)(play_time >> 8);
        status_buf[offset++] = (uint8_t)(play_time & 0xFF);
    }

    /* 电量百分比（Power 模块暂无精确接口，默认 0xFF=未知） */
    status_buf[offset++] = 0xFF;

    /* 充电状态 */
    status_buf[offset++] = 0x00;

    /* 蓝牙连接数（CH585F 暂无接口，默认 0） */
    status_buf[offset++] = 0x00;

    /* 蓝牙可发现 */
    status_buf[offset++] = 0x00;

    /* 屏幕亮度 */
    {
        int brightness = 0;
        /* 根据实际连接的 display 子类型选择 config key */
        const char *disp_key = "0101";
        for (i = 0; i < HB_MAX_SLOTS; i++) {
            if (hardware_g.hb_slots[i].module_id == MODULE_ID_DISPLAY &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {
                if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_DISPLAY_EINK)
                    disp_key = "0102";
                break;
            }
        }
        Config_GetInt(disp_key, "brightness", &brightness);
        status_buf[offset++] = (uint8_t)brightness;
    }

    /* 键盘背光 */
    {
        int kbd_backlight = 0;
        const char *kbd_key = "0401";
        for (i = 0; i < HB_MAX_SLOTS; i++) {
            if (hardware_g.hb_slots[i].module_id == MODULE_ID_KEYBOARD &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {
                if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_KEYBOARD_GAME)
                    kbd_key = "0402";
                else if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_KEYBOARD_MUSIC)
                    kbd_key = "0403";
                break;
            }
        }
        Config_GetInt(kbd_key, "backlight", &kbd_backlight);
        status_buf[offset++] = (uint8_t)kbd_backlight;
    }

    /* CH378 设备 */
    status_buf[offset++] = (ch378_g.now_device == CH378_Device_TF) ? 0x01 : 0x00;

    /* CH378 忙碌 */
    status_buf[offset++] = (ch378_g.enable) ? 0x01 : 0x00;

    /* CH9350 HID 数量 */
    status_buf[offset++] = (ch9350_g.dev_connected) ? 0x01 : 0x00;

    /* RGB 模式 */
    status_buf[offset++] = hardware_g.rgb_config.mode;

    /* RGB 亮度 */
    status_buf[offset++] = hardware_g.rgb_config.brightness;

    /* 配置已加载 */
    status_buf[offset++] = Config_IsApplied() ? 0x01 : 0x00;

    /* 模块总数 */
    status_buf[offset++] = HB_MAX_SLOTS;

    /* ---- v2 新增字段 ---- */

    /* Display 类型：从心跳槽位中查找 */
    {
        uint8_t disp_type = 0;
        for (i = 0; i < HB_MAX_SLOTS; i++) {
            if (hardware_g.hb_slots[i].module_id == MODULE_ID_DISPLAY &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {
                disp_type = hardware_g.hb_slots[i].subtype;
                break;
            }
        }
        status_buf[offset++] = disp_type;
    }

    /* Keyboard 类型：从心跳槽位中查找 */
    {
        uint8_t kbd_type = 0;
        for (i = 0; i < HB_MAX_SLOTS; i++) {
            if (hardware_g.hb_slots[i].module_id == MODULE_ID_KEYBOARD &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE) {
                kbd_type = hardware_g.hb_slots[i].subtype;
                break;
            }
        }
        status_buf[offset++] = kbd_type;
    }

    /* CH9350 连接设备类型 */
    status_buf[offset++] = ch9350_g.connected_dev_type;

    /* 音频曲目名称 (长度前缀) */
    {
        const char *track = Audio_GetCurrentTrackName();
        uint8_t tlen = 0;
        while (track[tlen] && tlen < 31) tlen++;
        status_buf[offset++] = tlen;
        if (tlen > 0) {
            memcpy(&status_buf[offset], track, tlen);
            offset += tlen;
        }
    }

    /* 键盘按键统计 (uint32 大端) */
    {
        uint32_t key_count = keyboard_g.type_id; /* 暂用 type_id 占位，后续扩展 */
        status_buf[offset++] = (uint8_t)(key_count >> 24);
        status_buf[offset++] = (uint8_t)(key_count >> 16);
        status_buf[offset++] = (uint8_t)(key_count >> 8);
        status_buf[offset++] = (uint8_t)(key_count & 0xFF);
    }

    /* 各模块条目 */
    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        const hb_slot_t *slot = &hardware_g.hb_slots[i];
        status_buf[offset++] = slot->module_id;
        status_buf[offset++] = slot->type;
        status_buf[offset++] = slot->subtype;
        status_buf[offset++] = (uint8_t)slot->status;
        status_buf[offset++] = slot->miss_count;
    }

    return subdisp_send_multiframe(submodel, SUBDISP_SUBCMD_SYS_STATUS,
                                   status_buf, offset,
                                   NULL, 0,
                                   CMD_SUB_DATA_REPORT);
}

uint8_t Submodels_SubDisp_SetDisplayMode(submodels_t *submodel, uint8_t mode)
{
    /* DATA: [SUB=0x04][mode:1] */
    uint8_t data[2] = { SUBDISP_SUBCMD_SET_DISPLAY_MODE, mode };
    return Submodels_SendCommand(submodel, CMD_SUB_SET_MODE, data, 2);
}
