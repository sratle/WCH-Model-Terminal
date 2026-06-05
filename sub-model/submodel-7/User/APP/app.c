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

/*=============================================================================
 *  Forward declarations
 *=============================================================================*/

static void App_HandleFrame(void);
static void App_HandleGetType(void);
static void App_HandleSetMode(void);
static void App_HandleBulkTransfer(void);
static void App_RenderStatus(void);
static void App_RenderContent(const uint8_t *data, uint8_t len);
static void App_ProcessBulkComplete(void);
static void App_DrawStatusBar(void);

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
    g_refresh_pending = 0;
    g_bulk_ready = 0;

    /* Draw default idle screen */
    App_DrawStatusBar();
    UI_DrawString(10, 100, "SubDisp Ready", UI_COLOR_BLACK, &font_montserrat_12);
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
        {
            /* Report current status back */
            uint8_t rsp[6];
            rsp[0] = g_status.battery_pct;
            rsp[1] = g_status.bt_connected;
            rsp[2] = (g_status.timestamp >> 24) & 0xFF;
            rsp[3] = (g_status.timestamp >> 16) & 0xFF;
            rsp[4] = (g_status.timestamp >> 8) & 0xFF;
            rsp[5] = g_status.timestamp & 0xFF;
            UartCore_PackAndSend(MODULE_ID_CORE, CMD_ACK, rsp, 6);
            break;
        }

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
    data_len = f->len - 2; /* LEN includes CMD, but data[] starts after CMD already
                            * f->len = CMD(1) + DATA(N), so DATA has (f->len - 1) bytes
                            * data = &f->data[1], so data_len = (f->len - 1) - 1 */

    /* Recalculate: f->len = 1(CMD) + N(DATA bytes)
     * f->data[] has (f->len - 1) bytes
     * sub_cmd = f->data[0]
     * remaining = f->data[1..len-2], count = f->len - 2 */

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

    /* Battery icon + percentage */
    if (g_status.valid_flags & STATUS_FLAG_BATTERY)
    {
        const uint8_t *bat_icon;

        /* Select battery icon based on level */
        if (g_status.battery_pct > 75)
            bat_icon = icon_battery_full_12_bitmap;
        else if (g_status.battery_pct > 50)
            bat_icon = icon_battery_3_12_bitmap;
        else if (g_status.battery_pct > 25)
            bat_icon = icon_battery_2_12_bitmap;
        else if (g_status.battery_pct > 5)
            bat_icon = icon_battery_1_12_bitmap;
        else
            bat_icon = icon_battery_empty_12_bitmap;

        /* Draw battery icon (white on black bar) at left */
        UI_DrawIcon(2, 4, bat_icon,
                    ICON_BATTERY_FULL_12_WIDTH, ICON_BATTERY_FULL_12_HEIGHT,
                    UI_COLOR_WHITE);

        /* Draw percentage text */
        {
            char buf[8];
            uint8_t pct = g_status.battery_pct;
            uint8_t idx = 0;
            if (pct >= 100) { buf[idx++] = '1'; pct -= 100; }
            if (pct >= 10 || idx > 0) { buf[idx++] = '0' + (char)(pct / 10); pct %= 10; }
            buf[idx++] = '0' + (char)pct;
            buf[idx++] = '%';
            buf[idx] = '\0';
            UI_DrawString(20, 5, buf, UI_COLOR_WHITE, &font_montserrat_12);
        }
    }

    /* Bluetooth icon */
    if (g_status.valid_flags & STATUS_FLAG_BLUETOOTH)
    {
        uint8_t color = g_status.bt_connected ? UI_COLOR_WHITE : UI_COLOR_WHITE;
        UI_DrawIcon(UI_SCREEN_WIDTH - 14, 4, icon_bluetooth_12_bitmap,
                    ICON_BLUETOOTH_12_WIDTH, ICON_BLUETOOTH_12_HEIGHT,
                    color);
    }

    /* Separator line */
    UI_DrawHLine(0, LAYOUT_STATUS_BAR_H, UI_SCREEN_WIDTH, UI_COLOR_BLACK);
}

static void App_RenderStatus(void)
{
    char time_buf[16];

    /* Clear content area */
    UI_FillRect(0, LAYOUT_STATUS_BAR_H, UI_SCREEN_WIDTH, LAYOUT_CONTENT_H,
                UI_COLOR_WHITE);

    /* Draw status bar */
    App_DrawStatusBar();

    /* Draw time in center */
    if (g_status.valid_flags & STATUS_FLAG_TIME)
    {
        /* Convert Unix timestamp to HH:MM (simple modulo) */
        uint32_t t = g_status.timestamp;
        uint32_t hours = (t / 3600) % 24;
        uint32_t minutes = (t / 60) % 60;

        /* Manual "HH:MM" formatting (avoid snprintf for newlib-nano) */
        time_buf[0] = '0' + (char)(hours / 10);
        time_buf[1] = '0' + (char)(hours % 10);
        time_buf[2] = ':';
        time_buf[3] = '0' + (char)(minutes / 10);
        time_buf[4] = '0' + (char)(minutes % 10);
        time_buf[5] = '\0';

        /* Center the time string */
        int16_t tw = UI_GetStringWidth(time_buf, &font_montserrat_12);
        int16_t tx = (UI_SCREEN_WIDTH - tw) / 2;
        if (tx < 0) tx = 0;

        UI_DrawString(tx, 80, time_buf, UI_COLOR_BLACK, &font_montserrat_12);
    }

    /* Draw date below time */
    if (g_status.valid_flags & STATUS_FLAG_TIME)
    {
        uint32_t t = g_status.timestamp;
        /* Simple date calculation (days since epoch) */
        uint32_t days = t / 86400;
        uint32_t year = 1970;
        uint32_t month = 1, day = 1;

        /* Approximate year from days since epoch */
        while (days >= 365)
        {
            if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
            {
                if (days >= 366) { days -= 366; year++; }
                else break;
            }
            else
            {
                days -= 365;
                year++;
            }
        }

        /* Simple month/day from remaining days */
        {
            static const uint8_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            uint8_t m;
            for (m = 0; m < 12; m++)
            {
                uint8_t md = mdays[m];
                if (m == 1 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
                    md = 29;
                if (days < md) { month = m + 1; day = days + 1; break; }
                days -= md;
            }
        }

        /* Manual "YYYY-MM-DD" formatting */
        time_buf[0] = '0' + (char)((year / 1000) % 10);
        time_buf[1] = '0' + (char)((year / 100) % 10);
        time_buf[2] = '0' + (char)((year / 10) % 10);
        time_buf[3] = '0' + (char)(year % 10);
        time_buf[4] = '-';
        time_buf[5] = '0' + (char)(month / 10);
        time_buf[6] = '0' + (char)(month % 10);
        time_buf[7] = '-';
        time_buf[8] = '0' + (char)(day / 10);
        time_buf[9] = '0' + (char)(day % 10);
        time_buf[10] = '\0';

        int16_t tw = UI_GetStringWidth(time_buf, &font_montserrat_12);
        int16_t tx = (UI_SCREEN_WIDTH - tw) / 2;
        if (tx < 0) tx = 0;

        UI_DrawString(tx, 110, time_buf, UI_COLOR_BLACK, &font_montserrat_12);
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
            /* DATA[1..N]: null-terminated text string */
            if (len > 1)
            {
                /* Ensure null termination */
                char text_buf[64];
                uint8_t copy_len = (len - 1 < sizeof(text_buf) - 1)
                                   ? (len - 1) : (sizeof(text_buf) - 1);
                uint8_t i;
                for (i = 0; i < copy_len; i++)
                    text_buf[i] = (char)data[1 + i];
                text_buf[copy_len] = '\0';

                /* Draw text in content area */
                UI_DrawString(4, LAYOUT_CONTENT_Y + 4, text_buf,
                              UI_COLOR_BLACK, &font_montserrat_12);
            }
            break;
        }

        case CONTENT_TYPE_ICON:
        {
            /* DATA[1]: icon_id, DATA[2..3]: x, DATA[4..5]: y */
            if (len >= 6)
            {
                /* Use predefined icons based on icon_id */
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
            /* DATA[1]: percentage (0~100), DATA[2..3]: x, DATA[4..5]: y,
             * DATA[6..7]: width */
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

                /* Draw progress bar */
                UI_DrawRect(px, py, pw, bar_h, UI_COLOR_BLACK);
                if (fill_w > 0)
                    UI_FillRect(px + 1, py + 1, fill_w - 2, bar_h - 2,
                                UI_COLOR_BLACK);
            }
            break;
        }

        case CONTENT_TYPE_SHAPE:
        {
            /* DATA[1]: shape_type (0=rect, 1=filled_rect)
             * DATA[2..3]: x, DATA[4..5]: y, DATA[6..7]: w, DATA[8..9]: h */
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
