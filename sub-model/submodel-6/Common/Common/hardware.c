#include "hardware.h"
#include "Uart/uart_core.h"
#include "Protocol/protocol.h"
#include "VL53L0X/vl53l0x.h"
#include <stdio.h>

/* Module ID: initially 0x00 (unknown), learned from GET_TYPE DST field */
uint8_t g_my_module_id = 0x00;

void Hardware_Init(void)
{
    UartCore_Init();
    VL53L0X_Init();
}

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_SUBMODEL;
    data[1] = MODULE_SUBTYPE_IR;
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

    data[0] = IR_SUB_RESULT_OK;
    data[1] = (uint8_t)(distance_mm >> 8);
    data[2] = (uint8_t)(distance_mm & 0xFF);

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_ACTION_RESULT, data, 3);
}

static void SendRangingError(uint8_t err_code)
{
    uint8_t data[2];

    data[0] = IR_SUB_RESULT_FAIL;
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
        case IR_SUB_START_RANGING:
        {
            printf("[HW] SET_MODE: start ranging (init=%d)\r\n", VL53L0X_IsInitialized());
            if (!VL53L0X_IsInitialized())
            {
                SendRangingError(IR_ERR_NOT_INIT);
                return;
            }
            VL53L0X_StartContinuous();
            break;
        }

        case IR_SUB_STOP_RANGING:
        {
            printf("[HW] SET_MODE: stop ranging\r\n");
            VL53L0X_StopContinuous();
            break;
        }

        default:
        {
            printf("[HW] SET_MODE: unknown subcmd 0x%02X\r\n", frame->data[0]);
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

            /* If ranging is active, send distance result right after ACK */
            if (vl53l0x_ctx.state == VL53L0X_STATE_RANGING)
            {
                if (VL53L0X_IsDataReady())
                {
                    uint16_t dist = VL53L0X_ReadDistance();
                    if (dist == 0xFFFF)
                        SendRangingError(IR_ERR_OUT_OF_RANGE);
                    else
                        SendRangingResult(dist);
                }
            }
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
