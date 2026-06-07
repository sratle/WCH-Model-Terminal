#include "hardware.h"
#include "Uart/uart_core.h"
#include "Uart/uart_fp.h"
#include "Protocol/protocol.h"
#include "Fingerprint/fingerprint.h"

volatile uint8_t g_touchout_flag = 0;

static void TouchOut_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    gpio.GPIO_Pin  = TOUCHOUT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(TOUCHOUT_PORT, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource4);

    exti.EXTI_Line    = TOUCHOUT_EXTI_LINE;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = TOUCHOUT_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void Hardware_Init(void)
{
    UartCore_Init();
    UartFp_Init();
    Fp_Init();

    Delay_Ms(80);

    TouchOut_GPIO_Init();

    Fp_HandShake();
    Delay_Ms(200);

    Fp_ReadValidTemplateNum();
}

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_SUBMODEL;
    data[1] = MODULE_SUBTYPE_FP;
    data[2] = 0x01;
    data[3] = 0x01;
    data[4] = 0x00;

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
}

void Hardware_SendIdentifyResult(uint8_t success, uint16_t page_id)
{
    uint8_t data[18];

    if (success)
    {
        data[0] = FP_SUB_IDENTIFY_OK;
        data[1] = (uint8_t)page_id;
        memset(&data[2], 0, 16);
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_EVT_NOTIFY, data, 18);
    }
    else
    {
        data[0] = FP_SUB_IDENTIFY_FAIL;
        data[1] = fp_ctx.last_ack;
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_EVT_NOTIFY, data, 2);
    }
}

void Hardware_SendEnrollResult(uint8_t success, uint16_t page_id)
{
    uint8_t data[18];

    if (success)
    {
        data[0] = FP_SUB_ENROLL_OK;
        data[1] = (uint8_t)page_id;
        memset(&data[2], 0, 16);
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_ACTION_RESULT, data, 18);
    }
    else
    {
        data[0] = FP_SUB_ENROLL_FAIL;
        data[1] = fp_ctx.last_ack;
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_ACTION_RESULT, data, 2);
    }
}

void Hardware_SendTemplateCount(uint16_t count)
{
    uint8_t data[1];

    data[0] = (uint8_t)(count & 0xFF);
    UartCore_PackAndSend(MODULE_ID_CORE, CMD_ACK, data, 1);
}

void Hardware_SendIndexList(void)
{
    /* 先收集所有已注册的 page ID */
    uint8_t ids[256];
    uint8_t count = 0;
    uint16_t i;

    for (i = 0; i < 256; i++)
    {
        uint8_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;

        if (byte_idx < 32 && (fp_ctx.index_table[byte_idx] & (1 << bit_idx)))
        {
            ids[count++] = (uint8_t)i;
        }
    }

    /* 发送 ACK 响应: [数量:1][ID0:1][ID1:1]...]
     * 符合 Protocol_Submodels.md 中 QUERY_LIST 的 ACK 格式 */
    {
        uint8_t data[252];
        uint8_t send_count;
        uint8_t j;

        send_count = count;
        if (send_count > 251)
            send_count = 251;

        data[0] = send_count;
        for (j = 0; j < send_count; j++)
            data[1 + j] = ids[j];

        UartCore_PackAndSend(MODULE_ID_CORE, CMD_ACK, data, 1 + send_count);
    }
}

static void HandleSubSetMode(const protocol_frame_t *frame)
{
    if (frame->len < 2)
    {
        SendNACK(frame, PROTO_ERR_LEN_MISMATCH);
        return;
    }

    switch (frame->data[0])
    {
        case FP_SUB_ENROLL_START:
        {
            uint16_t page_id;

            if (frame->len >= 3)
            {
                page_id = frame->data[1];
            }
            else
            {
                /* Syno 协议不支持自动分配 page_id，
                 * 用当前模板数量作为下一个空闲位置 */
                page_id = fp_ctx.template_count;
            }

            Fp_AutoEnroll(page_id, 5);
            break;
        }

        case FP_SUB_ENROLL_CANCEL:
        {
            Fp_Cancel();
            break;
        }

        case FP_SUB_SET_LED:
        {
            if (frame->len >= 5)
            {
                uint8_t func  = frame->data[1];
                uint8_t color = frame->data[2];
                uint8_t speed = frame->data[3];
                Fp_ControlLED(func, color, speed);
            }
            break;
        }

        case FP_SUB_SET_SECURITY:
        {
            if (frame->len >= 3)
            {
                uint8_t level = frame->data[1];
                Fp_WriteReg(0x05, (uint16_t)level);
            }
            break;
        }

        default:
        {
            SendNACK(frame, PROTO_ERR_INVALID_PARAM);
            break;
        }
    }
}

static void HandleSubSetConfig(const protocol_frame_t *frame)
{
    if (frame->len < 2)
    {
        SendNACK(frame, PROTO_ERR_LEN_MISMATCH);
        return;
    }

    switch (frame->data[0])
    {
        case FP_SUB_DELETE:
        {
            if (frame->len >= 3)
            {
                uint8_t page_id = frame->data[1];
                Fp_DeleteTemplate(page_id, 1);
            }
            break;
        }

        case FP_SUB_DELETE_ALL:
        {
            Fp_EmptyTemplate();
            break;
        }

        case FP_SUB_SLEEP:
        {
            Fp_Sleep();
            break;
        }

        default:
        {
            SendNACK(frame, PROTO_ERR_INVALID_PARAM);
            break;
        }
    }
}

static void HandleSubGetStatus(const protocol_frame_t *frame)
{
    if (frame->len < 2)
    {
        SendNACK(frame, PROTO_ERR_LEN_MISMATCH);
        return;
    }

    switch (frame->data[0])
    {
        case FP_SUB_QUERY_LIST:
        {
            Fp_ReadIndexTable(0);
            break;
        }

        case FP_SUB_QUERY_COUNT:
        {
            Fp_ReadValidTemplateNum();
            break;
        }

        default:
        {
            SendNACK(frame, PROTO_ERR_INVALID_PARAM);
            break;
        }
    }
}

void Hardware_ProcessCoreFrame(void)
{
    const protocol_frame_t *frame;

    if (!uart_core_rx_ctx.frame_ready)
        return;

    frame = &uart_core_rx_ctx.read_frame;

    if (frame->dst != MODULE_ID_SUBMODEL_1)
    {
        Protocol_ResetRxCtx(&uart_core_rx_ctx);
        return;
    }

    switch (frame->cmd)
    {
        case CMD_GET_TYPE:
        {
            SendGetTypeResponse(frame);
            break;
        }

        case CMD_NOP:
        {
            UartCore_PackAndSend(frame->src, CMD_ACK, NULL, 0);
            break;
        }

        case CMD_SUB_SET_MODE:
        {
            HandleSubSetMode(frame);
            break;
        }

        case CMD_SUB_SET_CONFIG:
        {
            HandleSubSetConfig(frame);
            break;
        }

        case CMD_SUB_GET_STATUS:
        {
            HandleSubGetStatus(frame);
            break;
        }

        default:
        {
            SendNACK(frame, PROTO_ERR_UNSUPPORTED_CMD);
            break;
        }
    }

    Protocol_ResetRxCtx(&uart_core_rx_ctx);
}

void Hardware_ProcessFpResponse(void)
{
    if (!Fp_IsFrameReady())
        return;

    Fp_HandleResponse();

    switch (fp_ctx.last_cmd)
    {
        case SYNO_CMD_AUTO_MATCH:
        {
            if (fp_ctx.last_ack == SYNO_ACK_OK)
            {
                Hardware_SendIdentifyResult(1, fp_ctx.last_page_id);
            }
            else
            {
                Hardware_SendIdentifyResult(0, 0);
            }
            break;
        }

        case SYNO_CMD_AUTO_ENROLL:
        {
            /* 每个应答都发送进度事件给 Core */
            if (fp_ctx.enroll_progress_ready)
            {
                uint8_t data[4];
                data[0] = FP_SUB_ENROLL_PROGRESS;
                data[1] = fp_ctx.enroll_step;      /* 步骤类型 (param1) */
                data[2] = fp_ctx.enroll_sub_step;   /* 子步骤 (param2) */
                data[3] = fp_ctx.last_ack;          /* 确认码 */
                UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_EVT_NOTIFY, data, 4);
                fp_ctx.enroll_progress_ready = 0;
            }

            /* 终止性错误：立即发送失败结果 */
            if (fp_ctx.state == FP_STATE_IDLE && fp_ctx.last_ack != SYNO_ACK_OK)
            {
                Hardware_SendEnrollResult(0, fp_ctx.last_ack);
            }
            break;
        }

        case SYNO_CMD_VALID_TEMPLATE_NUM:
        {
            if (fp_ctx.last_ack == SYNO_ACK_OK)
            {
                Hardware_SendTemplateCount(fp_ctx.template_count);
            }
            break;
        }

        case SYNO_CMD_READ_INDEX_TABLE:
        {
            if (fp_ctx.index_ready)
            {
                Hardware_SendIndexList();
                fp_ctx.index_ready = 0;
            }
            break;
        }

        case SYNO_CMD_INTO_SLEEP:
        {
            if (fp_ctx.led_cmd_cached)
            {
                fp_ctx.led_cmd_cached = 0;
                Fp_ControlLED(fp_ctx.led_cached_func,
                              fp_ctx.led_cached_color,
                              fp_ctx.led_cached_speed);
            }
            break;
        }

        default:
            break;
    }
}
