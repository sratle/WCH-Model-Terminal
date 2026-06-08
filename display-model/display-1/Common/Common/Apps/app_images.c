/********************************** (C) COPYRIGHT *******************************
* File Name          : app_images.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : Image browser app (V2.0).
*                      Browse BMP files via "bmp ls" CLI command.
*                      Fetch and decode BMP via "bmp get <filename>" hex dump.
*                      Supports 24bpp, 8bpp, 1bpp BMP rendering.
*                      Scaled preview with centering in right panel.
*                      Touch/keyboard file navigation.
********************************************************************************/
#include "app_images.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define IMG_LIST_W          250
#define IMG_LIST_X          8
#define IMG_LIST_Y          (APP_TITLE_BAR_H + 4)
#define IMG_LIST_H          (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - 38)
#define IMG_ITEM_H          34
#define IMG_VISIBLE_ITEMS   (IMG_LIST_H / IMG_ITEM_H)

#define IMG_PREVIEW_X       (IMG_LIST_W + 8)
#define IMG_PREVIEW_Y       (APP_TITLE_BAR_H + 4)
#define IMG_PREVIEW_W       (UI_SCREEN_WIDTH - IMG_LIST_W - 16)
#define IMG_PREVIEW_H       (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - 38)

/* Status bar */
#define IMG_BAR_Y           (UI_SCREEN_HEIGHT - 30)
#define IMG_BAR_H           30

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define IMG_BG              UI_COLOR_BG_MAIN
#define IMG_LIST_BG         UI_COLOR_BG_CARD
#define IMG_LIST_SEL        UI_HEX(0xE0F2F1)
#define IMG_LIST_ALT        UI_HEX(0xFAFAFA)
#define IMG_PREVIEW_BG      UI_HEX(0x2C2C2C)
#define IMG_PREVIEW_BORDER  UI_HEX(0x444444)
#define IMG_FILE_TEXT       UI_COLOR_TEXT_PRIMARY
#define IMG_SIZE_TEXT       UI_COLOR_TEXT_SECONDARY
#define IMG_BAR_BG          UI_HEX(0xE8E8E8)
#define IMG_BAR_TEXT         UI_HEX(0x555555)
#define IMG_BORDER          UI_HEX(0xE0E0E0)
#define IMG_PLACEHOLDER     UI_HEX(0x888888)

/*=============================================================================
 *  BMP Constants
 *=============================================================================*/

#define BMP_HEADER_SIZE     14
#define BMP_INFO_SIZE       40
#define BMP_FILE_HEADER_TOTAL (BMP_HEADER_SIZE + BMP_INFO_SIZE)

/* BMP compression */
#define BMP_BI_RGB          0
#define BMP_BI_RLE8         1
#define BMP_BI_RLE4         2

/*=============================================================================
 *  State
 *=============================================================================*/

#define IMG_MAX_FILES       32
#define IMG_CLI_BUF_SIZE    32768
#define IMG_BMP_BUF_SIZE    10240   /* Binary BMP data buffer (supports up to 10KB BMP) */

typedef struct {
    char     name[24];
    uint32_t size;
} img_file_t;

static ui_app_page_t s_app_img;

/* File list */
static img_file_t s_files[IMG_MAX_FILES];
static uint8_t  s_file_count;
static int16_t  s_file_scroll;
static int16_t  s_file_selected;      /* -1 = none */

/* CLI text buffer */
static char    s_cli_buf[IMG_CLI_BUF_SIZE];
static uint16_t s_cli_len;
static bool    s_cli_assembling;
static bool    s_cli_expect_ls;       /* expecting bmp ls */
static bool    s_cli_expect_get;      /* expecting bmp get */

/* BMP binary data */
static uint8_t s_bmp_data[IMG_BMP_BUF_SIZE];
static uint16_t s_bmp_data_len;

/* Parsed BMP info */
static int32_t  s_bmp_width;
static int32_t  s_bmp_height;
static uint16_t s_bmp_bpp;
static uint32_t s_bmp_data_offset;   /* Offset to pixel data in s_bmp_data */
static bool     s_bmp_top_down;       /* true if height < 0 (top-down bitmap) */
static bool     s_bmp_loaded;         /* true if valid BMP is loaded */

/* Status bar message */
static char s_bar_msg[80];

/* Widgets */
static ui_widget_t s_list_touch;
static ui_widget_t *s_img_widgets[2 + 2];  /* back_btn + title + list_touch + ... */

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void img_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last);
static void img_parse_bmp_ls(const char *text, uint16_t len);
static void img_decode_bmp(void);
static void img_render_preview(ui_rect_t *dirty);
static void img_set_bar(const char *msg);
static void img_invalidate_list(void);
static void img_invalidate_preview(void);

static uart_app_callbacks_t s_img_callbacks = {
    .on_cli_response = img_on_cli_response,
    .on_file_list    = NULL,
    .on_cwd_notify   = NULL,
};

/*=============================================================================
 *  Hex Utilities
 *=============================================================================*/

static uint8_t hex_digit(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return 0xFF;
}

/* Parse hex text ("42 4D 36 ...") to binary.
 * Returns number of bytes written. */
static uint16_t hex_to_binary(const char *hex, uint16_t hex_len,
                              uint8_t *out, uint16_t max_out)
{
    uint16_t out_pos = 0;
    uint16_t i = 0;

    while (i < hex_len && out_pos < max_out) {
        /* Skip whitespace and non-hex chars */
        while (i < hex_len) {
            char c = hex[i];
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
                break;
            i++;
        }
        if (i >= hex_len) break;

        uint8_t hi = hex_digit(hex[i++]);
        if (hi == 0xFF) continue;

        /* Find next hex digit for low nibble */
        while (i < hex_len) {
            char c = hex[i];
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
                break;
            i++;
        }
        if (i >= hex_len) break;

        uint8_t lo = hex_digit(hex[i++]);
        if (lo == 0xFF) continue;

        out[out_pos++] = (hi << 4) | lo;
    }

    return out_pos;
}

/* Read uint32 LE from buffer */
static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Read uint16 LE from buffer */
static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/* Read int32 LE from buffer */
static int32_t read_i32_le(const uint8_t *p)
{
    return (int32_t)read_u32_le(p);
}

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void img_set_bar(const char *msg)
{
    strncpy(s_bar_msg, msg, sizeof(s_bar_msg) - 1);
    s_bar_msg[sizeof(s_bar_msg) - 1] = '\0';
    ui_rect_t r = {0, IMG_BAR_Y, UI_SCREEN_WIDTH, IMG_BAR_H};
    ui_page_invalidate(&r);
}

static void img_invalidate_list(void)
{
    ui_rect_t r = {IMG_LIST_X, IMG_LIST_Y, IMG_LIST_W, IMG_LIST_H};
    ui_page_invalidate(&r);
}

static void img_invalidate_preview(void)
{
    ui_rect_t r = {IMG_PREVIEW_X, IMG_PREVIEW_Y, IMG_PREVIEW_W, IMG_PREVIEW_H};
    ui_page_invalidate(&r);
}

static void img_clear_files(void)
{
    s_file_count = 0;
    s_file_scroll = 0;
    s_file_selected = -1;
    img_invalidate_list();
}

static void img_request_ls(void)
{
    img_clear_files();
    s_cli_expect_ls = true;
    s_cli_expect_get = false;
    UART_SendCLI("bmp ls");
    img_set_bar("Loading BMP files...");
}

static void img_request_get(const char *filename)
{
    s_bmp_loaded = false;
    s_bmp_data_len = 0;
    s_cli_expect_get = true;
    s_cli_expect_ls = false;
    s_cli_len = 0;
    s_cli_assembling = false;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "bmp get %s", filename);
    UART_SendCLI(cmd);

    char msg[60];
    snprintf(msg, sizeof(msg), "Loading %s...", filename);
    img_set_bar(msg);
    img_invalidate_preview();
}

/*=============================================================================
 *  CLI Response
 *=============================================================================*/

static void img_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    if (!s_cli_assembling) {
        s_cli_len = 0;
        s_cli_assembling = true;
    }

    uint16_t space = IMG_CLI_BUF_SIZE - 1 - s_cli_len;
    uint16_t copy_len = (len < space) ? len : space;
    if (copy_len > 0) {
        memcpy(&s_cli_buf[s_cli_len], output, copy_len);
        s_cli_len += copy_len;
    }
    s_cli_buf[s_cli_len] = '\0';

    if (!is_last)
        return;

    s_cli_assembling = false;

    if (s_cli_expect_ls) {
        img_parse_bmp_ls(s_cli_buf, s_cli_len);
        s_cli_expect_ls = false;
    } else if (s_cli_expect_get) {
        /* Skip status line ("BMP: xxx size=xxx bytes\r\n"), parse hex dump only */
        const char *hex_start = s_cli_buf;
        const char *buf_end = s_cli_buf + s_cli_len;
        /* Find first newline — hex data starts after it */
        while (hex_start < buf_end && *hex_start != '\n') hex_start++;
        if (hex_start < buf_end) hex_start++;  /* skip the \n */

        uint16_t hex_len = (uint16_t)(buf_end - hex_start);
        s_bmp_data_len = hex_to_binary(hex_start, hex_len,
                                        s_bmp_data, IMG_BMP_BUF_SIZE);
        if (s_bmp_data_len >= BMP_FILE_HEADER_TOTAL) {
            img_decode_bmp();
        } else {
            img_set_bar("BMP data too small or transfer incomplete");
            s_bmp_loaded = false;
            img_invalidate_preview();
        }
        s_cli_expect_get = false;
    } else {
        char msg[78];
        uint16_t slen = s_cli_len;
        if (slen > 77) slen = 77;
        memcpy(msg, s_cli_buf, slen);
        msg[slen] = '\0';
        for (uint16_t i = 0; i < slen; i++) {
            if (msg[i] == '\r' || msg[i] == '\n') msg[i] = ' ';
        }
        img_set_bar(msg);
    }
}

static void img_parse_bmp_ls(const char *text, uint16_t len)
{
    img_clear_files();

    const char *p = text;
    const char *end = text + len;

    while (p < end && s_file_count < IMG_MAX_FILES) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n')
            eol++;

        char line[128];
        uint16_t llen = (uint16_t)(eol - p);
        if (llen > 127) llen = 127;
        memcpy(line, p, llen);
        line[llen] = '\0';

        const char *file_tag = strstr(line, "[FILE]");
        if (file_tag) {
            const char *name_start = file_tag + 6;
            while (*name_start == ' ') name_start++;

            int nlen = 0;
            while (name_start[nlen] && name_start[nlen] != '(' && nlen < 23) {
                s_files[s_file_count].name[nlen] = name_start[nlen];
                nlen++;
            }
            while (nlen > 0 && s_files[s_file_count].name[nlen - 1] == ' ')
                nlen--;
            s_files[s_file_count].name[nlen] = '\0';

            const char *paren = strstr(line, "(");
            if (paren) {
                s_files[s_file_count].size = (uint32_t)atol(paren + 1);
            }
            s_file_count++;
        }

        p = eol;
        while (p < end && (*p == '\r' || *p == '\n'))
            p++;
    }

    if (s_file_count > 0) {
        char msg[40];
        snprintf(msg, sizeof(msg), "Found %d BMP file(s)", s_file_count);
        img_set_bar(msg);
    } else {
        img_set_bar("No BMP files in \\BMP directory");
    }
    img_invalidate_list();
}

/*=============================================================================
 *  BMP Decode
 *=============================================================================*/

static void img_decode_bmp(void)
{
    if (s_bmp_data_len < BMP_FILE_HEADER_TOTAL) {
        s_bmp_loaded = false;
        img_set_bar("BMP data incomplete");
        img_invalidate_preview();
        return;
    }

    /* Verify BMP magic */
    if (s_bmp_data[0] != 'B' || s_bmp_data[1] != 'M') {
        s_bmp_loaded = false;
        img_set_bar("Not a valid BMP file");
        img_invalidate_preview();
        return;
    }

    /* Parse file header */
    /* uint32_t file_size = read_u32_le(&s_bmp_data[2]); */
    s_bmp_data_offset = read_u32_le(&s_bmp_data[10]);

    /* Parse info header */
    /* uint32_t info_size = read_u32_le(&s_bmp_data[14]); */
    s_bmp_width  = read_i32_le(&s_bmp_data[18]);
    s_bmp_height = read_i32_le(&s_bmp_data[22]);
    /* uint16_t planes = read_u16_le(&s_bmp_data[26]); */
    s_bmp_bpp    = read_u16_le(&s_bmp_data[28]);
    /* uint32_t compression = read_u32_le(&s_bmp_data[30]); */

    /* Handle top-down bitmaps (negative height) */
    s_bmp_top_down = false;
    if (s_bmp_height < 0) {
        s_bmp_height = -s_bmp_height;
        s_bmp_top_down = true;
    }

    /* Validate */
    if (s_bmp_width <= 0 || s_bmp_height <= 0 || s_bmp_data_offset >= s_bmp_data_len) {
        s_bmp_loaded = false;
        img_set_bar("Invalid BMP dimensions");
        img_invalidate_preview();
        return;
    }

    if (s_bmp_bpp != 1 && s_bmp_bpp != 8 && s_bmp_bpp != 24) {
        s_bmp_loaded = false;
        char msg[40];
        snprintf(msg, sizeof(msg), "Unsupported BPP: %d", s_bmp_bpp);
        img_set_bar(msg);
        img_invalidate_preview();
        return;
    }

    s_bmp_loaded = true;

    char msg[80];
    snprintf(msg, sizeof(msg), "%ldx%ld %dbpp (%d bytes loaded)",
             (long)s_bmp_width, (long)s_bmp_height, s_bmp_bpp, s_bmp_data_len);
    img_set_bar(msg);
    img_invalidate_preview();
}

/*=============================================================================
 *  BMP Rendering
 *=============================================================================*/

static void img_render_preview(ui_rect_t *dirty)
{
    /* Preview area background */
    ui_rect_t preview_bg = {IMG_PREVIEW_X, IMG_PREVIEW_Y, IMG_PREVIEW_W, IMG_PREVIEW_H};
    ui_draw_fill_rect(&preview_bg, IMG_PREVIEW_BG);
    ui_draw_rect_border(&preview_bg, IMG_PREVIEW_BORDER, 1);

    if (!s_bmp_loaded) {
        /* Placeholder text */
        const char *msg = s_bmp_data_len > 0 ? "Decode error" : "Select an image";
        int16_t tw = ui_text_width(msg, &font_montserrat_16);
        ui_draw_text(IMG_PREVIEW_X + (IMG_PREVIEW_W - tw) / 2,
                     IMG_PREVIEW_Y + IMG_PREVIEW_H / 2 - 8,
                     msg, &font_montserrat_16, IMG_PLACEHOLDER);
        return;
    }

    /* Calculate scale and centered position */
    int32_t scale = 1;
    while (s_bmp_width * (scale + 1) <= IMG_PREVIEW_W - 20 &&
           s_bmp_height * (scale + 1) <= IMG_PREVIEW_H - 20 &&
           scale < 16) {
        scale++;
    }

    int16_t render_w = (int16_t)(s_bmp_width * scale);
    int16_t render_h = (int16_t)(s_bmp_height * scale);
    int16_t ox = IMG_PREVIEW_X + (IMG_PREVIEW_W - render_w) / 2;
    int16_t oy = IMG_PREVIEW_Y + (IMG_PREVIEW_H - render_h) / 2;

    /* Calculate row stride for BMP data */
    uint32_t row_stride;
    if (s_bmp_bpp == 24)
        row_stride = ((uint32_t)s_bmp_width * 3 + 3) & ~3u;
    else if (s_bmp_bpp == 8)
        row_stride = ((uint32_t)s_bmp_width + 3) & ~3u;
    else /* 1bpp */
        row_stride = (((uint32_t)s_bmp_width + 31) / 32) * 4;

    /* Palette pointer for 8bpp (starts at offset 54 in BMP file) */
    const uint8_t *palette = NULL;
    if (s_bmp_bpp == 8 && s_bmp_data_offset >= 54 + 256 * 4) {
        palette = &s_bmp_data[54];
    }

    /* Render pixel rows */
    for (int32_t img_y = 0; img_y < s_bmp_height; img_y++) {
        /* BMP row index in data (bottom-up by default) */
        int32_t data_row = s_bmp_top_down ? img_y : (s_bmp_height - 1 - img_y);
        uint32_t row_offset = s_bmp_data_offset + (uint32_t)data_row * row_stride;

        /* Screen Y range for this image row */
        int16_t sy_start = oy + (int16_t)(img_y * scale);
        int16_t sy_end = sy_start + (int16_t)scale;

        /* Skip if row doesn't intersect dirty region */
        if (sy_end <= dirty->y || sy_start >= dirty->y + dirty->h)
            continue;
        /* Skip if completely out of preview area */
        if (sy_end <= IMG_PREVIEW_Y || sy_start >= IMG_PREVIEW_Y + IMG_PREVIEW_H)
            continue;

        for (int32_t img_x = 0; img_x < s_bmp_width; img_x++) {
            int16_t sx_start = ox + (int16_t)(img_x * scale);
            int16_t sx_end = sx_start + (int16_t)scale;

            /* Skip if column doesn't intersect dirty region */
            if (sx_end <= dirty->x || sx_start >= dirty->x + dirty->w)
                continue;

            ui_color_t pixel_color;

            if (s_bmp_bpp == 24) {
                uint32_t byte_offset = row_offset + (uint32_t)img_x * 3;
                if (byte_offset + 2 < s_bmp_data_len) {
                    uint8_t b_val = s_bmp_data[byte_offset];
                    uint8_t g_val = s_bmp_data[byte_offset + 1];
                    uint8_t r_val = s_bmp_data[byte_offset + 2];
                    pixel_color = UI_RGB(r_val, g_val, b_val);
                } else {
                    pixel_color = UI_COLOR_BLACK;
                }
            }
            else if (s_bmp_bpp == 8) {
                uint32_t byte_offset = row_offset + (uint32_t)img_x;
                if (byte_offset < s_bmp_data_len && palette != NULL) {
                    uint8_t idx = s_bmp_data[byte_offset];
                    uint8_t b_val = palette[idx * 4 + 0];
                    uint8_t g_val = palette[idx * 4 + 1];
                    uint8_t r_val = palette[idx * 4 + 2];
                    pixel_color = UI_RGB(r_val, g_val, b_val);
                } else {
                    pixel_color = UI_COLOR_BLACK;
                }
            }
            else { /* 1bpp */
                uint32_t byte_offset = row_offset + (uint32_t)img_x / 8;
                uint8_t bit = 7 - ((uint32_t)img_x % 8);
                if (byte_offset < s_bmp_data_len) {
                    bool white = (s_bmp_data[byte_offset] >> bit) & 1;
                    pixel_color = white ? UI_COLOR_WHITE : UI_COLOR_BLACK;
                } else {
                    pixel_color = UI_COLOR_BLACK;
                }
            }

            /* Draw the scaled pixel as a filled rect */
            ui_rect_t px = {sx_start, sy_start,
                            (int16_t)scale, (int16_t)scale};
            ui_draw_fill_rect(&px, pixel_color);
        }
    }

    /* Image border */
    ui_rect_t img_border = {ox - 1, oy - 1, render_w + 2, render_h + 2};
    ui_draw_rect_border(&img_border, IMG_PREVIEW_BORDER, 1);
}

/*=============================================================================
 *  List Touch Handler
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int16_t rel_y = e->pos.y - IMG_LIST_Y;
        int16_t idx = rel_y / IMG_ITEM_H - 1 + s_file_scroll;  /* -1 for header row */
        if (idx >= 0 && idx < s_file_count) {
            int16_t old = s_file_selected;
            s_file_selected = idx;
            if (old != idx) img_invalidate_list();

            /* On CLICK (not just DOWN), load the image */
            if (e->type == UI_EVENT_CLICK) {
                /* Strip .BMP extension since CLI "bmp get" adds it automatically */
                char name_no_ext[24];
                strncpy(name_no_ext, s_files[idx].name, sizeof(name_no_ext) - 1);
                name_no_ext[sizeof(name_no_ext) - 1] = '\0';
                char *dot = strrchr(name_no_ext, '.');
                if (dot) *dot = '\0';
                img_request_get(name_no_ext);
            }
        }
    }
    else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_file_scroll + IMG_VISIBLE_ITEMS < s_file_count) {
            s_file_scroll++;
            img_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_file_scroll > 0) {
            s_file_scroll--;
            img_invalidate_list();
        }
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void img_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_img_callbacks);
    img_request_ls();
}

static void img_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearAppCallbacks();
}

static void img_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Left panel: file list */
    if (dirty_top < IMG_LIST_Y + IMG_LIST_H && dirty->x < IMG_LIST_W) {
        ui_rect_t list_bg = {IMG_LIST_X, IMG_LIST_Y, IMG_LIST_W, IMG_LIST_H};
        ui_draw_fill_rect(&list_bg, IMG_LIST_BG);
        ui_draw_rect_border(&list_bg, IMG_BORDER, 1);

        /* Header */
        ui_rect_t hdr = {IMG_LIST_X, IMG_LIST_Y, IMG_LIST_W, IMG_ITEM_H};
        ui_draw_fill_rect(&hdr, UI_HEX(0xE8F5E9));
        char hdr_text[24];
        snprintf(hdr_text, sizeof(hdr_text), "BMP Files (%d)", s_file_count);
        ui_draw_text(IMG_LIST_X + 10, IMG_LIST_Y + 10, hdr_text,
                     &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

        /* File items */
        for (int16_t vis = 0; vis < IMG_VISIBLE_ITEMS; vis++) {
            int16_t idx = s_file_scroll + vis;
            if (idx >= s_file_count) break;

            int16_t item_y = IMG_LIST_Y + (vis + 1) * IMG_ITEM_H;
            if (item_y + IMG_ITEM_H > IMG_LIST_Y + IMG_LIST_H) break;

            ui_rect_t item_rect = {IMG_LIST_X + 1, item_y, IMG_LIST_W - 2, IMG_ITEM_H};

            if (idx == s_file_selected) {
                ui_draw_fill_rect(&item_rect, IMG_LIST_SEL);
            } else if (idx & 1) {
                ui_draw_fill_rect(&item_rect, IMG_LIST_ALT);
            }

            /* File icon */
            ui_draw_fill_round_rect(
                &(ui_rect_t){IMG_LIST_X + 8, item_y + 7, 20, 20},
                3, UI_HEX(0x66BB6A));
            ui_draw_text(IMG_LIST_X + 12, item_y + 10, "B",
                         &font_montserrat_12, UI_COLOR_WHITE);

            /* File name */
            ui_draw_text(IMG_LIST_X + 34, item_y + 10, s_files[idx].name,
                         &font_montserrat_12, IMG_FILE_TEXT);
        }

        /* Scroll indicator */
        if (s_file_count > IMG_VISIBLE_ITEMS) {
            int16_t sb_h = IMG_LIST_H - IMG_ITEM_H;
            int16_t thumb_h = sb_h * IMG_VISIBLE_ITEMS / (s_file_count + 1);
            if (thumb_h < 16) thumb_h = 16;
            int16_t thumb_y = IMG_LIST_Y + IMG_ITEM_H +
                              (sb_h - thumb_h) * s_file_scroll /
                              (s_file_count - IMG_VISIBLE_ITEMS + 1);
            ui_rect_t thumb = {IMG_LIST_X + IMG_LIST_W - 5, thumb_y, 3, thumb_h};
            ui_draw_fill_round_rect(&thumb, 1, UI_HEX(0xBDBDBD));
        }
    }

    /* Right panel: image preview */
    if (dirty_bot > IMG_PREVIEW_Y && dirty->x + dirty->w > IMG_PREVIEW_X) {
        img_render_preview(dirty);
    }

    /* Status bar */
    if (dirty_bot > IMG_BAR_Y) {
        ui_rect_t bar_bg = {0, IMG_BAR_Y, UI_SCREEN_WIDTH, IMG_BAR_H};
        ui_draw_fill_rect(&bar_bg, IMG_BAR_BG);
        ui_draw_text(12, IMG_BAR_Y + 8, s_bar_msg, &font_montserrat_12, IMG_BAR_TEXT);
    }
}

static bool img_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_file_selected < s_file_count - 1) {
            s_file_selected++;
            if (s_file_selected >= s_file_scroll + IMG_VISIBLE_ITEMS)
                s_file_scroll++;
            img_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_file_selected > 0) {
            s_file_selected--;
            if (s_file_selected < s_file_scroll)
                s_file_scroll--;
            img_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_OK) {
        if (s_file_selected >= 0 && s_file_selected < s_file_count) {
            img_request_get(s_files[s_file_selected].name);
        }
        return true;
    }

    return false;
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void app_images_init(void)
{
    ui_app_page_init(&s_app_img, "Images", 0x103);

    /* State */
    s_file_count = 0;
    s_file_scroll = 0;
    s_file_selected = -1;
    s_cli_len = 0;
    s_cli_assembling = false;
    s_cli_expect_ls = false;
    s_cli_expect_get = false;
    s_bmp_loaded = false;
    s_bmp_data_len = 0;
    strcpy(s_bar_msg, "Ready");

    int widx = 0;

    /* Back button and title (from ui_app_page_t) */
    s_img_widgets[widx++] = (ui_widget_t *)&s_app_img.btn_back;
    s_img_widgets[widx++] = (ui_widget_t *)&s_app_img.lbl_title;

    /* List touch area */
    {
        ui_rect_t r = {IMG_LIST_X, IMG_LIST_Y, IMG_LIST_W, IMG_LIST_H};
        ui_widget_init(&s_list_touch, &r);
        s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_list_touch.event_cb = list_touch_event;
        s_img_widgets[widx++] = &s_list_touch;
    }

    ui_page_set_widgets(&s_app_img.page, s_img_widgets, (uint16_t)widx);
    ui_page_set_callbacks(&s_app_img.page, img_page_enter, img_page_exit, img_page_draw, NULL);
    ui_page_set_event_cb(&s_app_img.page, img_page_event);
    ui_page_register(&s_app_img.page);
}

ui_page_t *app_images_get_page(void)
{
    return &s_app_img.page;
}
