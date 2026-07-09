/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Keyboard-2 (Game Keyboard) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      UART2 (PA2/PA3) @ 115200 for CH9329 HID output.
 *                      Inputs: 6 buttons, 3 toggle switches, 2 joysticks (ADC),
 *                      2 rotary encoders.
 *                      CH9329 outputs keyboard (WASD/HJKL/modifiers) + mouse
 *                      (ROC2/EC1/EC2/buttons). Core receives full GAME_INPUT data.
 *********************************************************************************/

#include "debug.h"
#include "./Gamepad/gamepad.h"
#include "./CH9329/ch9329.h"
#include "./Uart/uart_core.h"
#include "./protocol/protocol.h"
#include <string.h>

volatile uint8_t g_scan_flag = 0;

/* HID report buffers */
static uint8_t kb_report[CH9329_KB_DATA_LEN];
static uint8_t mouse_report[CH9329_MS_DATA_LEN];

/* Last sent HID reports (for change detection) */
static uint8_t last_kb_report[CH9329_KB_DATA_LEN];
static uint8_t last_mouse_report[CH9329_MS_DATA_LEN];

/* Core protocol data buffer */
static uint8_t core_data[GAME_INPUT_DATA_LEN];

/* CH9329 send rate limiting (9600 baud → ~20ms minimum interval for kb+mouse) */
static uint8_t ticks_since_send = 0xFF;

/* --- Send gamepad state to CH9329 (keyboard + mouse) --- */
static void SendHIDIfNeeded(void)
{
    uint8_t need_send = 0;

    if (ticks_since_send < 10)
    {
        ticks_since_send++;
        return;
    }

    Gamepad_BuildKeyboardReport(kb_report);
    Gamepad_BuildMouseReport(mouse_report);

    /* Check if keyboard report changed */
    if (memcmp(kb_report, last_kb_report, CH9329_KB_DATA_LEN) != 0)
        need_send = 1;

    /* Check if mouse report changed */
    if (memcmp(mouse_report, last_mouse_report, CH9329_MS_DATA_LEN) != 0)
        need_send = 1;

    if (need_send)
    {
        CH9329_SendKeyboard(kb_report);
        CH9329_SendMouse(mouse_report);
        memcpy(last_kb_report, kb_report, CH9329_KB_DATA_LEN);
        memcpy(last_mouse_report, mouse_report, CH9329_MS_DATA_LEN);
        ticks_since_send = 0;
    }
}

/* --- Send gamepad state to Core via CMD_KBD_GAME_INPUT --- */
static void SendGameInputToCore(void)
{
    Gamepad_BuildCoreData(core_data);
    UartCore_PackAndSend(MODULE_ID_CORE, CMD_KBD_GAME_INPUT,
                         core_data, GAME_INPUT_DATA_LEN);
}

/* --- CMD_GET_TYPE response --- */
static void SendGetTypeResponse(const protocol_frame_t *req)
{
    uint8_t data[5];

    data[0] = MODULE_TYPE_KEYBOARD;
    data[1] = MODULE_SUBTYPE_KBD_GAME;
    data[2] = 0x01;  /* hw_ver */
    data[3] = 0x01;  /* fw_major */
    data[4] = 0x00;  /* fw_minor */

    UartCore_PackAndSend(req->src, CMD_ACK, data, 5);
}

static void SendNACK(const protocol_frame_t *req, uint8_t err_code)
{
    UartCore_PackAndSend(req->src, CMD_NACK, &err_code, 1);
}

/* --- Process incoming Core protocol frame --- */
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

        case CMD_KBD_SET_BACKLIGHT:
        {
            /* Game keyboard has no backlight; silently accept */
            break;
        }

        case CMD_KBD_GET_BACKLIGHT:
        {
            uint8_t data[2] = {0x00, 0x00};
            UartCore_PackAndSend(frame->src, CMD_ACK, data, 2);
            break;
        }

        case CMD_KBD_SET_CONFIG:
        {
            break;
        }

        case CMD_KBD_GET_STATUS:
        {
            if (frame->len >= 2 && frame->data[0] == 0x03)
            {
                /* Query game input state → return current data */
                Gamepad_BuildCoreData(core_data);
                UartCore_PackAndSend(frame->src, CMD_ACK,
                                     core_data, GAME_INPUT_DATA_LEN);
            }
            else
            {
                SendNACK(frame, PROTO_ERR_UNSUPPORTED_CMD);
            }
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

/* --- Timer2 initialization (2ms scan period) --- */
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

    Gamepad_Init();
    CH9329_Init();
    UartCore_Init();
    Timer2_Init();

    while (1)
    {
        if (g_scan_flag)
        {
            g_scan_flag = 0;

            Gamepad_Scan();
            SendHIDIfNeeded();

            /* Send to Core when state changes */
            if (Gamepad_HasChanged())
            {
                SendGameInputToCore();
                Gamepad_MarkSent();
            }
        }
        else
        {
            CheckProtocolRx();
        }
    }
}
