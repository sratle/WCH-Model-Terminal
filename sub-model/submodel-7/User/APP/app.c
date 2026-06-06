/**
 * @file app.c
 * @brief Submodel-7 应用层实现：命令处理 + 内容渲染 + Bulk 图片接收
 */
#include "app.h"
#include "../ST7305/st7305.h"
#include "../UI/ui_render.h"
#include "../Font/font_montserrat_12.h"
#include "../Font/ui_icons_12.h"
#include <string.h>

/*=============================================================================
 *  Global instances
 *=============================================================================*/

struct st7305_stu g_lcd;
static uint8_t g_framebuffer[FULL_BUFFER_LENGTH];

/** Raw image buffer for multi-frame bulk receive.
 *  Max image: 122x250 @ 1bpp = ceil(122/8)*250 = 4000 bytes. */
static uint8_t g_raw_img_buf[BULK_IMG_MAX_BYTES];

/** Multi-frame bulk transfer state */
bulk_transfer_t g_bulk;

/*=============================================================================
 *  Status state (cached for rendering)
 *=============================================================================*/

typedef struct {
    uint8_t  battery_pct;       /* 0~100 */
    uint8_t  bt_connected;      /* 0=disconnected, 1=connected */
    uint32_t timestamp;         /* Unix seconds */
    uint8_t  app_id;
    uint8_t  valid_flags;       /* Which fields are valid */
} app_status_t;

static app_status_t g_status;
static volatile uint8_t g_refresh_pending;
static volatile uint8_t g_bulk_ready;     /* Set when all bulk bytes received */

/* System status cache */
static sys_status_t g_sys_status;
static dev_list_t g_dev_list;

/* Multi-frame reassembly state */
static multiframe_rx_t g_mf_rx;

/* Page switching state (heartbeat-based) */
static uint8_t g_current_page = 0;        /* Current page index in status mode (0..2) */
static uint8_t g_display_mode = 0;        /* 0=status rotation, 1=image */
static uint8_t g_hb_count = 0;           /* Heartbeat counter since last page switch */
static uint8_t g_hb_status_count = 0;    /* Heartbeat counter since last status request */
static uint8_t g_status_requested = 0;   /* 1=initial status request not yet sent */

/* BMP image cache (from BMP_TRANS multi-frame) */
static uint8_t g_bmp_buf[BULK_IMG_MAX_BYTES];
static uint16_t g_bmp_width = 0;
static uint16_t g_bmp_height = 0;
static uint8_t g_bmp_valid = 0;

/*=============================================================================
 *  Forward declarations
 *=============================================================================*/

static void App_HandleFrame(void);
static void App_HandleGetType(void);
static void App_HandleSetMode(void);
static void App_HandleBulkTransfer(void);
static void App_HandleDataReport(void);
static void App_HandleGetStatus(void);
static void App_RenderStatus(void);
static void App_RenderContent(const uint8_t *data, uint8_t len);
static void App_ProcessBulkComplete(void);
static void App_DrawStatusBar(void);
static void App_RenderPage0(void);
static void App_RenderPage1(void);
static void App_RenderPage2(void);
static void App_RenderPageImage(void);
static void App_RequestSysStatus(void);
static void App_RequestLsDev(void);
static void App_MultiframeReset(void);
static void App_MultiframeProcess(uint8_t subcmd, uint8_t flags,
                                   const uint8_t *data, uint8_t data_len);

/*=============================================================================
 *  Init
 *=============================================================================*/

void App_Init(void)
{
    /* Initialize ST7305 display */
    st7305_init(&g_lcd, g_framebuffer);

    /* Initialize UI renderer */
    UI_Init();

    /* Clear screen to white */
    UI_Clear(UI_COLOR_WHITE);

    /* Initialize UART protocol */
    UartCore_Init();

    /* Clear state */
    memset(&g_status, 0, sizeof(g_status));
    memset(&g_sys_status, 0, sizeof(g_sys_status));
    memset(&g_dev_list, 0, sizeof(g_dev_list));
    memset(&g_mf_rx, 0, sizeof(g_mf_rx));
    g_refresh_pending = 0;
    g_bulk_ready = 0;
    g_current_page = 0;
    g_hb_count = 0;
    g_hb_status_count = 0;
    g_status_requested = 0;
    g_bmp_valid = 0;

    /* Draw initial page 0 (status) */
    App_RenderPage0();
    UI_Refresh();
}

/*=============================================================================
 *  ISR entry point (called from USART1_IRQHandler - now unused,
 *  ISR directly calls Protocol_ParseByte for simplicity)
 *=============================================================================*/

void App_UART_ByteReceived(uint8_t byte)
{
    /* Kept for API compatibility; ISR now directly feeds protocol parser */
    Protocol_ParseByte(&uart_core_rx_ctx, byte);
}

/*=============================================================================
 *  Main loop processing
 *=============================================================================*/

void App_Process(void)
{
    /* Handle completed protocol frames */
    if (uart_core_rx_ctx.frame_ready)
    {
        App_HandleFrame();
        Protocol_ResetRxCtx(&uart_core_rx_ctx);
    }

    /* Handle bulk transfer completion (all chunks received) */
    if (g_bulk_ready)
    {
        App_ProcessBulkComplete();
        g_bulk_ready = 0;
    }

    /* Coalesced refresh: if content updated, refresh screen */
    if (g_refresh_pending)
    {
        UI_Refresh();
        g_refresh_pending = 0;

        /* Enter low-power mode after refresh */
        st7305_power_low(&g_lcd);
    }
}

/*=============================================================================
 *  Frame dispatcher
 *=============================================================================*/

static void App_HandleFrame(void)
{
    protocol_frame_t *f = &uart_core_rx_ctx.frame;

    switch (f->cmd)
    {
        case CMD_NOP:
            /* Heartbeat, no action */
            break;

        case CMD_GET_TYPE:
            App_HandleGetType();
            break;

        case CMD_SUB_SET_MODE:
            App_HandleSetMode();
            break;

        case CMD_SUB_BULK_TRANSFER:
            App_HandleBulkTransfer();
            break;

        case CMD_SUB_GET_STATUS:
            App_HandleGetStatus();
            break;

        case CMD_SUB_DATA_REPORT:
            App_HandleDataReport();
            break;

        default:
            /* Unsupported command: send NACK */
            {
                uint8_t err = PROTO_ERR_UNSUPPORTED_CMD;
                UartCore_PackAndSend(MODULE_ID_CORE, CMD_NACK, &err, 1);
            }
            break;
    }
}

/*=============================================================================
 *  CMD_GET_TYPE handler
 *=============================================================================*/

static void App_HandleGetType(void)
{
    uint8_t rsp[5];

    rsp[0] = MODULE_TYPE_SUBMODEL;          /* 0x05 */
    rsp[1] = MODULE_SUBTYPE_SUB_DISPLAY;    /* 0x07 */
    rsp[2] = FW_HW_VERSION;
    rsp[3] = FW_MAJOR_VERSION;
    rsp[4] = FW_MINOR_VERSION;

    UartCore_PackAndSend(MODULE_ID_CORE, CMD_ACK, rsp, 5);

    /* Heartbeat-based timing */
    g_hb_count++;
    g_hb_status_count++;

    /* Page switching: every HB_PAGE_SWITCH_COUNT heartbeats */
    if (g_hb_count >= HB_PAGE_SWITCH_COUNT)
    {
        g_hb_count = 0;

        /* 仅在状态模式(mode 0)下自动翻页；图片模式(mode 1)不翻页 */
        if (g_display_mode == 0)
        {
            g_current_page = (g_current_page + 1) % PAGE_COUNT;
            App_RenderStatus();
            g_refresh_pending = 1;
        }
    }

    /* Status request: first after HB_STATUS_INIT_COUNT, then every HB_STATUS_REQUEST_COUNT */
    if (!g_status_requested)
    {
        if (g_hb_status_count >= HB_STATUS_INIT_COUNT)
        {
            g_status_requested = 1;
            g_hb_status_count = 0;
            App_RequestSysStatus();
        }
    }
    else
    {
        if (g_hb_status_count >= HB_STATUS_REQUEST_COUNT)
        {
            g_hb_status_count = 0;
            App_RequestSysStatus();
        }
    }
}

/*=============================================================================
 *  CMD_SUB_SET_MODE (0x41) handler
 *=============================================================================*/

static void App_HandleSetMode(void)
{
    protocol_frame_t *f = &uart_core_rx_ctx.frame;
    uint8_t sub_cmd;
    uint8_t *data;
    uint8_t data_len;

    if (f->len < 2)
        return; /* Need at least SUB command byte */

    sub_cmd  = f->data[0];
    data     = &f->data[1];
    data_len = f->len - 2;

    switch (sub_cmd)
    {
        case SUBCMD_SET_STATUS:
        {
            /* Status data: DATA[0]=flags, then optional fields */
            if (data_len < 1)
                break;

            uint8_t flags = data[0];
            uint8_t idx = 1;

            g_status.valid_flags = flags;

            if ((flags & STATUS_FLAG_BATTERY) && idx < data_len + 1)
            {
                g_status.battery_pct = data[idx++];
            }
            if ((flags & STATUS_FLAG_BLUETOOTH) && idx < data_len + 1)
            {
                g_status.bt_connected = data[idx++];
            }
            if ((flags & STATUS_FLAG_TIME) && (idx + 3) < data_len + 1)
            {
                g_status.timestamp = ((uint32_t)data[idx] << 24)
                                   | ((uint32_t)data[idx+1] << 16)
                                   | ((uint32_t)data[idx+2] << 8)
                                   | (uint32_t)data[idx+3];
                idx += 4;
            }
            if ((flags & STATUS_FLAG_APP_ID) && idx < data_len + 1)
            {
                g_status.app_id = data[idx++];
            }

            /* Render status layout */
            App_RenderStatus();
            g_refresh_pending = 1;
            break;
        }

        case SUBCMD_SET_CONTENT:
        {
            /* Custom content: DATA[0]=content_type, rest=content_data */
            if (data_len < 1)
                break;

            App_RenderContent(data, data_len);
            g_refresh_pending = 1;
            break;
        }

        case SUBCMD_CLEAR_SCREEN:
        {
            UI_Clear(UI_COLOR_WHITE);
            g_refresh_pending = 1;
            break;
        }

        case SUBCMD_BMP_TRANS:
        {
            /* Multi-frame BMP transfer: data[0]=FLAGS, data[1..N]=payload */
            if (data_len < 1)
                break;

            {
                uint8_t flags = data[0];
                const uint8_t *payload = &data[1];
                uint8_t payload_len = data_len - 1;

                App_MultiframeProcess(SUBCMD_BMP_TRANS, flags, payload, payload_len);
            }
            break;
        }

        case SUBCMD_LS_DEV:
        {
            /* Multi-frame device list: data[0]=FLAGS, data[1..N]=payload */
            if (data_len < 1)
                break;

            {
                uint8_t flags = data[0];
                const uint8_t *payload = &data[1];
                uint8_t payload_len = data_len - 1;

                App_MultiframeProcess(SUBCMD_LS_DEV, flags, payload, payload_len);
            }
            break;
        }

        case SUBCMD_SET_DISPLAY_MODE:
        {
            /* data[0]=mode: 0=status rotation, 1=image */
            if (data_len < 1)
                break;

            uint8_t mode = data[0];
            if (mode == DISPLAY_MODE_IMAGE)
            {
                /* 切换到图片模式：停止自动翻页，显示图片 */
                g_display_mode = 1;
                g_hb_count = 0;
                App_RenderPageImage();
                g_refresh_pending = 1;
            }
            else
            {
                /* 切换到状态模式：恢复三页轮换 */
                g_display_mode = 0;
                g_current_page = 0;
                g_hb_count = 0;
                App_RenderPage0();
                g_refresh_pending = 1;
            }
            break;
        }

        default:
            break;
    }
}

/*=============================================================================
 *  CMD_SUB_BULK_TRANSFER (0x46) handler
 *=============================================================================*/

static void App_HandleBulkTransfer(void)
{
    protocol_frame_t *f = &uart_core_rx_ctx.frame;
    uint8_t sub_cmd;

    if (f->len < 2)
        return;

    sub_cmd = f->data[0];

    switch (sub_cmd)
    {
        case SUBCMD_BULK_HANDSHAKE:
        {
            /* DATA: [sub:1][width:2][height:2][total_bytes:4] */
            uint8_t *d = &f->data[1];
            uint8_t d_len = f->len - 2;

            if (d_len < 8)
                break;

            uint16_t img_w = ((uint16_t)d[0] << 8) | d[1];
            uint16_t img_h = ((uint16_t)d[2] << 8) | d[3];
            uint32_t total = ((uint32_t)d[4] << 24) | ((uint32_t)d[5] << 16)
                           | ((uint32_t)d[6] << 8)  | (uint32_t)d[7];

            /* Sanity check */
            if (total > BULK_IMG_MAX_BYTES)
                break;

            /* Initialize bulk transfer state */
            g_bulk.img_width    = img_w;
            g_bulk.img_height   = img_h;
            g_bulk.total_bytes  = total;
            g_bulk.received     = 0;
            g_bulk.active       = 1;

            /* ACK the handshake */
            UartCore_PackAndSend(MODULE_ID_CORE, CMD_ACK, NULL, 0);
            break;
        }

        case SUBCMD_BULK_DATA:
        {
            /* DATA: [sub:1][chunk_data:0..252]
             * Each frame carries up to 252 bytes of raw image data.
             * Bytes are accumulated into g_raw_img_buf at offset g_bulk.received. */
            if (!g_bulk.active)
                break;

            {
                const uint8_t *chunk = &f->data[1];
                uint8_t chunk_len = f->len - 2; /* DATA bytes minus sub_cmd */
                uint32_t remaining = g_bulk.total_bytes - g_bulk.received;
                uint8_t copy_len;
                uint32_t i;

                if (chunk_len > remaining)
                    chunk_len = (uint8_t)remaining;

                copy_len = chunk_len;
                for (i = 0; i < copy_len; i++)
                    g_raw_img_buf[g_bulk.received + i] = chunk[i];

                g_bulk.received += copy_len;

                /* Check if transfer is complete */
                if (g_bulk.received >= g_bulk.total_bytes)
                {
                    g_bulk_ready = 1;
                }
            }
            break;
        }

        default:
            break;
    }
}

/*=============================================================================
 *  Bulk completion processing
 *=============================================================================*/

static void App_ProcessBulkComplete(void)
{
    uint16_t img_w = g_bulk.img_width;
    uint16_t img_h = g_bulk.img_height;
    uint32_t total = g_bulk.total_bytes;
    uint16_t bytes_per_row;
    uint16_t row, col;

    /* Mark bulk transfer as done */
    g_bulk.active = 0;

    /* Raw 1bpp data is in g_raw_img_buf[0..total-1].
     * Convert to ST7305 format in g_framebuffer pixel by pixel. */
    bytes_per_row = (img_w + 7) / 8;

    /* Re-init lcd pointer */
    g_lcd.full_buffer = g_framebuffer;

    /* Clear framebuffer to white (ST7305 format) */
    memset(g_framebuffer, 0x00, FULL_BUFFER_LENGTH);

    /* Convert raw 1bpp -> ST7305 pixel format */
    for (row = 0; row < img_h && row < UI_SCREEN_HEIGHT; row++)
    {
        for (col = 0; col < img_w && col < UI_SCREEN_WIDTH; col++)
        {
            uint16_t byte_idx = row * bytes_per_row + (col / 8);
            uint8_t  bit = 0x80 >> (col % 8);

            if (byte_idx < total && (g_raw_img_buf[byte_idx] & bit))
            {
                st7305_buf_draw_pix(&g_lcd, col, row, UI_COLOR_BLACK);
            }
        }
    }

    /* Send completion confirmation to Core */
    {
        uint8_t rsp[2];
        rsp[0] = SUBCMD_BULK_COMPLETE;
        rsp[1] = 0x00;  /* result: 0=success */
        UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_BULK_TRANSFER, rsp, 2);
    }

    /* Refresh screen */
    UI_Refresh();
    st7305_power_low(&g_lcd);
}

/*=============================================================================
 *  Status Layout Renderer
 *=============================================================================*/

static void App_DrawStatusBar(void)
{
    /* Top bar background */
    UI_FillRect(0, 0, UI_SCREEN_WIDTH, LAYOUT_STATUS_BAR_H, UI_COLOR_BLACK);

    /* Battery icon + percentage (left side) */
    if (g_sys_status.valid && g_sys_status.battery_pct != 0xFF)
    {
        const uint8_t *bat_icon;
        uint8_t pct = g_sys_status.battery_pct;

        if (pct > 75)
            bat_icon = icon_battery_full_12_bitmap;
        else if (pct > 50)
            bat_icon = icon_battery_3_12_bitmap;
        else if (pct > 25)
            bat_icon = icon_battery_2_12_bitmap;
        else if (pct > 5)
            bat_icon = icon_battery_1_12_bitmap;
        else
            bat_icon = icon_battery_empty_12_bitmap;

        UI_DrawIcon(2, 4, bat_icon,
                    ICON_BATTERY_FULL_12_WIDTH, ICON_BATTERY_FULL_12_HEIGHT,
                    UI_COLOR_WHITE);

        {
            char buf[6];
            buf[0] = '0' + (char)(pct / 100); pct %= 100;
            buf[1] = '0' + (char)(pct / 10); pct %= 10;
            buf[2] = '0' + (char)pct;
            buf[3] = '%';
            buf[4] = '\0';
            UI_DrawString(16, 5, buf, UI_COLOR_WHITE, &font_montserrat_12);
        }
    }
    else
    {
        UI_DrawIcon(2, 4, icon_battery_full_12_bitmap,
                    ICON_BATTERY_FULL_12_WIDTH, ICON_BATTERY_FULL_12_HEIGHT,
                    UI_COLOR_WHITE);
        UI_DrawString(16, 5, "--%", UI_COLOR_WHITE, &font_montserrat_12);
    }

    /* Bluetooth icon (right side) */
    UI_DrawIcon(UI_SCREEN_WIDTH - 14, 4, icon_bluetooth_12_bitmap,
                ICON_BLUETOOTH_12_WIDTH, ICON_BLUETOOTH_12_HEIGHT,
                UI_COLOR_WHITE);

    /* Audio icon (center) */
    UI_DrawIcon(UI_SCREEN_WIDTH / 2 - 6, 4, icon_audio_12_bitmap,
                ICON_AUDIO_12_WIDTH, ICON_AUDIO_12_HEIGHT,
                UI_COLOR_WHITE);

    /* Separator line */
    UI_DrawHLine(0, LAYOUT_STATUS_BAR_H - 1, UI_SCREEN_WIDTH, UI_COLOR_BLACK);
}

/*=============================================================================
 *  Page 0: Audio / Battery / BLE
 *=============================================================================*/

static void App_DrawPageIndicator(uint8_t page_num, uint8_t total, const char *label)
{
    char buf[16];
    int16_t text_w;
    /* "1/3 Label" */
    buf[0] = '0' + page_num;
    buf[1] = '/';
    buf[2] = '0' + total;
    buf[3] = ' ';
    /* Append label */
    {
        uint8_t i;
        for (i = 0; label[i] && i + 4 < sizeof(buf) - 1; i++)
            buf[4 + i] = label[i];
        buf[4 + i] = '\0';
    }
    text_w = UI_GetStringWidth(buf, &font_montserrat_12);
    if (text_w > UI_SCREEN_WIDTH - 4) text_w = UI_SCREEN_WIDTH - 4;
    UI_FillRect(0, UI_SCREEN_HEIGHT - 14, UI_SCREEN_WIDTH, 14, UI_COLOR_BLACK);
    UI_DrawString((UI_SCREEN_WIDTH - text_w) / 2, UI_SCREEN_HEIGHT - 13,
                  buf, UI_COLOR_WHITE, &font_montserrat_12);
}

static void App_RenderPage0(void)
{
    char buf[20];
    int16_t x, y;

    UI_Clear(UI_COLOR_WHITE);
    App_DrawStatusBar();

    y = LAYOUT_STATUS_BAR_H + 4;

    /* ---- Audio ---- */
    UI_DrawIcon(4, y, icon_audio_12_bitmap,
                ICON_AUDIO_12_WIDTH, ICON_AUDIO_12_HEIGHT, UI_COLOR_BLACK);
    {
        const char *audio_str;
        switch (g_sys_status.audio_state)
        {
            case 1: audio_str = "PLAY"; break;
            case 2: audio_str = "PAUSE"; break;
            case 3: audio_str = "STOP"; break;
            default: audio_str = "IDLE"; break;
        }
        UI_DrawString(18, y + 1, audio_str, UI_COLOR_BLACK, &font_montserrat_12);
    }

    /* Volume on the right side */
    {
        uint8_t vol = g_sys_status.audio_volume;
        if (vol > 100) vol = 100;
        buf[0] = 'V'; buf[1] = ':';
        if (vol >= 100) { buf[2] = '1'; vol -= 100; }
        else { buf[2] = ' '; }
        buf[3] = '0' + (char)(vol / 10); vol %= 10;
        buf[4] = '0' + (char)vol;
        buf[5] = '\0';
        /* Right-align volume text */
        x = UI_SCREEN_WIDTH - 4 - UI_GetStringWidth(buf, &font_montserrat_12);
        UI_DrawString(x, y + 1, buf, UI_COLOR_BLACK, &font_montserrat_12);
    }

    y += 16;
    /* Volume bar full width */
    {
        uint8_t vol = g_sys_status.audio_volume;
        if (vol > 100) vol = 100;
        UI_DrawRect(4, y, UI_SCREEN_WIDTH - 8, 8, UI_COLOR_BLACK);
        if (vol > 0)
            UI_FillRect(5, y + 1, (uint16_t)vol * (UI_SCREEN_WIDTH - 10) / 100, 6, UI_COLOR_BLACK);
    }

    y += 14;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 5;

    /* ---- Battery ---- */
    {
        uint8_t bat = g_sys_status.battery_pct;
        if (bat == 0xFF)
            UI_DrawString(4, y, "BAT: ---", UI_COLOR_BLACK, &font_montserrat_12);
        else
        {
            buf[0] = 'B'; buf[1] = 'A'; buf[2] = 'T'; buf[3] = ':';
            buf[4] = ' ';
            if (bat >= 100) { buf[5] = '1'; bat -= 100; }
            else { buf[5] = ' '; }
            buf[6] = '0' + (char)(bat / 10); bat %= 10;
            buf[7] = '0' + (char)bat;
            buf[8] = '%';
            buf[9] = '\0';
            UI_DrawString(4, y, buf, UI_COLOR_BLACK, &font_montserrat_12);
        }
        /* Charging indicator */
        if (g_sys_status.charging)
            UI_DrawString(UI_SCREEN_WIDTH - 4 - UI_GetStringWidth("CHG", &font_montserrat_12),
                          y, "CHG", UI_COLOR_BLACK, &font_montserrat_12);
    }

    y += 16;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 5;

    /* ---- BLE ---- */
    {
        buf[0] = 'B'; buf[1] = 'L'; buf[2] = 'E'; buf[3] = ':';
        buf[4] = '0' + (g_sys_status.ble_connections & 0x0F);
        buf[5] = '\0';
        UI_DrawString(4, y, buf, UI_COLOR_BLACK, &font_montserrat_12);
    }
    {
        const char *disc = g_sys_status.ble_discoverable ? "SCAN" : "OFF";
        x = UI_SCREEN_WIDTH - 4 - UI_GetStringWidth(disc, &font_montserrat_12);
        UI_DrawString(x, y, disc, UI_COLOR_BLACK, &font_montserrat_12);
    }

    /* Page indicator */
    App_DrawPageIndicator(1, PAGE_COUNT, "Audio");
}

/*=============================================================================
 *  Page 1: Storage / Display / RGB / Config
 *=============================================================================*/

static void App_RenderPage1(void)
{
    char buf[20];
    int16_t x, y;

    UI_Clear(UI_COLOR_WHITE);
    App_DrawStatusBar();

    y = LAYOUT_STATUS_BAR_H + 4;

    /* ---- Storage ---- */
    UI_DrawIcon(4, y, icon_usb_12_bitmap,
                ICON_USB_12_WIDTH, ICON_USB_12_HEIGHT, UI_COLOR_BLACK);
    UI_DrawString(18, y + 1,
                  g_sys_status.ch378_device == 1 ? "SD" : "USB",
                  UI_COLOR_BLACK, &font_montserrat_12);
    {
        const char *busy_str = g_sys_status.ch378_busy ? "BUSY" : "RDY";
        x = UI_SCREEN_WIDTH - 4 - UI_GetStringWidth(busy_str, &font_montserrat_12);
        UI_DrawString(x, y + 1, busy_str, UI_COLOR_BLACK, &font_montserrat_12);
    }

    y += 16;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 5;

    /* ---- Display Brightness ---- */
    {
        uint8_t br = g_sys_status.display_brightness;
        if (br > 100) br = 100;
        buf[0] = 'D'; buf[1] = 'I'; buf[2] = 'S'; buf[3] = 'P'; buf[4] = ':';
        if (br >= 100) { buf[5] = '1'; br -= 100; }
        else { buf[5] = ' '; }
        buf[6] = '0' + (char)(br / 10); br %= 10;
        buf[7] = '0' + (char)br;
        buf[8] = '%';
        buf[9] = '\0';
        UI_DrawString(4, y, buf, UI_COLOR_BLACK, &font_montserrat_12);
    }

    y += 16;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 5;

    /* ---- Keyboard Backlight ---- */
    {
        uint8_t kb = g_sys_status.keyboard_backlight;
        if (kb > 100) kb = 100;
        buf[0] = 'K'; buf[1] = 'B'; buf[2] = 'D'; buf[3] = ':';
        if (kb >= 100) { buf[4] = '1'; kb -= 100; }
        else { buf[4] = ' '; }
        buf[5] = '0' + (char)(kb / 10); kb %= 10;
        buf[6] = '0' + (char)kb;
        buf[7] = '%';
        buf[8] = '\0';
        UI_DrawString(4, y, buf, UI_COLOR_BLACK, &font_montserrat_12);
    }

    y += 16;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 5;

    /* ---- RGB ---- */
    {
        const char *rgb_modes[] = {"FIX", "ON", "BRH", "RUN"};
        const char *mode_str = (g_sys_status.rgb_mode < 4)
                               ? rgb_modes[g_sys_status.rgb_mode] : "???";
        buf[0] = 'R'; buf[1] = 'G'; buf[2] = 'B'; buf[3] = ':';
        buf[4] = mode_str[0]; buf[5] = mode_str[1]; buf[6] = mode_str[2];
        buf[7] = '\0';
        UI_DrawString(4, y, buf, UI_COLOR_BLACK, &font_montserrat_12);
    }
    {
        uint8_t rb = g_sys_status.rgb_brightness;
        buf[0] = 'B'; buf[1] = ':';
        buf[2] = '0' + (char)(rb / 100); rb %= 100;
        buf[3] = '0' + (char)(rb / 10); rb %= 10;
        buf[4] = '0' + (char)rb;
        buf[5] = '\0';
        x = UI_SCREEN_WIDTH - 4 - UI_GetStringWidth(buf, &font_montserrat_12);
        UI_DrawString(x, y, buf, UI_COLOR_BLACK, &font_montserrat_12);
    }

    y += 16;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 5;

    /* ---- Config ---- */
    UI_DrawString(4, y,
                  g_sys_status.config_loaded ? "CFG: Loaded" : "CFG: None",
                  UI_COLOR_BLACK, &font_montserrat_12);

    /* Page indicator */
    App_DrawPageIndicator(2, PAGE_COUNT, "System");
}

/*=============================================================================
 *  Page 2: Module Status List
 *=============================================================================*/

static void App_RenderPage2(void)
{
    int16_t y;

    UI_Clear(UI_COLOR_WHITE);
    App_DrawStatusBar();

    y = LAYOUT_STATUS_BAR_H + 2;

    UI_DrawString(4, y + 1, "Modules", UI_COLOR_BLACK, &font_montserrat_12);
    y += 15;
    UI_DrawHLine(4, y, UI_SCREEN_WIDTH - 8, UI_COLOR_BLACK);
    y += 3;

    if (g_dev_list.valid)
    {
        uint8_t i;
        static const char *mod_names[] = {
            "DSP", "KBD", "PWR", "S1", "S2", "S3"
        };

        for (i = 0; i < g_dev_list.count && i < SYS_STATUS_MAX_MODULES; i++)
        {
            const char *name = (i < 6) ? mod_names[i] : "???";
            uint8_t st = g_dev_list.entries[i].status;

            /* Status dot */
            if (st == 1)
                UI_FillCircle(8, y + 5, 3, UI_COLOR_BLACK);
            else
                UI_DrawCircle(8, y + 5, 3, UI_COLOR_BLACK);

            /* Name */
            UI_DrawString(16, y, (char *)name, UI_COLOR_BLACK, &font_montserrat_12);

            /* Status text */
            {
                const char *st_str = st == 1 ? "ON" : (st == 2 ? "OFF" : "???");
                int16_t st_x = UI_SCREEN_WIDTH - 4 - UI_GetStringWidth(st_str, &font_montserrat_12);
                UI_DrawString(st_x, y, st_str, UI_COLOR_BLACK, &font_montserrat_12);
            }

            y += 14;
            if (y > UI_SCREEN_HEIGHT - 16) break;
        }
    }
    else
    {
        UI_DrawString(16, y, "Waiting...", UI_COLOR_BLACK, &font_montserrat_12);
    }

    /* Page indicator */
    App_DrawPageIndicator(3, PAGE_COUNT, "Modules");
}

/*=============================================================================
 *  Image Page (shown when mode=IMAGE and bmp is valid)
 *=============================================================================*/

static void App_RenderPageImage(void)
{
    UI_Clear(UI_COLOR_WHITE);
    App_DrawStatusBar();

    if (g_bmp_valid)
    {
        int16_t img_y = LAYOUT_STATUS_BAR_H;
        int16_t avail_h = UI_SCREEN_HEIGHT - img_y;
        uint16_t bytes_per_row = (g_bmp_width + 7) / 8;
        uint16_t row, col;

        int16_t x_off = 0;
        if (g_bmp_width < UI_SCREEN_WIDTH)
            x_off = (UI_SCREEN_WIDTH - g_bmp_width) / 2;

        for (row = 0; row < g_bmp_height && row < avail_h; row++)
        {
            for (col = 0; col < g_bmp_width && col < UI_SCREEN_WIDTH; col++)
            {
                uint16_t byte_idx = row * bytes_per_row + (col / 8);
                uint8_t  bit = 0x80 >> (col % 8);

                if (byte_idx < BULK_IMG_MAX_BYTES && (g_bmp_buf[byte_idx] & bit))
                {
                    UI_DrawPixel(x_off + col, img_y + row, UI_COLOR_BLACK);
                }
            }
        }
    }
    else
    {
        /* No image available */
        int16_t y = LAYOUT_STATUS_BAR_H + 4;
        UI_DrawString(4, y, "No Image", UI_COLOR_BLACK, &font_montserrat_12);
    }
}

static void App_RenderStatus(void)
{
    /* Render current status page */
    switch (g_current_page)
    {
        case 0: App_RenderPage0(); break;
        case 1: App_RenderPage1(); break;
        case 2: App_RenderPage2(); break;
        default: App_RenderPage0(); break;
    }
}

/*=============================================================================
 *  Custom Content Renderer
 *=============================================================================*/

static void App_RenderContent(const uint8_t *data, uint8_t len)
{
    uint8_t content_type;

    if (len < 1)
        return;

    content_type = data[0];

    /* Clear content area */
    UI_FillRect(0, LAYOUT_STATUS_BAR_H, UI_SCREEN_WIDTH, LAYOUT_CONTENT_H,
                UI_COLOR_WHITE);

    switch (content_type)
    {
        case CONTENT_TYPE_TEXT:
        {
            if (len > 1)
            {
                char text_buf[64];
                uint8_t copy_len = (len - 1 < sizeof(text_buf) - 1)
                                   ? (len - 1) : (sizeof(text_buf) - 1);
                uint8_t i;
                for (i = 0; i < copy_len; i++)
                    text_buf[i] = (char)data[1 + i];
                text_buf[copy_len] = '\0';

                UI_DrawString(4, LAYOUT_CONTENT_Y + 4, text_buf,
                              UI_COLOR_BLACK, &font_montserrat_12);
            }
            break;
        }

        case CONTENT_TYPE_ICON:
        {
            if (len >= 6)
            {
                const uint8_t *icon_bmp = NULL;
                int16_t icon_w = 0, icon_h = 0;
                int16_t ix = ((int16_t)data[2] << 8) | data[3];
                int16_t iy = ((int16_t)data[4] << 8) | data[5];

                switch (data[1])
                {
                    case 0x01: icon_bmp = icon_home_12_bitmap;
                               icon_w = ICON_HOME_12_WIDTH;
                               icon_h = ICON_HOME_12_HEIGHT; break;
                    case 0x02: icon_bmp = icon_settings_12_bitmap;
                               icon_w = ICON_SETTINGS_12_WIDTH;
                               icon_h = ICON_SETTINGS_12_HEIGHT; break;
                    case 0x03: icon_bmp = icon_bluetooth_12_bitmap;
                               icon_w = ICON_BLUETOOTH_12_WIDTH;
                               icon_h = ICON_BLUETOOTH_12_HEIGHT; break;
                    case 0x04: icon_bmp = icon_wifi_12_bitmap;
                               icon_w = ICON_WIFI_12_WIDTH;
                               icon_h = ICON_WIFI_12_HEIGHT; break;
                    case 0x05: icon_bmp = icon_audio_12_bitmap;
                               icon_w = ICON_AUDIO_12_WIDTH;
                               icon_h = ICON_AUDIO_12_HEIGHT; break;
                    case 0x06: icon_bmp = icon_envelope_12_bitmap;
                               icon_w = ICON_ENVELOPE_12_WIDTH;
                               icon_h = ICON_ENVELOPE_12_HEIGHT; break;
                    case 0x07: icon_bmp = icon_bell_12_bitmap;
                               icon_w = ICON_BELL_12_WIDTH;
                               icon_h = ICON_BELL_12_HEIGHT; break;
                    case 0x08: icon_bmp = icon_power_12_bitmap;
                               icon_w = ICON_POWER_12_WIDTH;
                               icon_h = ICON_POWER_12_HEIGHT; break;
                    default: break;
                }

                if (icon_bmp != NULL)
                {
                    UI_DrawIcon(ix, iy, icon_bmp, icon_w, icon_h,
                                UI_COLOR_BLACK);
                }
            }
            break;
        }

        case CONTENT_TYPE_PROGRESS:
        {
            if (len >= 8)
            {
                uint8_t pct = data[1];
                int16_t px = ((int16_t)data[2] << 8) | data[3];
                int16_t py = ((int16_t)data[4] << 8) | data[5];
                int16_t pw = ((int16_t)data[6] << 8) | data[7];
                int16_t bar_h = 8;
                int16_t fill_w;

                if (pct > 100) pct = 100;
                fill_w = (int16_t)((int32_t)pw * pct / 100);

                UI_DrawRect(px, py, pw, bar_h, UI_COLOR_BLACK);
                if (fill_w > 0)
                    UI_FillRect(px + 1, py + 1, fill_w - 2, bar_h - 2,
                                UI_COLOR_BLACK);
            }
            break;
        }

        case CONTENT_TYPE_SHAPE:
        {
            if (len >= 10)
            {
                int16_t sx = ((int16_t)data[2] << 8) | data[3];
                int16_t sy = ((int16_t)data[4] << 8) | data[5];
                int16_t sw = ((int16_t)data[6] << 8) | data[7];
                int16_t sh = ((int16_t)data[8] << 8) | data[9];

                if (data[1] == 0x00)
                    UI_DrawRect(sx, sy, sw, sh, UI_COLOR_BLACK);
                else
                    UI_FillRect(sx, sy, sw, sh, UI_COLOR_BLACK);
            }
            break;
        }

        default:
            break;
    }
}

/*=============================================================================
 *  CMD_SUB_GET_STATUS (0x42) handler
 *=============================================================================*/

static void App_HandleGetStatus(void)
{
    protocol_frame_t *f = &uart_core_rx_ctx.frame;
    uint8_t subcmd = (f->len >= 2) ? f->data[0] : 0;

    /* ACK the request */
    UartCore_PackAndSend(MODULE_ID_CORE, CMD_ACK, NULL, 0);

    /* Core will send data asynchronously after ACK */
    (void)subcmd;
}

/*=============================================================================
 *  CMD_SUB_DATA_REPORT (0x43) handler
 *=============================================================================*/

static void App_HandleDataReport(void)
{
    protocol_frame_t *f = &uart_core_rx_ctx.frame;
    uint8_t subcmd = (f->len >= 1) ? f->data[0] : 0;
    uint8_t flags = (f->len >= 2) ? f->data[1] : 0;
    const uint8_t *payload = (f->len >= 2) ? &f->data[2] : NULL;
    uint8_t payload_len = (f->len >= 2) ? f->len - 2 : 0;

    switch (subcmd)
    {
        case SUBCMD_SYS_STATUS:
            App_MultiframeProcess(SUBCMD_SYS_STATUS, flags, payload, payload_len);
            break;
        case SUBCMD_LS_DEV:
            App_MultiframeProcess(SUBCMD_LS_DEV, flags, payload, payload_len);
            break;
        case SUBCMD_BMP_TRANS:
            App_MultiframeProcess(SUBCMD_BMP_TRANS, flags, payload, payload_len);
            break;
        default:
            break;
    }
}

/*=============================================================================
 *  Multi-frame Reassembly
 *=============================================================================*/

static void App_MultiframeReset(void)
{
    g_mf_rx.active = 0;
    g_mf_rx.len = 0;
    g_mf_rx.subcmd = 0;
}

static void App_MultiframeProcess(uint8_t subcmd, uint8_t flags,
                                   const uint8_t *data, uint8_t data_len)
{
    if (flags & SUBDISP_FLAG_SOF)
    {
        /* Start of new multi-frame transfer */
        g_mf_rx.active = 1;
        g_mf_rx.len = 0;
        g_mf_rx.subcmd = subcmd;
    }

    if (!g_mf_rx.active)
        return;

    /* Append data to buffer */
    if (data != NULL && data_len > 0)
    {
        uint16_t space = MULTIFRAME_BUF_SIZE - g_mf_rx.len;
        uint8_t copy_len = (data_len > space) ? (uint8_t)space : data_len;
        uint8_t i;
        for (i = 0; i < copy_len; i++)
            g_mf_rx.buf[g_mf_rx.len + i] = data[i];
        g_mf_rx.len += copy_len;
    }

    /* Check if transfer is complete */
    if (flags & SUBDISP_FLAG_EOF)
    {
        g_mf_rx.active = 0;

        /* Process complete data based on subcmd */
        switch (g_mf_rx.subcmd)
        {
            case SUBCMD_BMP_TRANS:
            {
                /* First 8 bytes: width(2) + height(2) + total_size(4) */
                if (g_mf_rx.len < 8)
                    break;

                g_bmp_width  = ((uint16_t)g_mf_rx.buf[0] << 8) | g_mf_rx.buf[1];
                g_bmp_height = ((uint16_t)g_mf_rx.buf[2] << 8) | g_mf_rx.buf[3];

                if (g_bmp_width == 0)
                {
                    /* Failed transfer */
                    g_bmp_valid = 0;
                    break;
                }

                /* Copy image data (after 8-byte header) */
                {
                    uint16_t img_data_len = g_mf_rx.len - 8;
                    if (img_data_len > BULK_IMG_MAX_BYTES)
                        img_data_len = BULK_IMG_MAX_BYTES;
                    uint16_t i;
                    for (i = 0; i < img_data_len; i++)
                        g_bmp_buf[i] = g_mf_rx.buf[8 + i];
                }
                g_bmp_valid = 1;

                /* Switch to image mode */
                g_display_mode = 1;
                g_hb_count = 0;
                App_RenderPageImage();
                g_refresh_pending = 1;
                break;
            }

            case SUBCMD_LS_DEV:
            {
                /* First byte: module count, then 5-byte entries */
                if (g_mf_rx.len < 1)
                    break;

                g_dev_list.count = g_mf_rx.buf[0];
                if (g_dev_list.count > SYS_STATUS_MAX_MODULES)
                    g_dev_list.count = SYS_STATUS_MAX_MODULES;

                {
                    uint8_t i;
                    for (i = 0; i < g_dev_list.count; i++)
                    {
                        uint16_t off = 1 + i * 5;
                        if (off + 5 > g_mf_rx.len)
                            break;
                        g_dev_list.entries[i].module_id  = g_mf_rx.buf[off];
                        g_dev_list.entries[i].type       = g_mf_rx.buf[off + 1];
                        g_dev_list.entries[i].subtype    = g_mf_rx.buf[off + 2];
                        g_dev_list.entries[i].status     = g_mf_rx.buf[off + 3];
                        g_dev_list.entries[i].miss_count = g_mf_rx.buf[off + 4];
                    }
                }
                g_dev_list.valid = 1;
                break;
            }

            case SUBCMD_SYS_STATUS:
            {
                /* Parse system status data */
                const uint8_t *d = g_mf_rx.buf;
                uint16_t dlen = g_mf_rx.len;
                uint16_t off = 0;

                if (dlen < 20)
                    break;

                g_sys_status.version           = d[off++];
                g_sys_status.audio_state       = d[off++];
                g_sys_status.audio_volume      = d[off++];
                g_sys_status.audio_play_time_ms = ((uint32_t)d[off] << 24)
                                                 | ((uint32_t)d[off+1] << 16)
                                                 | ((uint32_t)d[off+2] << 8)
                                                 | (uint32_t)d[off+3];
                off += 4;
                g_sys_status.battery_pct       = d[off++];
                g_sys_status.charging          = d[off++];
                g_sys_status.ble_connections    = d[off++];
                g_sys_status.ble_discoverable  = d[off++];
                g_sys_status.display_brightness = d[off++];
                g_sys_status.keyboard_backlight = d[off++];
                g_sys_status.ch378_device      = d[off++];
                g_sys_status.ch378_busy        = d[off++];
                g_sys_status.ch9350_hid_count  = d[off++];
                g_sys_status.rgb_mode          = d[off++];
                g_sys_status.rgb_brightness    = d[off++];
                g_sys_status.config_loaded     = d[off++];
                g_sys_status.module_count      = d[off++];

                /* Parse module entries */
                if (g_sys_status.module_count > SYS_STATUS_MAX_MODULES)
                    g_sys_status.module_count = SYS_STATUS_MAX_MODULES;

                {
                    uint8_t i;
                    for (i = 0; i < g_sys_status.module_count; i++)
                    {
                        if (off + 5 > dlen) break;
                        g_sys_status.modules[i].module_id  = d[off++];
                        g_sys_status.modules[i].type       = d[off++];
                        g_sys_status.modules[i].subtype    = d[off++];
                        g_sys_status.modules[i].status     = d[off++];
                        g_sys_status.modules[i].miss_count = d[off++];
                    }
                }

                g_sys_status.valid = 1;

                /* Also update dev_list from module entries */
                g_dev_list.count = g_sys_status.module_count;
                {
                    uint8_t i;
                    for (i = 0; i < g_dev_list.count; i++)
                    {
                        g_dev_list.entries[i].module_id  = g_sys_status.modules[i].module_id;
                        g_dev_list.entries[i].type       = g_sys_status.modules[i].type;
                        g_dev_list.entries[i].subtype    = g_sys_status.modules[i].subtype;
                        g_dev_list.entries[i].status     = g_sys_status.modules[i].status;
                        g_dev_list.entries[i].miss_count = g_sys_status.modules[i].miss_count;
                    }
                }
                g_dev_list.valid = 1;

                /* Refresh current page if in status mode */
                if (g_display_mode == 0)
                {
                    App_RenderStatus();
                    g_refresh_pending = 1;
                }
                break;
            }

            default:
                break;
        }
    }
}

/*=============================================================================
 *  Request Functions (SubDisp → Core)
 *=============================================================================*/

static void App_RequestSysStatus(void)
{
    uint8_t data[1] = { SUBCMD_GET_SYS_STATUS };
    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_GET_STATUS, data, 1);
}

static void App_RequestLsDev(void)
{
    uint8_t data[1] = { SUBCMD_GET_LS_DEV };
    UartCore_PackAndSend(MODULE_ID_CORE, CMD_SUB_GET_STATUS, data, 1);
}
