/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_module.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/05/22
* Description        : UART1 module communication with Core.
*                      USART1 (PA9-TX / PA10-RX, AF7) initialization,
*                      interrupt-driven RX with protocol frame parsing,
*                      input event dispatch to ui_input_feed_*().
********************************************************************************/
#include "uart_module.h"
#include "../MiniUI/miniui.h"
#include "ch32h417.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  USART1 Configuration
 *=============================================================================*/

#define USART1_BAUDRATE     921600

/* GPIO pins for USART1 */
#define USART1_TX_PIN       GPIO_Pin_9
#define USART1_TX_PORT      GPIOA
#define USART1_RX_PIN       GPIO_Pin_10
#define USART1_RX_PORT      GPIOA
#define USART1_GPIO_AF      GPIO_AF7

/*=============================================================================
 *  RX State Machine
 *=============================================================================*/

typedef enum {
    RX_STATE_WAIT_HEAD = 0,
    RX_STATE_SRC,
    RX_STATE_DST,
    RX_STATE_LEN,
    RX_STATE_CMD,
    RX_STATE_DATA,
    RX_STATE_TAIL0,
    RX_STATE_TAIL1,
    RX_STATE_TAIL2,
    RX_STATE_TAIL3,
} rx_state_t;

static volatile rx_state_t s_rx_state = RX_STATE_WAIT_HEAD;
static volatile uint8_t s_rx_frame_src;
static volatile uint8_t s_rx_frame_dst;
static volatile uint8_t s_rx_frame_len;
static volatile uint8_t s_rx_frame_cmd;
static volatile uint8_t s_rx_data_buf[PROTO_MAX_DATA_LEN];
static volatile uint8_t s_rx_data_idx;
static volatile uint8_t s_rx_data_remaining;

/* Simple ring buffer for ISR → poll communication */
static volatile uint8_t s_rx_ring[UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_ring_head = 0;
static volatile uint16_t s_rx_ring_tail = 0;

/*=============================================================================
 *  USART1 Initialization
 *=============================================================================*/

void UART_Module_Init(void)
{
    printf("[UART_Module_Init] start\r\n");

    /* Enable GPIOA, AFIO and USART1 clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA | RCC_HB2Periph_USART1, ENABLE);

    /* Configure PA9 (TX) as AF7 push-pull */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, USART1_GPIO_AF);
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Pin = USART1_TX_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(USART1_TX_PORT, &GPIO_InitStructure);

    /* Configure PA10 (RX) as AF7 push-pull (full-duplex requires AF for RX too) */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, USART1_GPIO_AF);
    GPIO_InitStructure.GPIO_Pin = USART1_RX_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(USART1_RX_PORT, &GPIO_InitStructure);

    /* USART1 configuration */
    USART_InitTypeDef USART_InitStructure = {0};
    USART_InitStructure.USART_BaudRate = USART1_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    /* Enable RXNE interrupt */
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    /* Configure NVIC for USART1 */
    NVIC_SetPriority(USART1_IRQn, 1 << 4);  /* Priority level 1 */
    NVIC_EnableIRQ(USART1_IRQn);

    /* Enable USART1 */
    USART_Cmd(USART1, ENABLE);

    /* Reset state machine */
    s_rx_state = RX_STATE_WAIT_HEAD;
    s_rx_ring_head = 0;
    s_rx_ring_tail = 0;

    printf("[UART_Module_Init] done (921600 8-N-1)\r\n");
}

/*=============================================================================
 *  USART1 IRQ Handler — RX byte-by-byte state machine
 *=============================================================================*/

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == RESET)
        return;

    uint8_t byte = (uint8_t)USART_ReceiveData(USART1);

    switch (s_rx_state) {
        case RX_STATE_WAIT_HEAD:
            if (byte == PROTO_FRAME_HEAD) {
                s_rx_state = RX_STATE_SRC;
            }
            break;

        case RX_STATE_SRC:
            s_rx_frame_src = byte;
            s_rx_state = RX_STATE_DST;
            break;

        case RX_STATE_DST:
            s_rx_frame_dst = byte;
            s_rx_state = RX_STATE_LEN;
            break;

        case RX_STATE_LEN:
            s_rx_frame_len = byte;
            s_rx_state = RX_STATE_CMD;
            break;

        case RX_STATE_CMD:
            s_rx_frame_cmd = byte;
            s_rx_data_idx = 0;
            if (s_rx_frame_len > 1) {
                s_rx_data_remaining = s_rx_frame_len - 1;
                s_rx_state = RX_STATE_DATA;
            } else {
                s_rx_data_remaining = 0;
                s_rx_state = RX_STATE_TAIL0;
            }
            break;

        case RX_STATE_DATA:
            s_rx_data_buf[s_rx_data_idx++] = byte;
            s_rx_data_remaining--;
            if (s_rx_data_remaining == 0) {
                s_rx_state = RX_STATE_TAIL0;
            }
            break;

        case RX_STATE_TAIL0:
            if (byte == PROTO_FRAME_TAIL0) {
                s_rx_state = RX_STATE_TAIL1;
            } else {
                s_rx_state = RX_STATE_WAIT_HEAD;
            }
            break;

        case RX_STATE_TAIL1:
            if (byte == PROTO_FRAME_TAIL1) {
                s_rx_state = RX_STATE_TAIL2;
            } else {
                s_rx_state = RX_STATE_WAIT_HEAD;
            }
            break;

        case RX_STATE_TAIL2:
            if (byte == PROTO_FRAME_TAIL2) {
                s_rx_state = RX_STATE_TAIL3;
            } else {
                s_rx_state = RX_STATE_WAIT_HEAD;
            }
            break;

        case RX_STATE_TAIL3:
            if (byte == PROTO_FRAME_TAIL3) {
                /* Frame complete — push to ring buffer for poll processing */
                /* Encode: [src][dst][cmd][data_len][data0..N] */
                uint16_t total = 4 + s_rx_data_idx;
                if (total + s_rx_ring_head <= UART_RX_BUF_SIZE) {
                    s_rx_ring[(s_rx_ring_head++) % UART_RX_BUF_SIZE] = s_rx_frame_src;
                    s_rx_ring[(s_rx_ring_head++) % UART_RX_BUF_SIZE] = s_rx_frame_dst;
                    s_rx_ring[(s_rx_ring_head++) % UART_RX_BUF_SIZE] = s_rx_frame_cmd;
                    s_rx_ring[(s_rx_ring_head++) % UART_RX_BUF_SIZE] = s_rx_data_idx;
                    for (uint8_t i = 0; i < s_rx_data_idx; i++) {
                        s_rx_ring[(s_rx_ring_head++) % UART_RX_BUF_SIZE] = s_rx_data_buf[i];
                    }
                }
            }
            s_rx_state = RX_STATE_WAIT_HEAD;
            break;
    }
}

/*=============================================================================
 *  Frame Processing — called from poll
 *=============================================================================*/

static void process_input_event(const uint8_t *data, uint8_t data_len)
{
    /* DATA[0] = device type
     * DATA[1..N] = report data */
    if (data_len < 2) return;

    uint8_t dev_type = data[0];

    switch (dev_type) {
        case INPUT_DEV_KEYBOARD: {
            /* Keyboard report: modifiers(1) + reserved(1) + key_codes[6] = 8 bytes */
            if (data_len < 9) return;
            uint8_t modifiers = data[1];
            /* data[2] is reserved */
            uint8_t key_codes[6];
            memcpy(key_codes, &data[3], 6);
            ui_input_feed_keyboard(modifiers, key_codes);
            break;
        }

        case INPUT_DEV_MOUSE: {
            /* Mouse report: buttons(1) + dx(1) + dy(1) + scroll(1) = 4 bytes */
            if (data_len < 5) return;
            uint8_t buttons = data[1];
            int8_t dx = (int8_t)data[2];
            int8_t dy = (int8_t)data[3];
            int8_t scroll = (int8_t)data[4];
            ui_input_feed_mouse(dx, dy, buttons, scroll);
            break;
        }

        case INPUT_DEV_TOUCH: {
            /* Touch report: touch_id(1) + event(1) + x(2 BE) + y(2 BE) + pressure(1) = 7 bytes */
            if (data_len < 8) return;
            uint8_t touch_id = data[1];
            uint8_t event = data[2];
            int16_t x = (int16_t)((uint16_t)data[3] << 8 | data[4]);
            int16_t y = (int16_t)((uint16_t)data[5] << 8 | data[6]);
            /* data[7] = pressure, unused for now */

            switch (event) {
                case 0x00: /* Press */
                    ui_input_feed_touch(touch_id, true, x, y);
                    break;
                case 0x01: /* Move */
                    ui_input_feed_touch(touch_id, true, x, y);
                    break;
                case 0x02: /* Release */
                    ui_input_feed_touch(touch_id, false, x, y);
                    break;
                case 0x03: /* Long press — already handled by input system */
                    break;
                case 0x04: /* Double click — already handled by input system */
                    break;
            }
            break;
        }

        case INPUT_DEV_CORE_KEY: {
            /* Core key report: action(1) + key_id(1) = 2 bytes */
            if (data_len < 3) return;
            uint8_t action = data[1];
            uint8_t key_id = data[2];
            ui_input_feed_core_key(key_id, action);
            break;
        }

        default:
            break;
    }
}

static void process_frame(uint8_t src, uint8_t dst, uint8_t cmd,
                          const uint8_t *data, uint8_t data_len)
{
    /* Only process frames addressed to Display module or broadcast */
    if (dst != MODULE_ID_DISPLAY && dst != 0xFF) return;

    switch (cmd) {
        case CMD_DISP_INPUT_EVENT:
            process_input_event(data, data_len);
            break;

        case CMD_DISP_SET_BRIGHTNESS:
            /* TODO: implement brightness control */
            break;

        case CMD_DISP_UPDATE_STATUS:
            /* TODO: implement status update */
            break;

        case CMD_DISP_SHOW_PAGE:
            /* TODO: implement page switch */
            break;

        case CMD_DISP_SCREEN_CONTROL:
            /* TODO: implement screen control */
            break;

        case CMD_DISP_SHOW_NOTICE:
            /* TODO: implement notice popup */
            break;

        case CMD_DISP_MUSIC_STATUS:
            /* TODO: implement music status */
            break;

        case CMD_ACK:
        case CMD_NACK:
            /* TODO: handle ACK/NACK for pending requests */
            break;

        default:
            break;
    }
}

/*=============================================================================
 *  Poll — called from main loop
 *=============================================================================*/

void UART_Module_Poll(void)
{
    /* Process all completed frames in the ring buffer */
    while (s_rx_ring_tail != s_rx_ring_head) {
        uint16_t available = s_rx_ring_head - s_rx_ring_tail;
        if (available < 4) break; /* Need at least src+dst+cmd+data_len */

        uint8_t src = s_rx_ring[s_rx_ring_tail % UART_RX_BUF_SIZE];
        s_rx_ring_tail++;
        uint8_t dst = s_rx_ring[s_rx_ring_tail % UART_RX_BUF_SIZE];
        s_rx_ring_tail++;
        uint8_t cmd = s_rx_ring[s_rx_ring_tail % UART_RX_BUF_SIZE];
        s_rx_ring_tail++;
        uint8_t dlen = s_rx_ring[s_rx_ring_tail % UART_RX_BUF_SIZE];
        s_rx_ring_tail++;

        if (s_rx_ring_head - s_rx_ring_tail < dlen) {
            /* Not enough data yet — rewind and wait */
            s_rx_ring_tail -= 4;
            break;
        }

        uint8_t data[PROTO_MAX_DATA_LEN];
        for (uint8_t i = 0; i < dlen; i++) {
            data[i] = s_rx_ring[s_rx_ring_tail % UART_RX_BUF_SIZE];
            s_rx_ring_tail++;
        }

        process_frame(src, dst, cmd, data, dlen);
    }
}

/*=============================================================================
 *  Frame Transmission
 *=============================================================================*/

void UART_SendFrame(uint8_t dst, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t frame_len = 1 + len; /* CMD + DATA */

    /* Wait for TX ready */
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* HEAD */
    USART_SendData(USART1, PROTO_FRAME_HEAD);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* SRC */
    USART_SendData(USART1, MODULE_ID_DISPLAY);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* DST */
    USART_SendData(USART1, dst);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* LEN */
    USART_SendData(USART1, frame_len);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* CMD */
    USART_SendData(USART1, cmd);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

    /* DATA */
    for (uint8_t i = 0; i < len; i++) {
        USART_SendData(USART1, data[i]);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    }

    /* TAIL */
    USART_SendData(USART1, PROTO_FRAME_TAIL0);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, PROTO_FRAME_TAIL1);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, PROTO_FRAME_TAIL2);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, PROTO_FRAME_TAIL3);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}
