/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_module.c
* Author             : E-ink Model Team
* Version            : V3.0.0
* Date               : 2026/06/12
* Description        : UART1 module communication with Core.
*                      Adapted from Display-1 for CH32V307 + E-ink display.
*                      - CH32V307 USART1 (PA9-TX, PA10-RX) at 921600 baud
*                      - Removed SSD1963 backlight dependency
*                      - Module subtype = 0x02 (E-ink)
*                      - ISR-driven RX state machine (standard + stream frames)
*                      - Command dispatch for basic + extended opcodes
*                      - V3.0: CLI passthrough for file/music/directory ops
********************************************************************************/
#include "uart_module.h"
#include "../MiniUI/miniui.h"
#include "../Eink/epaper.h"
#include "../UI/ui_main.h"
#include "../UI/ui_models.h"
#include "../settings.h"
#include "ch32v30x.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Global Display State
 *=============================================================================*/

uart_disp_state_t g_disp_state;

/* Panel sleep state (e-paper DSLP).  The e-ink panel keeps its image while
 * asleep; the MCU keeps running and wakes the panel on any activity. */
static bool s_panel_asleep = false;

static void panel_sleep(void)
{
    if (s_panel_asleep) return;
    s_panel_asleep = true;
    Epaper_Sleep();
    printf("[panel] sleep (DSLP)\r\n");
}

static void panel_wake(void)
{
    if (!s_panel_asleep) return;
    s_panel_asleep = false;
    Epaper_WakeUp();            /* full re-init (HW reset + LUT) */
    ui_page_invalidate_all();   /* repaint via a full refresh */
    printf("[panel] wake\r\n");
}

/*=============================================================================
 *  App-Layer Callback Storage
 *=============================================================================*/

static uart_cli_cb_t s_cli_cb;
static bool s_cli_cb_valid = false;
volatile pending_req_t g_pending_req = PENDING_NONE;

/* EMusic app callbacks */
static uart_emusic_callbacks_t s_emusic_cb;
static bool s_emusic_cb_valid = false;

/* Submodel event callbacks */
static uart_submodel_cb_t s_submodel_cb;
static bool s_submodel_cb_valid = false;

/*=============================================================================
 *  CLI Response Assembly Engine (V3.1 — unified, single global buffer)
 *
 *  8 KB so that `cat` of a small book/document/image fits in one response;
 *  the EBook/Editor/Images apps parse directly from this buffer (zero-copy).
 *===========================================================================*/

#define CLI_RESP_BUF_SIZE   8192
static char s_cli_resp_buf[CLI_RESP_BUF_SIZE];
static uint16_t s_cli_resp_len = 0;
static bool s_cli_resp_accumulating = false;

#define CLI_CMD_TAG_SIZE   32
static char s_cli_cmd_tag[CLI_CMD_TAG_SIZE];

static bool s_pending_ls_after_cd = false;

static file_entry_t s_parsed_entries[FILE_LIST_MAX_ENTRIES];
static uint8_t s_poll_data[PROTO_MAX_DATA_LEN];

/*=============================================================================
 *  Incremental ls parser
 *
 *  The whole-response buffer above truncates at CLI_RESP_BUF_SIZE, which
 *  made big directory listings (>8KB of ls text) unusable.  The ls parser
 *  therefore works LINE BY LINE as CLI chunks arrive (before any
 *  truncation), so a directory is only limited by the Core-side capture
 *  buffer (32KB ≈ several hundred entries).  Entries land in the shared
 *  s_parsed_entries table and are delivered via on_file_list at EOF.
 *===========================================================================*/

#define LS_LINE_BUF_SIZE    128
static char    s_ls_line[LS_LINE_BUF_SIZE];
static uint8_t s_ls_line_len = 0;
static uint8_t s_ls_count = 0;

static void cli_resp_append(const char *data, uint16_t len);
static void cli_resp_dispatch(void);
static void cli_ls_feed(const char *data, uint16_t len);
static void cli_ls_reset(void);

void UART_SetCLICallbacks(const uart_cli_cb_t *cb)
{
    if (cb) {
        memcpy(&s_cli_cb, cb, sizeof(uart_cli_cb_t));
        s_cli_cb_valid = true;
    } else {
        memset(&s_cli_cb, 0, sizeof(uart_cli_cb_t));
        s_cli_cb_valid = false;
    }
}

void UART_ClearCLICallbacks(void)
{
    memset(&s_cli_cb, 0, sizeof(uart_cli_cb_t));
    s_cli_cb_valid = false;
    g_pending_req = PENDING_NONE;
}

const char *UART_GetCLIBuf(void) { return s_cli_resp_buf; }
uint16_t UART_GetCLIBufLen(void) { return s_cli_resp_len; }

void UART_SetSubmodelCallbacks(const uart_submodel_cb_t *cb)
{
    if (cb) {
        memcpy(&s_submodel_cb, cb, sizeof(uart_submodel_cb_t));
        s_submodel_cb_valid = true;
    } else {
        memset(&s_submodel_cb, 0, sizeof(uart_submodel_cb_t));
        s_submodel_cb_valid = false;
    }
}

void UART_ClearSubmodelCallbacks(void)
{
    memset(&s_submodel_cb, 0, sizeof(uart_submodel_cb_t));
    s_submodel_cb_valid = false;
}

void UART_SetEMusicCallbacks(const uart_emusic_callbacks_t *cb)
{
    if (cb) {
        memcpy(&s_emusic_cb, cb, sizeof(uart_emusic_callbacks_t));
        s_emusic_cb_valid = true;
    } else {
        memset(&s_emusic_cb, 0, sizeof(uart_emusic_callbacks_t));
        s_emusic_cb_valid = false;
    }
}

void UART_ClearEMusicCallbacks(void)
{
    memset(&s_emusic_cb, 0, sizeof(uart_emusic_callbacks_t));
    s_emusic_cb_valid = false;
}

/*=============================================================================
 *  USART1 Configuration (CH32V307)
 *=============================================================================*/

#define USART1_BAUDRATE     921600
#define USART1_TX_PIN       GPIO_Pin_9
#define USART1_TX_PORT      GPIOA
#define USART1_RX_PIN       GPIO_Pin_10
#define USART1_RX_PORT      GPIOA

/*=============================================================================
 *  RX State Machine
 *=============================================================================*/

typedef enum {
    RX_WAIT_HEAD = 0,
    RX_SRC,
    RX_DST,
    RX_LEN,
    RX_CMD,
    RX_DATA,
    RX_TAIL0,
    RX_TAIL1,
    RX_TAIL2,
    RX_TAIL3,
    RX_STREAM_DATA,
    RX_STREAM_TT1,
    RX_STREAM_TT2,
    RX_STREAM_TT3,
} rx_state_t;

static volatile rx_state_t s_rx_state = RX_WAIT_HEAD;
static uint8_t s_rx_src, s_rx_dst, s_rx_len, s_rx_cmd;
static uint8_t s_rx_data[PROTO_MAX_DATA_LEN];
static volatile uint8_t s_rx_idx;
static volatile uint8_t s_rx_remaining;

#define STREAM_BUF_SIZE  512
static volatile uint8_t s_stream_buf[STREAM_BUF_SIZE];
static volatile uint16_t s_stream_idx;

/* Set in the RX ISR when a CLI response is complete and ready to dispatch;
 * consumed by UART_Module_Poll in the main loop. */
static volatile uint8_t s_cli_dispatch_pending = 0;

/* Ring buffer for ISR → poll communication */
#define RING_ENTRY_MAX  (4 + PROTO_MAX_DATA_LEN)
static volatile uint8_t s_ring[UART_RX_BUF_SIZE];
static volatile uint16_t s_ring_head = 0;
static volatile uint16_t s_ring_tail = 0;

/*=============================================================================
 *  CLI Chunk Handling in ISR context
 *
 *  CLI responses (especially multi-KB `ls` / `cat` / `read` transfers of
 *  14+ frames back-to-back) overflow the 2KB main-loop ring when the loop
 *  is blocked by an e-paper refresh — dropped frames lose EOF and the
 *  transfer hangs forever.  CLI chunk frames are therefore consumed
 *  DIRECTLY in the RX ISR (append + incremental ls parse + stream cb);
 *  only the final dispatch is deferred to the main loop via a flag.
 *  Note: on_cli_stream handlers must tolerate ISR context (they only
 *  touch app-side RAM buffers and are gated by their is_loading flags).
 *===========================================================================*/

static void cli_resp_append(const char *data, uint16_t len);
static void cli_resp_dispatch(void);
static void cli_ls_feed(const char *data, uint16_t len);
static void cli_ls_reset(void);

static void cli_chunk_isr(const uint8_t *data, uint8_t len)
{
    if (len >= 3) {
        uint8_t flags = data[1];
        bool has_sof = (flags & 0x01) != 0;
        bool has_eof = (flags & 0x02) != 0;

        if (has_sof) {
            s_cli_resp_len = 0;
            s_cli_resp_accumulating = true;
        }

        if (s_cli_resp_accumulating || has_sof) {
            const char *frame_text = (const char *)&data[2];
            uint16_t frame_len = len - 2;

            if (s_cli_cb_valid && s_cli_cb.on_cli_stream) {
                s_cli_cb.on_cli_stream(frame_text, frame_len, has_eof);
            }

            cli_resp_append(frame_text, frame_len);

            /* Incremental ls parse (immune to response-buffer truncation) */
            if (strcmp(s_cli_cmd_tag, "ls") == 0) {
                cli_ls_feed(frame_text, frame_len);
            }

            if (has_eof) {
                s_cli_resp_accumulating = false;
                s_cli_dispatch_pending = 1;
            }
        } else {
            /* Legacy frame without flags */
            const char *frame_text = (const char *)&data[1];
            uint16_t frame_len = len - 1;

            if (s_cli_cb_valid && s_cli_cb.on_cli_stream) {
                s_cli_cb.on_cli_stream(frame_text, frame_len, true);
            }
            cli_resp_append(frame_text, frame_len);
            if (strcmp(s_cli_cmd_tag, "ls") == 0) {
                cli_ls_feed(frame_text, frame_len);
            }
            s_cli_dispatch_pending = 1;
        }
    } else if (len >= 2) {
        const char *frame_text = (const char *)&data[1];
        uint16_t frame_len = len - 1;

        if (s_cli_cb_valid && s_cli_cb.on_cli_stream) {
            s_cli_cb.on_cli_stream(frame_text, frame_len, true);
        }
        cli_resp_append(frame_text, frame_len);
        if (strcmp(s_cli_cmd_tag, "ls") == 0) {
            cli_ls_feed(frame_text, frame_len);
        }
        s_cli_dispatch_pending = 1;
    }
    g_pending_req = PENDING_NONE;
}

static void ring_push_frame(uint8_t src, uint8_t dst, uint8_t cmd,
                             const volatile uint8_t *data, uint8_t dlen)
{
    uint16_t total = 4 + dlen;
    if ((uint16_t)(s_ring_head - s_ring_tail) + total > UART_RX_BUF_SIZE) return;

    uint16_t h = s_ring_head;
    s_ring[h++ % UART_RX_BUF_SIZE] = src;
    s_ring[h++ % UART_RX_BUF_SIZE] = dst;
    s_ring[h++ % UART_RX_BUF_SIZE] = cmd;
    s_ring[h++ % UART_RX_BUF_SIZE] = dlen;
    for (uint8_t i = 0; i < dlen; i++) {
        s_ring[h++ % UART_RX_BUF_SIZE] = data[i];
    }
    s_ring_head = h;
}

/*=============================================================================
 *  USART1 Initialization (CH32V307)
 *=============================================================================*/

void UART_Module_Init(void)
{
    printf("[UART_Module_Init] start\r\n");

    /* Init display state */
    memset(&g_disp_state, 0, sizeof(g_disp_state));
    g_disp_state.screen_on   = true;
    g_disp_state.brightness  = 100;  /* E-ink: no backlight, always "on" */
    g_disp_state.rotation    = 0;
    g_disp_state.auto_off_sec = g_settings.auto_off_enable ?
                                (uint16_t)g_settings.auto_off_min * 60 : 0;
    g_disp_state.last_activity_ms = ui_get_real_ms();

    /* Enable clocks: AFIO + GPIOA + USART1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_USART1, ENABLE);

    /* TX pin: PA9 AF push-pull */
    GPIO_InitTypeDef gpio = {0};
    gpio.GPIO_Pin   = USART1_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(USART1_TX_PORT, &gpio);

    /* RX pin: PA10 AF push-pull (CH32V307 USART1 RX uses AF_PP) */
    gpio.GPIO_Pin  = USART1_RX_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(USART1_RX_PORT, &gpio);

    /* USART config */
    USART_InitTypeDef usart = {0};
    usart.USART_BaudRate            = USART1_BAUDRATE;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART1_IRQn, 1 << 4);
    NVIC_EnableIRQ(USART1_IRQn);
    USART_Cmd(USART1, ENABLE);

    s_rx_state   = RX_WAIT_HEAD;
    s_ring_head  = 0;
    s_ring_tail  = 0;

    printf("[UART_Module_Init] done (921600 8-N-1)\r\n");
}

/*=============================================================================
 *  USART1 IRQ — RX byte-by-byte state machine
 *=============================================================================*/

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == RESET) return;
    uint8_t b = (uint8_t)USART_ReceiveData(USART1);

    switch (s_rx_state) {
    case RX_WAIT_HEAD:
        if (b == PROTO_FRAME_HEAD) s_rx_state = RX_SRC;
        break;
    case RX_SRC:
        s_rx_src = b; s_rx_state = RX_DST; break;
    case RX_DST:
        s_rx_dst = b; s_rx_state = RX_LEN; break;
    case RX_LEN:
        s_rx_len = b;
        if (b == PROTO_STREAM_MARKER) {
            s_stream_idx = 0;
            s_rx_state = RX_STREAM_DATA;
        } else {
            s_rx_state = RX_CMD;
        }
        break;
    case RX_CMD:
        s_rx_cmd = b;
        s_rx_idx = 0;
        if (s_rx_len > 1) {
            s_rx_remaining = s_rx_len - 1;
            s_rx_state = RX_DATA;
        } else {
            s_rx_remaining = 0;
            s_rx_state = RX_TAIL0;
        }
        break;
    case RX_DATA:
        if (s_rx_idx < PROTO_MAX_DATA_LEN)
            s_rx_data[s_rx_idx] = b;
        s_rx_idx++;
        s_rx_remaining--;
        if (s_rx_remaining == 0) s_rx_state = RX_TAIL0;
        break;
    case RX_TAIL0:
        s_rx_state = (b == PROTO_FRAME_TAIL0) ? RX_TAIL1 : RX_WAIT_HEAD;
        break;
    case RX_TAIL1:
        s_rx_state = (b == PROTO_FRAME_TAIL1) ? RX_TAIL2 : RX_WAIT_HEAD;
        break;
    case RX_TAIL2:
        s_rx_state = (b == PROTO_FRAME_TAIL2) ? RX_TAIL3 : RX_WAIT_HEAD;
        break;
    case RX_TAIL3:
        if (b == PROTO_FRAME_TAIL3) {
            uint8_t dlen = (s_rx_idx < PROTO_MAX_DATA_LEN) ? s_rx_idx : 0;
            /* CLI chunk frames bypass the ring: they are consumed inline so
             * multi-KB transfers cannot overflow the 2KB main-loop ring
             * while the loop is blocked by an e-paper refresh. */
            if (s_rx_cmd == CMD_DISP_EXT && dlen >= 1 && s_rx_data[0] == DISP_EXT_CLI) {
                cli_chunk_isr(s_rx_data, dlen);
            } else {
                ring_push_frame(s_rx_src, s_rx_dst, s_rx_cmd, s_rx_data, dlen);
            }
        }
        s_rx_state = RX_WAIT_HEAD;
        break;

    /* Stream frame states (LEN=0xFF) */
    case RX_STREAM_DATA:
        if (b == PROTO_FRAME_TAIL0) {
            s_rx_state = RX_STREAM_TT1;
        } else {
            if (s_stream_idx < STREAM_BUF_SIZE)
                s_stream_buf[s_stream_idx++] = b;
        }
        break;
    case RX_STREAM_TT1:
        if (b == PROTO_FRAME_TAIL1) { s_rx_state = RX_STREAM_TT2; }
        else {
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = PROTO_FRAME_TAIL0;
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = b;
            s_rx_state = RX_STREAM_DATA;
        }
        break;
    case RX_STREAM_TT2:
        if (b == PROTO_FRAME_TAIL2) { s_rx_state = RX_STREAM_TT3; }
        else {
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = PROTO_FRAME_TAIL0;
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = PROTO_FRAME_TAIL1;
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = b;
            s_rx_state = RX_STREAM_DATA;
        }
        break;
    case RX_STREAM_TT3:
        if (b == PROTO_FRAME_TAIL3) {
            if (s_stream_idx >= 1) {
                s_rx_cmd = s_stream_buf[0];
                uint8_t dlen = (s_stream_idx - 1 < PROTO_MAX_DATA_LEN) ?
                                (uint8_t)(s_stream_idx - 1) : PROTO_MAX_DATA_LEN;
                for (uint8_t i = 0; i < dlen; i++)
                    s_rx_data[i] = s_stream_buf[1 + i];
                /* CLI chunk frames bypass the ring (see RX_TAIL3 note) */
                if (s_rx_cmd == CMD_DISP_EXT && dlen >= 1 && s_rx_data[0] == DISP_EXT_CLI) {
                    cli_chunk_isr(s_rx_data, dlen);
                } else {
                    ring_push_frame(s_rx_src, s_rx_dst, s_rx_cmd, s_rx_data, dlen);
                }
            }
            s_rx_state = RX_WAIT_HEAD;
        } else {
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = PROTO_FRAME_TAIL0;
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = PROTO_FRAME_TAIL1;
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = PROTO_FRAME_TAIL2;
            if (s_stream_idx < STREAM_BUF_SIZE) s_stream_buf[s_stream_idx++] = b;
            s_rx_state = RX_STREAM_DATA;
        }
        break;
    }
}

/*=============================================================================
 *  Frame Transmission
 *=============================================================================*/

static inline void tx_byte(uint8_t b)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, b);
}

void UART_SendFrame(uint8_t dst, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t frame_len = 1 + len;
    tx_byte(PROTO_FRAME_HEAD);
    tx_byte(MODULE_ID_DISPLAY);
    tx_byte(dst);
    tx_byte(frame_len);
    tx_byte(cmd);
    for (uint8_t i = 0; i < len; i++) tx_byte(data[i]);
    tx_byte(PROTO_FRAME_TAIL0);
    tx_byte(PROTO_FRAME_TAIL1);
    tx_byte(PROTO_FRAME_TAIL2);
    tx_byte(PROTO_FRAME_TAIL3);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

void UART_SendACK(uint8_t dst, const uint8_t *data, uint8_t data_len)
{
    UART_SendFrame(dst, CMD_ACK, data, data_len);
}

void UART_SendNACK(uint8_t dst, uint8_t error_code)
{
    UART_SendFrame(dst, CMD_NACK, &error_code, 1);
}

/*=============================================================================
 *  Input Event Processing  (CMD 0x15)
 *=============================================================================*/

static void process_input_event(const uint8_t *data, uint8_t data_len)
{
    if (data_len < 2) return;
    UART_NotifyActivity();

    switch (data[0]) {
    case INPUT_DEV_KEYBOARD: {
        if (data_len < 9) return;
        uint8_t modifiers = data[1];
        uint8_t key_codes[6];
        memcpy(key_codes, &data[3], 6);
        ui_input_feed_keyboard(modifiers, key_codes);
        break;
    }
    case INPUT_DEV_MOUSE: {
        if (data_len < 5) return;
        ui_input_feed_mouse((int8_t)data[2], (int8_t)data[3],
                            data[1], (int8_t)data[4]);
        break;
    }
    case INPUT_DEV_TOUCH: {
        if (data_len < 8) return;
        uint8_t tid = data[1];
        int16_t x = (int16_t)((uint16_t)data[3] << 8 | data[4]);
        int16_t y = (int16_t)((uint16_t)data[5] << 8 | data[6]);
        switch (data[2]) {
        case TOUCH_EVT_PRESS:   ui_input_feed_touch(tid, true, x, y);  break;
        case TOUCH_EVT_MOVE:    ui_input_feed_touch(tid, true, x, y);  break;
        case TOUCH_EVT_RELEASE: ui_input_feed_touch(tid, false, x, y); break;
        default: break;
        }
        break;
    }
    case INPUT_DEV_CORE_KEY: {
        if (data_len < 3) return;
        ui_input_feed_core_key(data[2], data[1]);
        break;
    }
    default: break;
    }
}

/*=============================================================================
 *  Basic Command Handlers  (0x11–0x1F)
 *=============================================================================*/

static void handle_set_brightness(const uint8_t *data, uint8_t len, uint8_t src)
{
    (void)src;
    if (len < 1) return;
    uint8_t pct = data[0];
    if (pct > 100) pct = 100;
    g_disp_state.brightness = pct;
    /* E-ink: no backlight, just store value */
}

static void handle_get_brightness(uint8_t src)
{
    uint8_t pct = g_disp_state.brightness;
    UART_SendACK(src, &pct, 1);
}

static void handle_show_page(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    /* Page numbering mirrors the sidebar menu order:
     * 0=Home, 1=Apps, 2=Games, 3=Models, 4=Settings */
    if (data[0] < SIDEBAR_ITEM_COUNT) {
        ui_main_set_menu((menu_item_t)data[0]);
    }
}

static void handle_update_status(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    uint8_t bitmap = data[0];
    uint8_t idx = 1;

    if ((bitmap & STATUS_BIT_BATTERY) && idx < len) {
        g_disp_state.battery_pct = data[idx++];
        g_disp_state.status_valid |= STATUS_BIT_BATTERY;
    }
    if ((bitmap & STATUS_BIT_BLUETOOTH) && idx < len) {
        g_disp_state.bt_connected = data[idx++];
        g_disp_state.status_valid |= STATUS_BIT_BLUETOOTH;
    }
    if ((bitmap & STATUS_BIT_TIME) && idx + 3 < len) {
        g_disp_state.unix_time = ((uint32_t)data[idx] << 24) |
                                  ((uint32_t)data[idx+1] << 16) |
                                  ((uint32_t)data[idx+2] << 8) |
                                   (uint32_t)data[idx+3];
        idx += 4;
        g_disp_state.status_valid |= STATUS_BIT_TIME;
    }
    if ((bitmap & STATUS_BIT_CHARGING) && idx < len) {
        g_disp_state.charge_state = data[idx++];
        g_disp_state.status_valid |= STATUS_BIT_CHARGING;
    }
    if ((bitmap & STATUS_BIT_CURRENT_APP) && idx < len) {
        g_disp_state.current_app_id = data[idx++];
        g_disp_state.status_valid |= STATUS_BIT_CURRENT_APP;
    }
}

static void handle_set_rotation(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    g_disp_state.rotation = data[0];
    /* TODO: implement e-ink rotation if needed */
}

static void handle_get_rotation(uint8_t src)
{
    uint8_t angle = g_disp_state.rotation;
    UART_SendACK(src, &angle, 1);
}

static void handle_screen_control(const uint8_t *data, uint8_t len, uint8_t src)
{
    if (len < 1) return;
    switch (data[0]) {
    case SCREEN_CTRL_OFF:
        g_disp_state.screen_on = false;
        panel_sleep();      /* e-ink: panel deep sleep, image retained */
        break;
    case SCREEN_CTRL_ON:
        g_disp_state.screen_on = true;
        panel_wake();
        break;
    case SCREEN_CTRL_SET_AUTO_OFF:
        if (len >= 2) {
            g_disp_state.auto_off_sec = (uint16_t)data[1] * 60;
            g_settings.auto_off_min = data[1];
            g_settings.auto_off_enable = (data[1] > 0);
        }
        break;
    case SCREEN_CTRL_GET_AUTO_OFF: {
        uint8_t minutes = g_disp_state.auto_off_sec / 60;
        UART_SendACK(src, &minutes, 1);
        break;
    }
    default:
        UART_SendNACK(src, PROTO_ERR_INVALID_PARAM);
        break;
    }
}

static void handle_get_screen_state(uint8_t src)
{
    uint8_t state[4];
    state[0] = g_disp_state.screen_on ? 1 : 0;
    state[1] = g_disp_state.brightness;
    state[2] = g_disp_state.rotation;
    state[3] = (uint8_t)(g_disp_state.auto_off_sec / 60);
    UART_SendACK(src, state, 4);
}

static void handle_show_notice(const uint8_t *data, uint8_t len)
{
    static ui_dialog_t s_notice_dlg;
    static char s_notice_title[17];
    static char s_notice_content[128];
    static uint8_t s_notice_dlg_inited = 0;

    if (len < 18) return;

    if (!s_notice_dlg_inited) {
        ui_dialog_init(&s_notice_dlg);
        s_notice_dlg_inited = 1;
    }

    for (uint8_t i = 0; i < 16; i++) {
        s_notice_title[i] = (char)data[1 + i];
        if (data[1 + i] == '\0') break;
    }
    s_notice_title[16] = '\0';

    uint8_t clen = len - 17;
    if (clen > sizeof(s_notice_content) - 1)
        clen = sizeof(s_notice_content) - 1;
    memcpy(s_notice_content, &data[17], clen);
    s_notice_content[clen] = '\0';

    ui_dialog_show(&s_notice_dlg, s_notice_title, s_notice_content, NULL, NULL);
}

static void handle_music_status(const uint8_t *data, uint8_t len)
{
    if (len < 11) return;
    g_disp_state.music_state  = data[0];
    g_disp_state.music_pos_ms = ((uint32_t)data[1] << 24) | ((uint32_t)data[2] << 16) |
                                 ((uint32_t)data[3] << 8)  |  (uint32_t)data[4];
    g_disp_state.music_dur_ms = ((uint32_t)data[5] << 24) | ((uint32_t)data[6] << 16) |
                                 ((uint32_t)data[7] << 8)  |  (uint32_t)data[8];
    g_disp_state.music_volume = data[9];
    g_disp_state.music_speaker = data[10];
    uint8_t name_len = (len > 11) ? (len - 11) : 0;
    if (name_len > sizeof(g_disp_state.music_track) - 1)
        name_len = sizeof(g_disp_state.music_track) - 1;
    memcpy(g_disp_state.music_track, &data[11], name_len);
    g_disp_state.music_track[name_len] = '\0';
}

static void handle_factory_reset(const uint8_t *data, uint8_t len)
{
    if (len < 1 || data[0] != 0xA5) return;
    settings_load_defaults();
    settings_apply();
    g_disp_state.brightness  = 100;
    g_disp_state.auto_off_sec = g_settings.auto_off_enable ?
                                 (uint16_t)g_settings.auto_off_min * 60 : 0;
    ui_page_invalidate_all();
}

/*=============================================================================
 *  Extended Command Handlers  (CMD=0x10, DATA[0]=sub-cmd)
 *=============================================================================*/

static void handle_ext_module_status(const uint8_t *data, uint8_t len)
{
    if (len < 5) return;
    /* DATA[1]=event (insert/remove/list), DATA[2]=module ID,
     * DATA[3]=type, DATA[4]=subtype. */
    uint8_t evt_type = data[1];
    uint8_t mod_type = data[3];

    if (evt_type == MODULE_EVT_INSERTED || evt_type == MODULE_EVT_REMOVED) {
        uint8_t online = (evt_type == MODULE_EVT_INSERTED) ? 1 : 0;
        if (mod_type == MODULE_TYPE_POWER)
            g_disp_state.power_online = online;
        else if (mod_type == MODULE_TYPE_WIRELESS)
            g_disp_state.wireless_online = online;
    }

    printf("[UART] module status: evt=%d id=0x%02X type=%d sub=%d\r\n",
           data[1], data[2], data[3], data[4]);
    ui_models_notify_module_change();
}

static void handle_ext_bt_event(const uint8_t *data, uint8_t len)
{
    if (len < 2) return;
    uint8_t evt_type = data[1];

    /* 连接状态变化镜像到状态缓存，Home 的 BT 卡片据此响应式刷新。
     * BT 应用在 E-ink 上仍为 stub，流量数组仅为两屏状态一致而保留。 */
    switch (evt_type) {
    case BT_EVT_CONNECTED:
        g_disp_state.wireless_online = 1;
        g_disp_state.bt_connected = 1;
        break;
    case BT_EVT_DISCONNECTED:
        g_disp_state.bt_connected = 0;
        break;
    case BT_EVT_STATUS:
        if (len >= 4) {
            g_disp_state.wireless_online = data[2];
            g_disp_state.bt_connected    = data[3];
        }
        break;
    case BT_EVT_TRAFFIC:
        if (len >= 3) {
            uint8_t n = data[2];
            uint8_t i;
            if (n > 10) n = 10;
            for (i = 0; i < n && (uint8_t)(3 + i * 2 + 1) < len; i++) {
                g_disp_state.bt_traffic[i] =
                    ((uint16_t)data[3 + i * 2] << 8) | data[4 + i * 2];
            }
            g_disp_state.bt_traffic_count = i;
            g_disp_state.bt_traffic_head  = 0;
            g_disp_state.wireless_online  = 1;
        }
        break;
    default:
        break;
    }
}

static void handle_ext_submodel_event(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;
    uint8_t sub_type = data[1];
    uint8_t sub_cmd  = data[2];
    const uint8_t *evt_data = (len > 3) ? &data[3] : NULL;
    uint8_t evt_len = (len > 3) ? (len - 3) : 0;

    printf("[SUB] type=%d cmd=%d len=%d\r\n", sub_type, sub_cmd, evt_len);

    if (s_submodel_cb_valid && s_submodel_cb.on_submodel_event) {
        s_submodel_cb.on_submodel_event(sub_type, sub_cmd, evt_data, evt_len);
    }
}

static void handle_ext_power_event(const uint8_t *data, uint8_t len)
{
    if (len < 2) return;
    uint8_t evt_type = data[1];

    /* 收到电源事件即说明 Power 模块在线 */
    g_disp_state.power_online = 1;

    /* POWER_EVT_STATUS_CHANGE: DATA[2]=电量%, DATA[3]=充电状态位图(bit0 充电中)
     * 字节顺序与 Core(Power_ReportStatusToDisplay)、display-1 一致 */
    if (evt_type == POWER_EVT_STATUS_CHANGE && len >= 4) {
        g_disp_state.battery_pct  = data[2];
        g_disp_state.charge_state = data[3];
        g_disp_state.status_valid |= STATUS_BIT_BATTERY | STATUS_BIT_CHARGING;
    }
}

static void handle_ext_config_result(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;
    /* TODO: show config save/load result */
}

static void handle_ext_hid_status(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;
    uint8_t evt      = data[1];
    uint8_t dev_type = data[2];

    bool connected = (evt == HID_EVT_CONNECTED);

    switch (dev_type) {
    case HID_DEV_KEYBOARD:
        g_settings.ext_keyboard_connected = connected;
        printf("[HID] ext keyboard %s\r\n", connected ? "connected" : "disconnected");
        break;
    case HID_DEV_MOUSE:
        g_settings.ext_mouse_connected = connected;
        ui_input_set_mouse_connected(connected);
        printf("[HID] ext mouse %s\r\n", connected ? "connected" : "disconnected");
        break;
    default:
        break;
    }
}

static void process_extended_cmd(uint8_t sub_cmd, const uint8_t *data, uint8_t len, uint8_t src)
{
    switch (sub_cmd) {
    case DISP_EXT_MODULE_STATUS:
        handle_ext_module_status(data, len);
        break;
    case DISP_EXT_BT_EVENT:
        handle_ext_bt_event(data, len);
        break;
    case DISP_EXT_SUBMODEL_EVENT:
        handle_ext_submodel_event(data, len);
        break;
    case DISP_EXT_POWER_EVENT:
        handle_ext_power_event(data, len);
        break;
    case DISP_EXT_CONFIG_RESULT:
        handle_ext_config_result(data, len);
        break;
    case DISP_EXT_HID_STATUS:
        handle_ext_hid_status(data, len);
        break;
    case DISP_EXT_CLI:
        if (len >= 3) {
            uint8_t flags = data[1];
            bool has_sof = (flags & 0x01) != 0;
            bool has_eof = (flags & 0x02) != 0;

            if (has_sof) {
                s_cli_resp_len = 0;
                s_cli_resp_accumulating = true;
            }

            if (s_cli_resp_accumulating || has_sof) {
                const char *frame_text = (const char *)&data[2];
                uint16_t frame_len = len - 2;

                if (s_cli_cb_valid && s_cli_cb.on_cli_stream) {
                    s_cli_cb.on_cli_stream(frame_text, frame_len, has_eof);
                }

                cli_resp_append(frame_text, frame_len);

                /* Incremental ls parse (immune to response-buffer truncation) */
                if (strcmp(s_cli_cmd_tag, "ls") == 0) {
                    cli_ls_feed(frame_text, frame_len);
                }

                if (has_eof) {
                    s_cli_resp_accumulating = false;
                    cli_resp_dispatch();
                }
            } else {
                const char *frame_text = (const char *)&data[1];
                uint16_t frame_len = len - 1;

                if (s_cli_cb_valid && s_cli_cb.on_cli_stream) {
                    s_cli_cb.on_cli_stream(frame_text, frame_len, true);
                }
                cli_resp_append(frame_text, frame_len);
                if (strcmp(s_cli_cmd_tag, "ls") == 0) {
                    cli_ls_feed(frame_text, frame_len);
                }
                cli_resp_dispatch();
            }
        } else if (len >= 2) {
            const char *frame_text = (const char *)&data[1];
            uint16_t frame_len = len - 1;

            if (s_cli_cb_valid && s_cli_cb.on_cli_stream) {
                s_cli_cb.on_cli_stream(frame_text, frame_len, true);
            }
            cli_resp_append(frame_text, frame_len);
            if (strcmp(s_cli_cmd_tag, "ls") == 0) {
                cli_ls_feed(frame_text, frame_len);
            }
            cli_resp_dispatch();
        }
        g_pending_req = PENDING_NONE;
        break;
    case DISP_EXT_CWD_NOTIFY:
        if (len >= 2 && s_cli_cb_valid && s_cli_cb.on_cwd_notify) {
            char path_buf[64];
            uint8_t plen = len - 1;
            if (plen > sizeof(path_buf) - 1) plen = sizeof(path_buf) - 1;
            memcpy(path_buf, &data[1], plen);
            path_buf[plen] = '\0';
            s_cli_cb.on_cwd_notify(path_buf);
        }
        break;
    default:
        UART_SendNACK(src, PROTO_ERR_UNSUPPORTED_CMD);
        break;
    }
}

/*=============================================================================
 *  Main Frame Dispatcher
 *=============================================================================*/

static void process_frame(uint8_t src, uint8_t dst, uint8_t cmd,
                           const uint8_t *data, uint8_t data_len)
{
    if (dst != MODULE_ID_DISPLAY && dst != 0xFF) return;

    switch (cmd) {
    case CMD_NOP:
        /* Heartbeat / keepalive: acknowledge without side effects */
        UART_SendACK(src, NULL, 0);
        break;
    case CMD_DISP_SET_BRIGHTNESS:
        handle_set_brightness(data, data_len, src);
        break;
    case CMD_DISP_GET_BRIGHTNESS:
        handle_get_brightness(src);
        break;
    case CMD_DISP_SHOW_PAGE:
        handle_show_page(data, data_len);
        break;
    case CMD_DISP_UPDATE_STATUS:
        handle_update_status(data, data_len);
        break;
    case CMD_DISP_INPUT_EVENT:
        process_input_event(data, data_len);
        break;
    case CMD_DISP_SET_ROTATION:
        handle_set_rotation(data, data_len);
        break;
    case CMD_DISP_GET_ROTATION:
        handle_get_rotation(src);
        break;
    case CMD_DISP_SCREEN_CONTROL:
        handle_screen_control(data, data_len, src);
        break;
    case CMD_DISP_GET_SCREEN_STATE:
        handle_get_screen_state(src);
        break;
    case CMD_DISP_SHOW_NOTICE:
        handle_show_notice(data, data_len);
        break;
    case CMD_DISP_MUSIC_STATUS:
        handle_music_status(data, data_len);
        break;
    case CMD_DISP_FACTORY_RESET:
        handle_factory_reset(data, data_len);
        break;

    case CMD_KBD_MUSIC_KEYS:
        if (s_emusic_cb_valid && s_emusic_cb.on_music_keys && data_len >= 3)
            s_emusic_cb.on_music_keys(data);
        break;
    case CMD_KBD_MUSIC_BUTTONS:
        if (s_emusic_cb_valid && s_emusic_cb.on_music_buttons && data_len >= 2)
            s_emusic_cb.on_music_buttons(data);
        break;
    case CMD_KBD_MUSIC_FADERS:
        if (s_emusic_cb_valid && s_emusic_cb.on_music_faders && data_len >= 6) {
            uint16_t fl = ((uint16_t)data[0] << 8) | data[1];
            uint16_t fm = ((uint16_t)data[2] << 8) | data[3];
            uint16_t fr = ((uint16_t)data[4] << 8) | data[5];
            s_emusic_cb.on_music_faders(fl, fm, fr);
        }
        break;

    case CMD_DISP_EXT:
        if (data_len >= 1) {
            process_extended_cmd(data[0], data, data_len, src);
        }
        break;

    case CMD_GET_TYPE: {
        /* E-ink display: subtype = 0x02 */
        uint8_t resp[5] = {
            MODULE_TYPE_DISPLAY,
            0x02,                 /* subtype = E-ink */
            0x01,                 /* HW version */
            0x02,                 /* FW major */
            0x00,                 /* FW minor */
        };
        UART_SendACK(src, resp, 5);
        break;
    }
    case CMD_GET_STATUS: {
        uint8_t resp[3] = {
            g_disp_state.screen_on ? 1 : 0,
            g_disp_state.brightness,
            g_disp_state.rotation,
        };
        UART_SendACK(src, resp, 3);
        break;
    }
    case CMD_ACK:
        g_pending_req = PENDING_NONE;
        break;
    case CMD_NACK:
        if (s_cli_cb_valid && g_pending_req == PENDING_CLI) {
            s_cli_resp_len = 0;
            if (s_cli_cb.on_cli_stream)
                s_cli_cb.on_cli_stream("ERROR: NACK", 11, true);
            else if (s_cli_cb.on_cli_complete)
                s_cli_cb.on_cli_complete("ERROR: NACK", 11, s_cli_cmd_tag);
        }
        g_pending_req = PENDING_NONE;
        break;

    default:
        UART_SendNACK(src, PROTO_ERR_UNSUPPORTED_CMD);
        break;
    }
}

/*=============================================================================
 *  Poll — called from main loop
 *=============================================================================*/

void UART_Module_Poll(void)
{
    /* Deferred CLI dispatch (the chunks themselves are consumed in the ISR) */
    if (s_cli_dispatch_pending) {
        s_cli_dispatch_pending = 0;
        cli_resp_dispatch();
    }

    while (s_ring_tail != s_ring_head) {
        uint16_t available = s_ring_head - s_ring_tail;
        if (available < 4) break;

        uint8_t src  = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        uint8_t dst  = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        uint8_t cmd  = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        uint8_t dlen = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];

        if (s_ring_head - s_ring_tail < dlen) {
            if (s_ring_tail >= 4) s_ring_tail -= 4;
            else s_ring_tail = UART_RX_BUF_SIZE - (4 - s_ring_tail);
            break;
        }

        for (uint8_t i = 0; i < dlen; i++) {
            s_poll_data[i] = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        }

        process_frame(src, dst, cmd, s_poll_data, dlen);
    }

    /* Auto screen-off check (e-ink: panel DSLP, image retained).
     * Skipped while a CLI transfer is pending so long file reads are
     * not interrupted by the panel power cycle. */
    if (g_disp_state.auto_off_sec > 0 && g_disp_state.screen_on &&
        g_pending_req == PENDING_NONE) {
        uint32_t now = ui_get_real_ms();
        if (now - g_disp_state.last_activity_ms >
            (uint32_t)g_disp_state.auto_off_sec * 1000) {
            g_disp_state.screen_on = false;
            panel_sleep();
        }
    }
}

/*=============================================================================
 *  CLI Response Assembly — implementation
 *=============================================================================*/

static void cli_resp_reset(void)
{
    s_cli_resp_len = 0;
    s_cli_resp_accumulating = false;
}

static void cli_resp_append(const char *data, uint16_t len)
{
    uint16_t space = CLI_RESP_BUF_SIZE - 1 - s_cli_resp_len;
    uint16_t copy = (len < space) ? len : space;
    if (copy > 0) {
        memcpy(&s_cli_resp_buf[s_cli_resp_len], data, copy);
        s_cli_resp_len += copy;
    }
}

/*=============================================================================
 *  Incremental ls parser — implementation
 *===========================================================================*/

static void cli_ls_reset(void)
{
    s_ls_line_len = 0;
    s_ls_count = 0;
}

/* Parse one complete ls output line into the entry table / cwd notify */
static void cli_ls_parse_line(const char *line, uint8_t len)
{
    /* Path header: "--- <path> ---" */
    if (len >= 8 && line[0] == '-' && line[1] == '-' && line[2] == '-' && line[3] == ' ') {
        const char *path_start = line + 4;
        const char *path_end = strstr(path_start, " ---");
        if (path_end) {
            uint8_t path_len = (uint8_t)(path_end - path_start);
            char ls_path[64];
            if (path_len > sizeof(ls_path) - 1) path_len = sizeof(ls_path) - 1;
            memcpy(ls_path, path_start, path_len);
            ls_path[path_len] = '\0';
            if (s_cli_cb_valid && s_cli_cb.on_cwd_notify)
                s_cli_cb.on_cwd_notify(ls_path);
        }
        return;
    }

    if (s_ls_count >= FILE_LIST_MAX_ENTRIES) return;

    const char *dir_tag = strstr(line, "[DIR]");
    const char *file_tag = strstr(line, "[FILE]");
    const char *tag = NULL;
    uint8_t is_dir = 0;
    if (dir_tag && (!file_tag || dir_tag < file_tag)) {
        tag = dir_tag; is_dir = 1;
    } else if (file_tag) {
        tag = file_tag; is_dir = 0;
    }
    if (!tag) return;

    const char *p = tag + (is_dir ? 5 : 6);
    while (*p == ' ' || *p == '\t') p++;

    file_entry_t *e = &s_parsed_entries[s_ls_count];
    uint8_t nlen = 0;
    while (*p && *p != '\r' && *p != '\n' && nlen < FILE_NAME_MAX_LEN) {
        e->name[nlen++] = *p++;
    }
    e->name[nlen] = '\0';
    while (nlen > 0 && (e->name[nlen-1] == ' ' || e->name[nlen-1] == '\t')) {
        e->name[--nlen] = '\0';
    }
    if (nlen == 0) return;

    e->attr = is_dir ? FILE_ATTR_IS_DIR : 0;
    e->size = 0;

    if (!is_dir) {
        /* Trailing "(<size>)" suffix */
        uint8_t nlen2 = (uint8_t)strlen(e->name);
        for (int8_t k = (int8_t)nlen2 - 1; k >= 0; k--) {
            if (e->name[k] == '(') {
                e->name[k] = '\0';
                uint32_t val = 0;
                const char *sp = &e->name[k] + 1;
                while (*sp >= '0' && *sp <= '9') {
                    val = val * 10 + (uint32_t)(*sp - '0');
                    sp++;
                }
                e->size = val;
                nlen2 = (uint8_t)strlen(e->name);
                while (nlen2 > 0 && (e->name[nlen2-1] == ' ' || e->name[nlen2-1] == '\t')) {
                    e->name[--nlen2] = '\0';
                }
                break;
            }
        }
    }
    s_ls_count++;
}

/* Feed one CLI text chunk into the incremental parser (line assembly) */
static void cli_ls_feed(const char *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\n') {
            s_ls_line[s_ls_line_len] = '\0';
            cli_ls_parse_line(s_ls_line, s_ls_line_len);
            s_ls_line_len = 0;
        } else if (c != '\r') {
            if (s_ls_line_len < LS_LINE_BUF_SIZE - 1) {
                s_ls_line[s_ls_line_len++] = c;
            }
        }
    }
}

/* Flush a trailing partial line (no newline before EOF) */
static void cli_ls_flush(void)
{
    if (s_ls_line_len > 0) {
        s_ls_line[s_ls_line_len] = '\0';
        cli_ls_parse_line(s_ls_line, s_ls_line_len);
        s_ls_line_len = 0;
    }
}

static void cli_resp_dispatch(void)
{
    s_cli_resp_buf[s_cli_resp_len] = '\0';
    printf("[CLI] dispatch tag='%s' len=%d p_ls=%d\r\n",
           s_cli_cmd_tag, s_cli_resp_len, s_pending_ls_after_cd);

    if (!s_cli_cb_valid) return;

    if (strcmp(s_cli_cmd_tag, "cd") == 0 && s_pending_ls_after_cd) {
        s_pending_ls_after_cd = false;
        printf("[CLI] cd done, sending ls\r\n");
        /* CH378 状态稳定延时：cd 改变了 CH378 目录状态，
         * 需要等待 CH378 内部状态稳定后再发 ls（与 display-1 一致） */
        Delay_Ms(20);
        UART_SendCLI("ls");
        return;
    }

    if (s_cli_cb.on_file_list && strcmp(s_cli_cmd_tag, "ls") == 0) {
        printf("[CLI] ls done: %d entries\r\n", s_ls_count);
        cli_ls_flush();   /* last line may lack a trailing newline */
        s_cli_cb.on_file_list(0x00, s_parsed_entries, s_ls_count);
        return;
    }

    if (s_cli_cb.on_cli_complete) {
        s_cli_cb.on_cli_complete(s_cli_resp_buf, s_cli_resp_len, s_cli_cmd_tag);
        return;
    }
}

/*=============================================================================
 *  Display → Core Request Helpers
 *=============================================================================*/

void UART_NotifyActivity(void)
{
    g_disp_state.last_activity_ms = ui_get_real_ms();
    if (!g_disp_state.screen_on) {
        g_disp_state.screen_on = true;
    }
    panel_wake();
}

const char *UART_GetLastCLITag(void)
{
    return s_cli_cmd_tag;
}

void UART_SendCLI(const char *cmd)
{
    {
        uint8_t i = 0;
        while (i < CLI_CMD_TAG_SIZE - 1 && cmd[i] && cmd[i] != ' ') {
            s_cli_cmd_tag[i] = cmd[i];
            i++;
        }
        s_cli_cmd_tag[i] = '\0';
    }

    /* New ls listing: reset the incremental parser state */
    if (strcmp(s_cli_cmd_tag, "ls") == 0) {
        cli_ls_reset();
    }

    cli_resp_reset();
    g_pending_req = PENDING_CLI;

    uint8_t buf[PROTO_MAX_DATA_LEN];
    buf[0] = DISP_EXT_CLI;
    uint8_t clen = (uint8_t)strlen(cmd);
    if (clen > sizeof(buf) - 2) clen = sizeof(buf) - 2;
    memcpy(&buf[1], cmd, clen);
    UART_SendFrame(MODULE_ID_CORE, CMD_DISP_EXT, buf, 1 + clen);
}

void UART_RequestFileList(const char *dir_or_null)
{
    if (dir_or_null && dir_or_null[0] != '\0') {
        s_pending_ls_after_cd = true;
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "cd \"%s\"", dir_or_null);
        UART_SendCLI(cmd);
    } else {
        s_pending_ls_after_cd = false;
        UART_SendCLI("ls");
    }
}

void UART_SendCD(const char *dir)
{
    char cmd[80];
    snprintf(cmd, sizeof(cmd), "cd \"%s\"", dir);
    UART_SendCLI(cmd);
}

void UART_SendCDUp(void)
{
    UART_SendCLI("cd ..");
}

void UART_SendPWD(void)
{
    UART_SendCLI("pwd");
}

void UART_SendPlayMusic(const char *path)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "play \"%s\"", path);
    UART_SendCLI(cmd);
}

void UART_SendMusicControl(uint8_t ctrl_type, uint8_t param)
{
    (void)param;
    switch (ctrl_type) {
    case MUSIC_CTRL_PLAY:    UART_SendCLI("resume"); break;
    case MUSIC_CTRL_PAUSE:   UART_SendCLI("pause"); break;
    case MUSIC_CTRL_STOP:    UART_SendCLI("stop");  break;
    default: break;
    }
}

void UART_SendVolumeControl(uint8_t op, uint8_t volume)
{
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "vol %d", volume);
    UART_SendCLI(cmd);
}

void UART_SendFileOperation(uint8_t op_type, const char *path)
{
    char cmd[128];
    switch (op_type) {
    case FILE_OP_MKDIR:
        snprintf(cmd, sizeof(cmd), "mkdir \"%s\"", path);
        break;
    case FILE_OP_DELETE_FILE:
        snprintf(cmd, sizeof(cmd), "rm \"%s\"", path);
        break;
    case FILE_OP_DELETE_DIR:
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
        break;
    case FILE_OP_RENAME:
        return;
    default:
        return;
    }
    UART_SendCLI(cmd);
}

void UART_RequestFileRead(const char *path)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "cat \"%s\"", path);
    UART_SendCLI(cmd);
}

void UART_RequestFileSave(const char *path)
{
    (void)path;
}

void UART_SendBTControl(uint8_t ctrl_type, const uint8_t *param, uint8_t param_len)
{
    uint8_t buf[PROTO_MAX_DATA_LEN];
    buf[0] = DISP_EXT_BT_CONTROL;
    buf[1] = ctrl_type;
    uint8_t total = 2;
    if (param && param_len > 0 && param_len <= sizeof(buf) - 2) {
        memcpy(&buf[2], param, param_len);
        total += param_len;
    }
    UART_SendFrame(MODULE_ID_CORE, CMD_DISP_EXT, buf, total);
}

void UART_SendGetSysStatus(void)
{
    uint8_t d = DISP_EXT_GET_SYS_STATUS;
    UART_SendFrame(MODULE_ID_CORE, CMD_DISP_EXT, &d, 1);
}

void UART_SendErrorReport(uint8_t error_code, const char *msg)
{
    uint8_t buf[PROTO_MAX_DATA_LEN];
    buf[0] = DISP_EXT_ERROR_REPORT;
    buf[1] = error_code;
    uint8_t mlen = msg ? (uint8_t)strlen(msg) : 0;
    if (mlen > sizeof(buf) - 3) mlen = sizeof(buf) - 3;
    if (mlen > 0) memcpy(&buf[2], msg, mlen);
    UART_SendFrame(MODULE_ID_CORE, CMD_DISP_EXT, buf, 2 + mlen);
}
