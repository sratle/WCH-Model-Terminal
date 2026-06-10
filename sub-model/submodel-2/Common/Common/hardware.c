#include "hardware.h"
#include "Uart/uart_core.h"
#include "Protocol/protocol.h"
#include "Max30102/max30102.h"

/* g_max30102_int_flag is defined in max30102.c */

/* Our own module ID, learned from the first received frame's DST field.
 * Can be plugged into slot 1 (0x40), slot 2 (0x41), or slot 3 (0x42).
 * Initially MODULE_ID_SUBMODEL_1 (0x40), updated on first frame reception. */
uint8_t g_my_module_id = MODULE_ID_SUBMODEL_1;

void Hardware_Init(void)
{
    UartCore_Init();
    Max30102_Init();

    Delay_Ms(50);

    /* Start in IDLE state; PollFIFO will detect finger placement
     * and auto-start monitoring when IR signal is high enough. */
}

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_SUBMODEL;
    data[1] = MODULE_SUBTYPE_HEALTH;
    data[2] = 0x01;  /* hw_ver */
    data[3] = 0x01;  /* fw_major */
    data[4] = 0x00;  /* fw_minor */

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
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
        case HEALTH_SUB_SET_INTERVAL:
        {
            if (frame->len >= 4)
            {
                uint16_t interval = ((uint16_t)frame->data[1] << 8) | frame->data[2];
                Max30102_SetInterval(interval);
            }
            break;
        }

        case HEALTH_SUB_START_MONITOR:
        {
            Max30102_StartMonitoring();
            break;
        }

        case HEALTH_SUB_STOP_MONITOR:
        {
            Max30102_StopMonitoring();
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
        case HEALTH_SUB_QUERY_DATA:
        {
            /* ACK + [心跳:1][血氧:1][HRV:2(uint16大端)] */
            uint8_t data[4];
            uint16_t hrv = Max30102_GetHRV();

            data[0] = Max30102_GetHeartRate();
            data[1] = Max30102_GetSpO2();
            data[2] = (uint8_t)(hrv >> 8);
            data[3] = (uint8_t)(hrv & 0xFF);

            UartCore_PackAndSend(frame->src, CMD_ACK, data, 4);
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

    if (frame->dst < MODULE_ID_SUBMODEL_1 || frame->dst > MODULE_ID_SUBMODEL_3)
    {
        Protocol_ResetRxCtx(&uart_core_rx_ctx);
        return;
    }

    /* Learn our module ID from the DST field */
    if (g_my_module_id != frame->dst)
        g_my_module_id = frame->dst;

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

void Hardware_ReportHealthData(void)
{
    /* Format: [子命令:1][心跳:1][血氧:1][HRV:2(BE)] */
    uint8_t data[5];
    uint16_t hrv;

    if (max30102_ctx.state != HEALTH_STATE_MONITORING)
        return;

    hrv = Max30102_GetHRV();

    data[0] = HEALTH_SUB_DATA_REPORT;
    data[1] = Max30102_GetHeartRate();
    data[2] = Max30102_GetSpO2();
    data[3] = (uint8_t)(hrv >> 8);
    data[4] = (uint8_t)(hrv & 0xFF);

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_DATA_REPORT, data, 5);
}
