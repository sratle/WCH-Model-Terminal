/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Keyboard-3 (Music Keyboard) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      Dual TTP229 for 24 touch keys, 12 GPIO buttons, 3 ADC faders.
 *                      TIM2 drives periodic scan, main loop handles events.
 *********************************************************************************/

#include "debug.h"
#include "./TTP229/ttp229.h"
#include "./Button/button.h"
#include "./Fader/fader.h"
#include "./Uart/uart_core.h"
#include "./Protocol/protocol.h"
#include <string.h>

/* ======================== Global State ======================== */

volatile uint8_t g_scan_flag = 0;

static uint8_t  event_reporting = 0;    /* 0 = standby, 1 = event-driven reporting */

/* ======================== Cached Input State ======================== */

static uint8_t  key_bitmap[TTP_BITS_BYTES];
static uint8_t  btn_bitmap[BUTTON_BITS_BYTES];
static uint16_t fader_values[FADER_COUNT];

/* Previous state for change detection */
static uint8_t  prev_key_bitmap[TTP_BITS_BYTES];
static uint8_t  prev_btn_bitmap[BUTTON_BITS_BYTES];
static uint16_t prev_fader_values[FADER_COUNT];

/* ======================== Protocol Handlers ======================== */

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_KEYBOARD;       /* 0x04 */
    data[1] = MODULE_SUBTYPE_KBD_MUSIC;   /* 0x03 */
    data[2] = 0x01;                        /* HW version */
    data[3] = 0x01;                        /* FW major */
    data[4] = 0x00;                        /* FW minor */

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
}

static void SendKeysResponse(const protocol_frame_t *req)
{
    UartCore_PackAndSend(req->src, CMD_ACK, key_bitmap, TTP_BITS_BYTES);
}

static void SendButtonsResponse(const protocol_frame_t *req)
{
    UartCore_PackAndSend(req->src, CMD_ACK, btn_bitmap, BUTTON_BITS_BYTES);
}

static void SendFadersResponse(const protocol_frame_t *req)
{
    uint8_t data[6];

    /* Big-endian encoding for 3 x uint16 */
    data[0] = (uint8_t)(fader_values[0] >> 8);
    data[1] = (uint8_t)(fader_values[0] & 0xFF);
    data[2] = (uint8_t)(fader_values[1] >> 8);
    data[3] = (uint8_t)(fader_values[1] & 0xFF);
    data[4] = (uint8_t)(fader_values[2] >> 8);
    data[5] = (uint8_t)(fader_values[2] & 0xFF);

    UartCore_PackAndSend(req->src, CMD_ACK, data, 6);
}

static void SendEventCtrlStatus(const protocol_frame_t *req)
{
    UartCore_PackAndSend(req->src, CMD_ACK, &event_reporting, 1);
}

static void ProcessCoreFrame(const protocol_frame_t *frame)
{
    if (frame->dst != MODULE_ID_KEYBOARD)
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

        case CMD_KBD_GET_STATUS:
        {
            if (frame->len >= 2)
            {
                uint8_t status_type = frame->data[0];
                switch (status_type)
                {
                    case 0x00: SendKeysResponse(frame);    break;
                    case 0x01: SendButtonsResponse(frame); break;
                    case 0x02: SendFadersResponse(frame);  break;
                    case 0x05: SendEventCtrlStatus(frame); break;
                    default:   SendNACK(frame, PROTO_ERR_INVALID_PARAM); break;
                }
            }
            else
            {
                SendNACK(frame, PROTO_ERR_LEN_MISMATCH);
            }
            break;
        }

        case CMD_KBD_MUSIC_EVENT_CTRL:
        {
            if (frame->len >= 2)
            {
                uint8_t state = frame->data[0];
                if (state == 0x00 || state == 0x01)
                {
                    event_reporting = state;
                    if (state == 0x01)
                    {
                        /* Reset previous state so first scan triggers a report */
                        memset(prev_key_bitmap, 0xFF, sizeof(prev_key_bitmap));
                        memset(prev_btn_bitmap, 0xFF, sizeof(prev_btn_bitmap));
                        memset(prev_fader_values, 0xFF, sizeof(prev_fader_values));
                    }
                }
            }
            /* Fire-and-forget: no ACK/NACK */
            break;
        }

        case CMD_KBD_SET_BACKLIGHT:
        case CMD_KBD_GET_BACKLIGHT:
        case CMD_KBD_SET_CONFIG:
        {
            /* These commands are for Keyboard-1/2, silently ignore on Music Keyboard */
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

/* ======================== Event-Driven Reporting ======================== */

static uint8_t HasStateChanged(void)
{
    uint8_t i;

    /* Check keys */
    if (memcmp(key_bitmap, prev_key_bitmap, TTP_BITS_BYTES) != 0)
        return 1;

    /* Check buttons */
    if (memcmp(btn_bitmap, prev_btn_bitmap, BUTTON_BITS_BYTES) != 0)
        return 1;

    /* Check faders (with threshold to ignore noise) */
    for (i = 0; i < FADER_COUNT; i++)
    {
        int32_t diff = (int32_t)fader_values[i] - (int32_t)prev_fader_values[i];
        if (diff > 512 || diff < -512)  /* ~0.8% threshold on 16-bit range */
            return 1;
    }

    return 0;
}

static void SavePrevState(void)
{
    memcpy(prev_key_bitmap, key_bitmap, TTP_BITS_BYTES);
    memcpy(prev_btn_bitmap, btn_bitmap, BUTTON_BITS_BYTES);
    memcpy(prev_fader_values, fader_values, sizeof(fader_values));
}

static void ReportChangedEvents(void)
{
    uint8_t data[6];

    /* Report key bitmap if changed */
    if (memcmp(key_bitmap, prev_key_bitmap, TTP_BITS_BYTES) != 0)
    {
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_KBD_MUSIC_KEYS,
                             key_bitmap, TTP_BITS_BYTES);
    }

    /* Report button bitmap if changed */
    if (memcmp(btn_bitmap, prev_btn_bitmap, BUTTON_BITS_BYTES) != 0)
    {
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_KBD_MUSIC_BUTTONS,
                             btn_bitmap, BUTTON_BITS_BYTES);
    }

    /* Report fader values if changed (with threshold) */
    {
        uint8_t i;
        uint8_t fader_changed = 0;
        for (i = 0; i < FADER_COUNT; i++)
        {
            int32_t diff = (int32_t)fader_values[i] - (int32_t)prev_fader_values[i];
            if (diff > 512 || diff < -512)
            {
                fader_changed = 1;
                break;
            }
        }
        if (fader_changed)
        {
            data[0] = (uint8_t)(fader_values[0] >> 8);
            data[1] = (uint8_t)(fader_values[0] & 0xFF);
            data[2] = (uint8_t)(fader_values[1] >> 8);
            data[3] = (uint8_t)(fader_values[1] & 0xFF);
            data[4] = (uint8_t)(fader_values[2] >> 8);
            data[5] = (uint8_t)(fader_values[2] & 0xFF);
            UartCore_PackAndSend(MODULE_ID_CORE, CMD_KBD_MUSIC_FADERS, data, 6);
        }
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

    /* Initialize all peripherals */
    TTP229_Init();
    Button_Init();
    Fader_Init();
    UartCore_Init();
    Timer2_Init();

    /* Wait for TTP229 power-on stabilization (~500ms) */
    Delay_Ms(500);

    /* Calibrate TTP229 baseline (capture idle state, cancel parasitic bits) */
    TTP229_Calibrate();

    while (1)
    {
        /* ALWAYS check protocol - never blocked by scanning */
        CheckProtocolRx();

        if (g_scan_flag)
        {
            g_scan_flag = 0;

            /* Scan all inputs */
            TTP229_Read(key_bitmap);
            Button_Scan();
            Fader_Scan();

            /* Copy fader output values */
            Fader_GetValues(fader_values);

            /* Copy button bitmap */
            Button_GetBitmap(btn_bitmap);

            /* Event-driven reporting: only send when state changes */
            if (event_reporting && HasStateChanged())
            {
                ReportChangedEvents();
                SavePrevState();
            }
        }
    }
}
