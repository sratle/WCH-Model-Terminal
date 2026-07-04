#include "hardware.h"
#include "Uart/uart_core.h"
#include "Protocol/protocol.h"
#include "Nfc/nfc.h"

/* Our own module ID, learned from the first received frame's DST field.
 * Can be plugged into slot 1 (0x40), slot 2 (0x41), or slot 3 (0x42).
 * Initially MODULE_ID_SUBMODEL_1 (0x40), updated on first frame reception. */
uint8_t g_my_module_id = MODULE_ID_SUBMODEL_1;

void Hardware_Init(void)
{
    UartCore_Init();
    Nfc_Init();

    /* Boot debug banner */
    {
        const char *msg = "\r\n[SUB3-NFC] boot ok  UART1=230400  UART2=9600  passthrough\r\n";
        const uint8_t *p = (const uint8_t *)msg;
        uint16_t i = 0;
        while (p[i]) i++;
        UartCore_SendData(p, i);
    }
}

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_SUBMODEL;
    data[1] = MODULE_SUBTYPE_NFC;
    data[2] = 0x01;   /* HW version */
    data[3] = 0x01;   /* FW major */
    data[4] = 0x00;   /* FW minor */

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
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
        case NFC_SUB_QUERY_STATUS:
        {
            /* Return current card state:
             * [card_id:1][card_number:5]
             * card_id=0 if no card */
            uint8_t data[6];

            data[0] = nfc_ctx.reported_card_id;
            memcpy(&data[1], nfc_ctx.reported_card_number, 5);
            UartCore_PackAndSend(frame->src, CMD_ACK, data, 6);
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

    /* Learn our module ID from the DST field of the first valid frame */
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

void Hardware_ProcessNfcCard(void)
{
    if (!nfc_ctx.card_ready)
        return;

    nfc_ctx.card_ready = 0;

    /* Send card detection event to Core:
     * CMD = 0x40 (CMD_SUB_EVT_NOTIFY)
     * DATA: [sub_cmd=0x01][card_id:1][card_number:5] */
    {
        uint8_t data[7];

        data[0] = NFC_SUB_CARD_DETECT;
        data[1] = nfc_ctx.reported_card_id;
        memcpy(&data[2], nfc_ctx.reported_card_number, 5);

        UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_EVT_NOTIFY, data, 7);
    }
}
