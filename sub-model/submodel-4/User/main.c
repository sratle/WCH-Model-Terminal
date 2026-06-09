/*
 * Submodel-4 Touch Ring firmware
 *
 * Hardware:
 *   - CH32V103C8T6 @ 72MHz
 *   - TTP229-BSF: PB15-SDO, PB14-SCL (16-key mode)
 *   - UART1 (PA9/PA10) @ 230400 for Core protocol communication
 *
 * Protocol:
 *   - Responds to CMD_GET_TYPE, CMD_NOP, CMD_SUB_GET_STATUS
 *   - Touch event reporting via CMD_SUB_DATA_REPORT on state change
 *   - Recalibration via CMD_SUB_SET_MODE SUB=0x01
 *   - TIM2 drives 10ms periodic scan, main loop handles events
 */

#include "debug.h"
#include "TTP229/ttp229.h"
#include "Uart/uart_core.h"
#include "Protocol/protocol.h"
#include <string.h>

/* ======================== Module Identity ======================== */

#define MY_MODULE_ID    MODULE_ID_SUBMODEL_1  /* 0x40 */
#define HW_VERSION      0x01
#define FW_MAJOR        0x01
#define FW_MINOR        0x00

/* ======================== Global State ======================== */

volatile uint8_t g_scan_flag = 0;

static uint16_t touch_bitmap = 0;     /* current calibrated touch bitmap */
static uint16_t prev_touch_bitmap = 0; /* previous state for change detection */

/* ======================== Protocol Handlers ======================== */

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_SUBMODEL;       /* 0x05 */
    data[1] = MODULE_SUBTYPE_TOUCH_RING;  /* 0x04 */
    data[2] = HW_VERSION;
    data[3] = FW_MAJOR;
    data[4] = FW_MINOR;

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
}

static void SendTouchStatus(const protocol_frame_t *req)
{
    uint8_t data[3];

    data[0] = 0x00;  /* sub-command: query touch state */
    data[1] = (uint8_t)(touch_bitmap >> 8);   /* bitmap high byte */
    data[2] = (uint8_t)(touch_bitmap & 0xFF); /* bitmap low byte */

    UartCore_PackAndSend(req->src, CMD_ACK, data, 3);
}

static void ReportTouchEvent(void)
{
    uint8_t data[3];

    data[0] = 0x01;  /* sub-command: touch event */
    data[1] = (uint8_t)(touch_bitmap >> 8);
    data[2] = (uint8_t)(touch_bitmap & 0xFF);

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_DATA_REPORT, data, 3);
}

static void ProcessCoreFrame(const protocol_frame_t *frame)
{
    if (frame->dst != MY_MODULE_ID)
        return;

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

        case CMD_SUB_GET_STATUS:  /* 0x42 */
        {
            if (frame->len >= 2)
            {
                uint8_t sub = frame->data[0];
                if (sub == 0x00)
                {
                    SendTouchStatus(frame);
                }
                else
                {
                    SendNACK(frame, PROTO_ERR_INVALID_PARAM);
                }
            }
            else
            {
                SendNACK(frame, PROTO_ERR_LEN_MISMATCH);
            }
            break;
        }

        case CMD_SUB_SET_MODE:  /* 0x41 */
        {
            if (frame->len >= 2)
            {
                uint8_t sub = frame->data[0];
                if (sub == 0x01)
                {
                    /* Recalibrate baseline */
                    TTP229_Calibrate();
                }
            }
            /* Fire-and-forget: no ACK/NACK */
            break;
        }

        default:
        {
            SendNACK(frame, PROTO_ERR_UNSUPPORTED_CMD);
            break;
        }
    }
}

static void CheckProtocolRx(void)
{
    if (uart_core_rx_ctx.frame_ready)
    {
        ProcessCoreFrame(&uart_core_rx_ctx.frame);
        Protocol_ResetRxCtx(&uart_core_rx_ctx);
    }
}

/* ======================== Timer ======================== */

static void Timer2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* 72MHz / 72 = 1MHz, period = 10000 -> 10ms interval */
    TIM_TimeBaseStructure.TIM_Period            = 10000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler         = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM_Cmd(TIM2, ENABLE);
}

/* ======================== Main ======================== */

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* Initialize peripherals */
    TTP229_Init();
    UartCore_Init();
    Timer2_Init();

    /* Wait for TTP229 power-on stabilization (~500ms) */
    Delay_Ms(500);

    /* Calibrate TTP229 baseline (capture idle state, cancel parasitic bits) */
    TTP229_Calibrate();

    /* Initialize previous state to suppress false events at startup */
    prev_touch_bitmap = 0;

    while (1)
    {
        /* Always check protocol first */
        CheckProtocolRx();

        if (g_scan_flag)
        {
            g_scan_flag = 0;

            /* Scan touch */
            touch_bitmap = TTP229_Read();

            /* Event-driven reporting: only send on state change */
            if (touch_bitmap != prev_touch_bitmap)
            {
                ReportTouchEvent();
                prev_touch_bitmap = touch_bitmap;
            }
        }
    }
}
