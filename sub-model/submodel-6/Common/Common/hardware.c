#include "hardware.h"
#include "Uart/uart_core.h"
#include "Protocol/protocol.h"
#include "VL53L0X/vl53l0x.h"
#include "Filter/filter.h"

/* Module ID: initially 0x00 (unknown), learned from GET_TYPE DST field */
uint8_t g_my_module_id = 0x00;

/* ---- Reporting states ----
 * STANDBY: sensor ranges continuously, report averaged distance every 1s.
 * RANGING: entered on Core SET_MODE SUB=0x01, report every 50ms. */
typedef enum {
    LR_REPORT_STANDBY = 0,
    LR_REPORT_RANGING
} lr_report_state_t;

#define LR_REPORT_PERIOD_STANDBY_MS    1000
#define LR_REPORT_PERIOD_RANGING_MS    50

static lr_report_state_t s_report_state = LR_REPORT_STANDBY;
static uint32_t          s_last_report_ms = 0;
static filter_avg_t      s_dist_filter;

void Hardware_Init(void)
{
    UartCore_Init();

    Filter_Init(&s_dist_filter);
    s_report_state = LR_REPORT_STANDBY;
    s_last_report_ms = 0;

    VL53L0X_Init();

    /* Start continuous ranging right away: standby state reports
     * averaged distance every 1s without waiting for a Core command. */
    if (VL53L0X_IsInitialized())
        VL53L0X_StartContinuous();
}

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_SUBMODEL;
    data[1] = MODULE_SUBTYPE_LASER;
    data[2] = 0x01;  /* hw_ver */
    data[3] = 0x01;  /* fw_major */
    data[4] = 0x00;  /* fw_minor */

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
}

static void SendRangingResult(uint16_t distance_mm)
{
    uint8_t data[3];

    data[0] = LR_SUB_RESULT_OK;
    data[1] = (uint8_t)(distance_mm >> 8);
    data[2] = (uint8_t)(distance_mm & 0xFF);

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_ACTION_RESULT, data, 3);
}

static void SendRangingError(uint8_t err_code)
{
    uint8_t data[2];

    data[0] = LR_SUB_RESULT_FAIL;
    data[1] = err_code;

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_ACTION_RESULT, data, 2);
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
        case LR_SUB_START_RANGING:
        {
            if (!VL53L0X_IsInitialized())
            {
                SendRangingError(LR_ERR_NOT_INIT);
                return;
            }
            s_report_state = LR_REPORT_RANGING;
            break;
        }

        case LR_SUB_STOP_RANGING:
        {
            /* Back to standby: keep ranging, report once per second */
            s_report_state = LR_REPORT_STANDBY;
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

        case CMD_SUB_SET_MODE:
        {
            HandleSubSetMode(frame);
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

/* Called from main loop with the current millisecond tick.
 * Samples the sensor as fast as data becomes ready and pushes valid
 * readings into the moving average filter; reports the averaged
 * distance to Core at the state-dependent period. */
void Hardware_ProcessRanging(uint32_t now_ms)
{
    uint32_t period;

    if (!VL53L0X_IsInitialized())
        return;

    /* Drain all pending samples (continuous mode, ~33ms timing budget) */
    while (VL53L0X_IsDataReady())
    {
        uint16_t d = VL53L0X_ReadDistance();
        if (d != VL53L0X_DISTANCE_INVALID)
            Filter_Push(&s_dist_filter, d);
    }

    period = (s_report_state == LR_REPORT_RANGING)
                 ? LR_REPORT_PERIOD_RANGING_MS
                 : LR_REPORT_PERIOD_STANDBY_MS;

    if (now_ms - s_last_report_ms >= period)
    {
        s_last_report_ms = now_ms;

        if (Filter_Count(&s_dist_filter) > 0)
            SendRangingResult(Filter_Average(&s_dist_filter));
        else
            SendRangingError(LR_ERR_OUT_OF_RANGE);
    }
}
