/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Keyboard-1 main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 115200 for Core protocol communication.
 *                      UART2 (PA2/PA3) @ 115200 for CH9329 HID output.
 *                      5x 74HC165 daisy-chain for 39-key matrix reading.
 *                      TIM2 drives periodic scan, main loop handles events.
 *                      Key events are reported to Core via CMD_KBD_HID_REPORT
 *                      and to CH9329 for USB HID output.
 *********************************************************************************/

#include "debug.h"
#include "./74HC165/74HC165.h"
#include "./74HC165/key.h"
#include "./CH9329/ch9329.h"
#include "./Uart/uart_core.h"
#include "./protocol/protocol.h"
#include <string.h>

volatile uint8_t g_scan_flag = 0;

static uint8_t hc165_buf[HC165_CHIP_COUNT];

static uint8_t last_hid_report[CH9329_KB_DATA_LEN] = {0};

static uint8_t  hid_pending = 0;
static uint8_t  hid_report_buf[CH9329_KB_DATA_LEN];

static uint8_t  kbd_backlight_mode = 0x00;
static uint8_t  kbd_backlight_brightness = 0;

static uint8_t BuildHIDReport(uint8_t *report)
{
    uint8_t i, slot;
    uint8_t mod = 0;

    report[0] = 0x00;
    report[1] = 0x00;
    for (i = 2; i < CH9329_KB_DATA_LEN; i++) report[i] = 0x00;

    slot = 2;
    for (i = 0; i < KEY_COUNT; i++)
    {
        if (Key_GetState(i) != KEY_STATE_PRESSED)
            continue;

        if (key_mod_bits[i] != 0)
        {
            mod |= key_mod_bits[i];
        }
        else if (key_hid_codes[i] != 0x00 && slot < CH9329_KB_DATA_LEN)
        {
            report[slot++] = key_hid_codes[i];
        }
    }

    report[0] = mod;
    return (slot - 2);
}

static void SendHIDToCore(const uint8_t *report)
{
    uint8_t data[9];

    data[0] = 0x00;
    memcpy(&data[1], report, CH9329_KB_DATA_LEN);

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_KBD_HID_REPORT,
                         data, 1 + CH9329_KB_DATA_LEN);
}

static void SendHIDIfNeeded(void)
{
    static uint8_t report[CH9329_KB_DATA_LEN];
    static uint8_t ticks_since_send = 0xFF;

    if (ticks_since_send < 4)
    {
        ticks_since_send++;
        return;
    }

    BuildHIDReport(report);

    if (memcmp(report, last_hid_report, CH9329_KB_DATA_LEN) != 0)
    {
        CH9329_SendKeyboard(report);
        SendHIDToCore(report);
        memcpy(last_hid_report, report, CH9329_KB_DATA_LEN);
        memcpy(hid_report_buf, report, CH9329_KB_DATA_LEN);
        ticks_since_send = 0;
        hid_pending = 1;
    }
}

static void FlushEvents(void)
{
    uint8_t i, k;

    for (i = 0; i < key_evt_count; i++)
    {
        printf("[KEY] %-7s: #%02d [%s]\r\n",
               key_evt_buf[i].pressed ? "Press" : "Release",
               key_evt_buf[i].key_idx + 1,
               key_names[key_evt_buf[i].key_idx]);
    }

    if (hid_pending)
    {
        printf("[HID] mod=0x%02X keys=", hid_report_buf[0]);
        for (k = 2; k < CH9329_KB_DATA_LEN; k++)
        {
            if (hid_report_buf[k]) printf("0x%02X ", hid_report_buf[k]);
        }
        printf("\r\n");
        hid_pending = 0;
    }

    key_evt_count = 0;
}

static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_KEYBOARD;
    data[1] = MODULE_SUBTYPE_KBD_MAIN;
    data[2] = 0x01;
    data[3] = 0x01;
    data[4] = 0x00;

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
}

static void SendBacklightResponse(const protocol_frame_t *req)
{
    uint8_t data[2];

    data[0] = kbd_backlight_mode;
    data[1] = kbd_backlight_brightness;

    UartCore_PackAndSend(req->src, CMD_ACK, data, 2);
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
            printf("[PROTO] CMD_GET_TYPE -> ACK\r\n");
            break;
        }

        case CMD_NOP:
        {
            UartCore_PackAndSend(frame->src, CMD_ACK, NULL, 0);
            break;
        }

        case CMD_KBD_SET_BACKLIGHT:
        {
            if (frame->len >= 3)
            {
                kbd_backlight_mode = frame->data[0];
                kbd_backlight_brightness = frame->data[1];
                printf("[PROTO] SET_BACKLIGHT: mode=%d brightness=%d\r\n",
                       kbd_backlight_mode, kbd_backlight_brightness);
            }
            break;
        }

        case CMD_KBD_GET_BACKLIGHT:
        {
            SendBacklightResponse(frame);
            printf("[PROTO] GET_BACKLIGHT -> ACK\r\n");
            break;
        }

        case CMD_KBD_SET_CONFIG:
        {
            printf("[PROTO] SET_CONFIG (fire-and-forget)\r\n");
            break;
        }

        case CMD_KBD_GET_STATUS:
        {
            if (frame->len >= 2)
            {
                printf("[PROTO] GET_STATUS type=%d\r\n", frame->data[0]);
            }
            else
            {
                SendNACK(frame, PROTO_ERR_LEN_MISMATCH);
            }
            break;
        }

        default:
        {
            SendNACK(frame, PROTO_ERR_UNSUPPORTED_CMD);
            printf("[PROTO] Unknown CMD: 0x%02X\r\n", frame->cmd);
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

static void Timer2_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseStructure.TIM_Period = 2000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM_Cmd(TIM2, ENABLE);
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);

    printf("\r\n===== Keyboard-1 Starting =====\r\n");
    printf("SystemClk: %d\r\n", SystemCoreClock);
    printf("ChipID: %08x\r\n", DBGMCU_GetCHIPID());
    printf("Keys: %d, Scan: %dms, Debounce: %dms\r\n",
           KEY_COUNT, KEY_SCAN_PERIOD_MS,
           KEY_SCAN_PERIOD_MS * KEY_DEBOUNCE_CNT);

    HC165_Init();
    Key_Init();
    CH9329_Init();
    UartCore_Init();
    Timer2_Init();

    printf("Init complete. Core UART1 @ 115200, CH9329 UART2 @ 115200\r\n");
    printf("Waiting for key events...\r\n");

    while (1)
    {
        if (g_scan_flag)
        {
            g_scan_flag = 0;

            HC165_Read(hc165_buf);
            Key_Scan(hc165_buf);
            Key_Process();
            SendHIDIfNeeded();
        }
        else
        {
            FlushEvents();
            CheckProtocolRx();
        }
    }
}
