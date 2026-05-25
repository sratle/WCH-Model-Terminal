/********************************** (C) COPYRIGHT *******************************
* File Name          : uart_module.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2026/05/23
* Description        : UART1 module communication with Core.
*                      Implements Protocol_Display.md V3.0:
*                      - ISR-driven RX state machine (standard + stream frames)
*                      - Command dispatch for basic + extended opcodes
*                      - ACK/NACK response generation
*                      - Input event routing to ui_input_feed_*()
*                      - V3.0: CLI passthrough for file/music/directory ops
********************************************************************************/
#include "uart_module.h"
#include "../MiniUI/miniui.h"
#include "../settings.h"
#include "../SSD1963/ssd1963.h"
#include "ch32h417.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Global Display State
 *=============================================================================*/

uart_disp_state_t g_disp_state;

/*=============================================================================
 *  App-Layer Callback Storage
 *=============================================================================*/

static uart_app_callbacks_t s_app_cb;
static bool s_app_cb_valid = false;
volatile pending_req_t g_pending_req = PENDING_NONE;

/*=============================================================================
 *  CLI Response Assembly Buffer (forward declarations for internal use)
 *=============================================================================*/

#define CLI_RESP_BUF_SIZE   2048
static char s_cli_resp_buf[CLI_RESP_BUF_SIZE];
static uint16_t s_cli_resp_len = 0;
static bool s_cli_resp_active = false;

static void cli_resp_append(const char *data, uint16_t len);
static void cli_resp_dispatch(void);

void UART_SetAppCallbacks(const uart_app_callbacks_t *cb)
{
    if (cb) {
        memcpy(&s_app_cb, cb, sizeof(uart_app_callbacks_t));
        s_app_cb_valid = true;
    } else {
        memset(&s_app_cb, 0, sizeof(uart_app_callbacks_t));
        s_app_cb_valid = false;
    }
}

void UART_ClearAppCallbacks(void)
{
    memset(&s_app_cb, 0, sizeof(uart_app_callbacks_t));
    s_app_cb_valid = false;
    g_pending_req = PENDING_NONE;
}

/*=============================================================================
 *  USART1 Configuration
 *=============================================================================*/

#define USART1_BAUDRATE     921600
#define USART1_TX_PIN       GPIO_Pin_9
#define USART1_TX_PORT      GPIOA
#define USART1_RX_PIN       GPIO_Pin_10
#define USART1_RX_PORT      GPIOA
#define USART1_GPIO_AF      GPIO_AF7

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
    /* Stream frame states (LEN=0xFF) */
    RX_STREAM_DATA,
    RX_STREAM_TT1,
    RX_STREAM_TT2,
    RX_STREAM_TT3,
} rx_state_t;

static volatile rx_state_t s_rx_state = RX_WAIT_HEAD;
static volatile uint8_t s_rx_src, s_rx_dst, s_rx_len, s_rx_cmd;
static volatile uint8_t s_rx_data[PROTO_MAX_DATA_LEN];
static volatile uint8_t s_rx_idx;
static volatile uint8_t s_rx_remaining;

/* Stream frame buffer (larger, allocated statically) */
#define STREAM_BUF_SIZE  2048
static volatile uint8_t s_stream_buf[STREAM_BUF_SIZE];
static volatile uint16_t s_stream_idx;

/* Ring buffer for ISR → poll communication */
#define RING_ENTRY_MAX  (4 + PROTO_MAX_DATA_LEN)  /* src+dst+cmd+dlen+data */
static volatile uint8_t s_ring[UART_RX_BUF_SIZE];
static volatile uint16_t s_ring_head = 0;
static volatile uint16_t s_ring_tail = 0;

/* Helper: push a completed frame into the ring buffer */
static void ring_push_frame(uint8_t src, uint8_t dst, uint8_t cmd,
                             const volatile uint8_t *data, uint8_t dlen)
{
    uint16_t total = 4 + dlen;
    /* Simple overflow check: if not enough space, drop the frame */
    if (s_ring_head + total - s_ring_tail > UART_RX_BUF_SIZE) return;

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
 *  USART1 Initialization
 *=============================================================================*/

void UART_Module_Init(void)
{
    printf("[UART_Module_Init] start\r\n");

    /* Init display state */
    memset(&g_disp_state, 0, sizeof(g_disp_state));
    g_disp_state.screen_on   = true;
    g_disp_state.brightness  = g_settings.backlight;
    g_disp_state.rotation    = 0;
    g_disp_state.auto_off_sec = g_settings.auto_off_enable ?
                                (uint16_t)g_settings.auto_off_min * 60 : 0;
    g_disp_state.last_activity_ms = ui_get_real_ms();

    /* Enable clocks */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | RCC_HB2Periph_GPIOA |
                           RCC_HB2Periph_USART1, ENABLE);

    /* TX pin: PA9 AF7 push-pull */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, USART1_GPIO_AF);
    GPIO_InitTypeDef gpio = {0};
    gpio.GPIO_Pin   = USART1_TX_PIN;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(USART1_TX_PORT, &gpio);

    /* RX pin: PA10 AF7 */
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, USART1_GPIO_AF);
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
    /* ---- Standard frame states ---- */
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
            /* Stream frame: scan for tail sequence */
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
            ring_push_frame(s_rx_src, s_rx_dst, s_rx_cmd, s_rx_data, dlen);
        }
        s_rx_state = RX_WAIT_HEAD;
        break;

    /* ---- Stream frame states (LEN=0xFF) ---- */
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
            /* False alarm: push TAIL0 + this byte back into stream data */
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
            /* Stream frame complete.
             * First byte of stream data is CMD, rest is payload.
             * For now, push as a standard frame with CMD from stream_buf[0]
             * and data from stream_buf[1..]. Capped at PROTO_MAX_DATA_LEN. */
            if (s_stream_idx >= 1) {
                s_rx_cmd = s_stream_buf[0];
                uint8_t dlen = (s_stream_idx - 1 < PROTO_MAX_DATA_LEN) ?
                                (uint8_t)(s_stream_idx - 1) : PROTO_MAX_DATA_LEN;
                /* Copy stream payload to s_rx_data for ring_push_frame */
                for (uint8_t i = 0; i < dlen; i++)
                    s_rx_data[i] = s_stream_buf[1 + i];
                ring_push_frame(s_rx_src, s_rx_dst, s_rx_cmd, s_rx_data, dlen);
            }
            s_rx_state = RX_WAIT_HEAD;
        } else {
            /* False alarm at last byte: push all 3 tail bytes + this byte */
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

/* Low-level: send one byte, wait for TXE */
static inline void tx_byte(uint8_t b)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, b);
}

void UART_SendFrame(uint8_t dst, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    uint8_t frame_len = 1 + len; /* CMD + DATA */
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
    UART_NotifyActivity();  /* Reset auto-off timer */

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
    g_settings.backlight = (uint8_t)((uint16_t)pct * 255 / 100);
    SSD1963_SetBacklight(g_settings.backlight);
}

static void handle_get_brightness(uint8_t src)
{
    uint8_t pct = g_disp_state.brightness;
    UART_SendACK(src, &pct, 1);
}

static void handle_show_page(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    /* Page ID mapping is application-specific.
     * For now, just log it. Apps can register handlers later. */
    (void)data[0];
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
    /* UI can read g_disp_state to update status bar widgets */
}

static void handle_set_rotation(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    g_disp_state.rotation = data[0];
    /* TODO: call LCD rotation API when implemented */
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
        SSD1963_SetBacklight(0);
        break;
    case SCREEN_CTRL_ON:
        g_disp_state.screen_on = true;
        SSD1963_SetBacklight(g_settings.backlight);
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

    if (len < 18) return;  /* priority(1) + title(16) + content(1+) */

    if (!s_notice_dlg_inited) {
        ui_dialog_init(&s_notice_dlg);
        s_notice_dlg_inited = 1;
    }

    /* 解析 title: data[1..16] */
    for (uint8_t i = 0; i < 16; i++) {
        s_notice_title[i] = (char)data[1 + i];
        if (data[1 + i] == '\0') break;
    }
    s_notice_title[16] = '\0';

    /* 解析 content: data[17..len-1] */
    uint8_t clen = len - 17;
    if (clen > sizeof(s_notice_content) - 1)
        clen = sizeof(s_notice_content) - 1;
    memcpy(s_notice_content, &data[17], clen);
    s_notice_content[clen] = '\0';

    ui_dialog_show(&s_notice_dlg, s_notice_title, s_notice_content, NULL, NULL);
}

static void handle_music_status(const uint8_t *data, uint8_t len)
{
    if (len < 10) return;
    g_disp_state.music_state  = data[0];
    g_disp_state.music_pos_ms = ((uint32_t)data[1] << 24) | ((uint32_t)data[2] << 16) |
                                 ((uint32_t)data[3] << 8)  |  (uint32_t)data[4];
    g_disp_state.music_dur_ms = ((uint32_t)data[5] << 24) | ((uint32_t)data[6] << 16) |
                                 ((uint32_t)data[7] << 8)  |  (uint32_t)data[8];
    g_disp_state.music_volume = data[9];
    /* Track name: data[10..len-1] */
    uint8_t name_len = (len > 10) ? (len - 10) : 0;
    if (name_len > sizeof(g_disp_state.music_track) - 1)
        name_len = sizeof(g_disp_state.music_track) - 1;
    memcpy(g_disp_state.music_track, &data[10], name_len);
    g_disp_state.music_track[name_len] = '\0';
    /* UI can read g_disp_state to update music player widgets */
}

static void handle_factory_reset(const uint8_t *data, uint8_t len)
{
    if (len < 1 || data[0] != 0xA5) return;  /* Confirm code required */
    settings_load_defaults();
    settings_apply();
    g_disp_state.brightness  = g_settings.backlight;
    g_disp_state.auto_off_sec = g_settings.auto_off_enable ?
                                 (uint16_t)g_settings.auto_off_min * 60 : 0;
    ui_page_invalidate_all();
}

/*=============================================================================
 *  Extended Command Handlers  (CMD=0x10, DATA[0]=sub-cmd)
 *=============================================================================*/

static void handle_ext_module_status(const uint8_t *data, uint8_t len)
{
    if (len < 5) return;  /* ext(1) + evt_type(1) + module_id(1) + type(1) + subtype(1) */
    /* uint8_t evt_type  = data[1]; */
    /* uint8_t module_id = data[2]; */
    /* uint8_t mod_type  = data[3]; */
    /* uint8_t mod_sub   = data[4]; */
    /* TODO: update UI module status display */
}

static void handle_ext_bt_event(const uint8_t *data, uint8_t len)
{
    if (len < 2) return;  /* ext(1) + evt_type(1) + ... */
    /* uint8_t evt_type = data[1]; */
    /* Parse based on evt_type (connected/disconnected/scan result) */
    /* TODO: update UI BT device list */
}

static void handle_ext_submodel_event(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;  /* ext(1) + subtype(1) + evt_op(1) + ... */
    /* uint8_t sub_type = data[1]; */
    /* uint8_t evt_op   = data[2]; */
    /* Route to appropriate UI handler based on sub_type */
    /* TODO: implement per-submodel UI updates */
}

static void handle_ext_power_event(const uint8_t *data, uint8_t len)
{
    if (len < 2) return;  /* ext(1) + evt_type(1) + ... */
    /* uint8_t evt_type = data[1]; */
    /* Update power status in g_disp_state or dedicated UI */
    /* TODO: implement power status UI */
}

static void handle_ext_config_result(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;  /* ext(1) + result(1) + error_code(1) */
    /* uint8_t result     = data[1];  0=success, 1=failure */
    /* uint8_t error_code = data[2]; */
    /* TODO: show config save/load result to user */
}

static void handle_ext_hid_status(const uint8_t *data, uint8_t len)
{
    if (len < 3) return;  /* ext_code(1) + evt(1) + dev_type(1) */
    /* data[0] = DISP_EXT_HID_STATUS (already consumed by dispatcher) */
    uint8_t evt      = data[1];  /* HID_EVT_CONNECTED or HID_EVT_DISCONNECTED */
    uint8_t dev_type = data[2];  /* HID_DEV_KEYBOARD or HID_DEV_MOUSE */

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
        /* CLI response from Core (text output).
         * Each frame from Core is a standalone chunk; accumulate in buffer
         * and dispatch immediately.  Core guarantees frame-level completeness. */
        if (len >= 2) {
            cli_resp_append((const char *)&data[1], len - 1);
            cli_resp_dispatch();
        }
        g_pending_req = PENDING_NONE;
        break;
    case DISP_EXT_CWD_NOTIFY:
        /* CWD change notification from Core (pushed on cd/device success).
         * DATA[1..N] = path string (null-terminated or len-delimited). */
        if (len >= 2 && s_app_cb_valid && s_app_cb.on_cwd_notify) {
            /* Ensure null-termination */
            char path_buf[64];
            uint8_t plen = len - 1;
            if (plen > sizeof(path_buf) - 1) plen = sizeof(path_buf) - 1;
            memcpy(path_buf, &data[1], plen);
            path_buf[plen] = '\0';
            s_app_cb.on_cwd_notify(path_buf);
        }
        break;
    case DISP_EXT_APP_LAUNCH:
    case DISP_EXT_APP_CLOSE:
    case DISP_EXT_APP_DATA:
    case DISP_EXT_SUBDISP_CONTENT:
    case DISP_EXT_SUBDISP_CONFIG:
        /* TODO: implement as features are needed */
        break;
    /* 0x06-0x0B, 0x14, 0x19: deprecated, fall through to NACK */
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
    /* Only process frames addressed to Display or broadcast */
    if (dst != MODULE_ID_DISPLAY && dst != 0xFF) return;

    switch (cmd) {
    /* --- Basic opcodes --- */
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

    /* --- Extended command dispatcher --- */
    case CMD_DISP_EXT:
        if (data_len >= 1) {
            process_extended_cmd(data[0], data, data_len, src);
        }
        break;

    /* --- Generic commands --- */
    case CMD_GET_TYPE: {
        /* Respond with Display module type info */
        uint8_t resp[5] = {
            MODULE_TYPE_DISPLAY,  /* type = Display */
            0x01,                 /* subtype = LCD */
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
        if (s_app_cb_valid && g_pending_req == PENDING_CLI && s_app_cb.on_cli_response) {
            s_cli_resp_buf[0] = '\0';
            s_cli_resp_len = 0;
            s_app_cb.on_cli_response("ERROR: NACK", 11, false, true);
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
    while (s_ring_tail != s_ring_head) {
        uint16_t available = s_ring_head - s_ring_tail;
        if (available < 4) break;

        uint8_t src  = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        uint8_t dst  = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        uint8_t cmd  = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        uint8_t dlen = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];

        if (s_ring_head - s_ring_tail < dlen) {
            s_ring_tail -= 4;  /* rewind */
            break;
        }

        uint8_t data[PROTO_MAX_DATA_LEN];
        for (uint8_t i = 0; i < dlen; i++) {
            data[i] = s_ring[s_ring_tail++ % UART_RX_BUF_SIZE];
        }

        process_frame(src, dst, cmd, data, dlen);
    }

    /* Auto screen-off check */
    if (g_disp_state.auto_off_sec > 0 && g_disp_state.screen_on) {
        uint32_t now = ui_get_real_ms();
        if (now - g_disp_state.last_activity_ms >
            (uint32_t)g_disp_state.auto_off_sec * 1000) {
            g_disp_state.screen_on = false;
            SSD1963_SetBacklight(0);
        }
    }
}

/*=============================================================================
 *  CLI Response Assembly — implementation
 *=============================================================================*/

/* Track the last CLI command sent, for context-aware parsing */
#define CLI_CMD_TAG_SIZE   32
static char s_cli_cmd_tag[CLI_CMD_TAG_SIZE];  /* e.g. "ls", "cd", "play" */

/* When UART_RequestFileList(path) is called with a non-empty path,
 * we first send "cd <path>" and set this flag. When the cd response
 * arrives, cli_resp_dispatch automatically sends "ls". */
static bool s_pending_ls_after_cd = false;

static void cli_resp_reset(void)
{
    s_cli_resp_len = 0;
    s_cli_resp_active = true;
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

/* Parse CLI "ls" output into file_entry_t array.
 * ls output format:
 *   --- \PATH ---
 *     [DIR]  dirname
 *     [FILE] filename  (12345 bytes)
 */
static void cli_resp_dispatch(void)
{
    s_cli_resp_buf[s_cli_resp_len] = '\0';
    s_cli_resp_active = false;

    if (!s_app_cb_valid) return;

    /* After "cd" response, auto-send "ls" if pending */
    if (strcmp(s_cli_cmd_tag, "cd") == 0 && s_pending_ls_after_cd) {
        s_pending_ls_after_cd = false;
        /* cd succeeded — now send ls to get the file list */
        UART_SendCLI("ls");
        return;
    }

    /* If on_file_list is set and command was "ls", parse the ls output */
    if (s_app_cb.on_file_list && strcmp(s_cli_cmd_tag, "ls") == 0) {
        file_entry_t entries[FILE_LIST_MAX_ENTRIES];
        uint8_t count = 0;
        const char *p = s_cli_resp_buf;

        /* Extract path from "--- \PATH ---" header line */
        const char *hdr_start = strstr(p, "--- ");
        if (hdr_start) {
            const char *path_start = hdr_start + 4; /* skip "--- " */
            const char *path_end = strstr(path_start, " ---");
            if (path_end) {
                uint8_t path_len = (uint8_t)(path_end - path_start);
                char ls_path[64];
                if (path_len > sizeof(ls_path) - 1) path_len = sizeof(ls_path) - 1;
                memcpy(ls_path, path_start, path_len);
                ls_path[path_len] = '\0';
                /* Notify app of the real CWD from ls output */
                if (s_app_cb.on_cwd_notify)
                    s_app_cb.on_cwd_notify(ls_path);
            }
        }

        while (*p && count < FILE_LIST_MAX_ENTRIES) {
            /* Skip to next line starting with [DIR] or [FILE] */
            const char *dir_tag = strstr(p, "[DIR]");
            const char *file_tag = strstr(p, "[FILE]");

            /* Find the earliest tag */
            const char *tag = NULL;
            uint8_t is_dir = 0;
            if (dir_tag && (!file_tag || dir_tag < file_tag)) {
                tag = dir_tag;
                is_dir = 1;
            } else if (file_tag) {
                tag = file_tag;
                is_dir = 0;
            }

            if (!tag) break;

            /* Skip past the tag */
            p = tag + (is_dir ? 5 : 6);
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t') p++;

            /* Extract name */
            uint8_t nlen = 0;
            while (*p && *p != '\r' && *p != '\n' && nlen < FILE_NAME_MAX_LEN) {
                entries[count].name[nlen++] = *p++;
            }
            entries[count].name[nlen] = '\0';

            /* Skip trailing whitespace in name */
            while (nlen > 0 && (entries[count].name[nlen-1] == ' ' ||
                                entries[count].name[nlen-1] == '\t')) {
                entries[count].name[--nlen] = '\0';
            }

            if (nlen == 0) continue;

            entries[count].attr = is_dir ? FILE_ATTR_IS_DIR : 0;

            /* Parse size for files: look for "(NNN bytes)" */
            entries[count].size = 0;
            if (!is_dir) {
                const char *size_start = strstr(entries[count].name, "(");
                if (size_start) {
                    /* Trim name at '(' */
                    uint8_t trim_pos = (uint8_t)(size_start - entries[count].name);
                    entries[count].name[trim_pos] = '\0';
                    /* Parse size */
                    /* Simple ASCII-to-int (no stdlib on embedded) */
                    {
                        uint32_t val = 0;
                        const char *sp = size_start + 1;
                        while (*sp >= '0' && *sp <= '9') {
                            val = val * 10 + (uint32_t)(*sp - '0');
                            sp++;
                        }
                        entries[count].size = val;
                    }
                }
            }

            /* Final trim: remove ALL trailing whitespace from name
             * (ls output may have multiple spaces before "(NNN bytes)") */
            {
                uint8_t nlen2 = (uint8_t)strlen(entries[count].name);
                while (nlen2 > 0 && (entries[count].name[nlen2-1] == ' ' ||
                                     entries[count].name[nlen2-1] == '\t')) {
                    entries[count].name[--nlen2] = '\0';
                }
            }

            count++;
        }

        s_app_cb.on_file_list(0x00, entries, count);
    } else if (s_app_cb.on_cli_response) {
        /* For all other commands, pass raw CLI text */
        s_app_cb.on_cli_response(s_cli_resp_buf, s_cli_resp_len, false, true);
    }
}

/*=============================================================================
 *  Display → Core Request Helpers (all via CLI passthrough)
 *=============================================================================*/

void UART_NotifyActivity(void)
{
    g_disp_state.last_activity_ms = ui_get_real_ms();
    /* Wake screen if off */
    if (!g_disp_state.screen_on) {
        g_disp_state.screen_on = true;
        SSD1963_SetBacklight(g_settings.backlight);
    }
}

void UART_SendCLI(const char *cmd)
{
    /* Extract command tag (first word) for context-aware parsing */
    {
        uint8_t i = 0;
        while (i < CLI_CMD_TAG_SIZE - 1 && cmd[i] && cmd[i] != ' ') {
            s_cli_cmd_tag[i] = cmd[i];
            i++;
        }
        s_cli_cmd_tag[i] = '\0';
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
        /* Two-step: send "cd <dir>" first, auto-follow with "ls" on response.
         * dir_or_null should be a single-level dir name (e.g. "DOC", "..", "\"). */
        s_pending_ls_after_cd = true;
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "cd \"%s\"", dir_or_null);
        UART_SendCLI(cmd);
    } else {
        /* Already in target directory, just list */
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
        /* Rename needs two args; caller should use UART_SendCLI directly */
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
    /* File save via CLI "write" command.
     * Note: this only sends the command header; the caller (editor)
     * typically builds the full "write <path> <content>" command
     * via UART_SendCLI directly to include file content.
     * This function is kept for API compatibility. */
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
