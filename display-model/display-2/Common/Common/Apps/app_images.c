/********************************** (C) COPYRIGHT *******************************
* File Name          : app_images.c
* Author             : E-ink Model Team (ported from Display-1 V2.0)
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : Image browser app for E-ink (1bpp).
*                      Lists .bmp files in \BMP via the built-in ls parser,
*                      fetches image data via "cat" (raw binary) and parses
*                      it directly from the UART CLI buffer — zero-copy, no
*                      multi-KB staging buffers (RAM discipline for 128KB).
*                      Supports 1bpp (native), 8bpp and 24bpp BMP (luma
*                      threshold to B/W).  Integer zoom + swipe panning.
********************************************************************************/
#include "app_images.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (648x480)
 *=============================================================================*/

#define IMG_LIST_W          200
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
 *  BMP Constants
 *=============================================================================*/

#define BMP_HEADER_SIZE     14
#define BMP_INFO_SIZE       40
#define BMP_FILE_HEADER_TOTAL (BMP_HEADER_SIZE + BMP_INFO_SIZE)

/*=============================================================================
 *  State
 *=============================================================================*/

#define IMG_MAX_FILES       24      /* RAM discipline: 24 x 32B = ~0.8KB */

typedef struct {
    char     name[26];
    uint32_t size;
} img_file_t;

static ui_app_page_t s_app_img;

/* File list */
static img_file_t s_files[IMG_MAX_FILES];
static uint8_t  s_file_count;
static int16_t  s_file_scroll;
static int16_t  s_file_selected;      /* -1 = none */

/* BMP data — zero-copy view into the UART CLI response buffer */
static const uint8_t *s_bmp;
static uint32_t s_bmp_len;
static bool     s_bmp_truncated;

/* Pending open (from Files app) */
static char     s_pending_name[26];
static bool     s_pending_open;

/* Parsed BMP info */
static int32_t  s_bmp_width;
static int32_t  s_bmp_height;
static uint16_t s_bmp_bpp;
static uint32_t s_bmp_data_offset;
static bool     s_bmp_top_down;
static bool     s_bmp_loaded;

/* Pan offset for images larger than the preview */
static int16_t  s_pan_x;
static int16_t  s_pan_y;

/* CLI phase */
static uint8_t  s_cli_phase;          /* 0=idle, 1=reading cat */

/* Status bar message */
static char s_bar_msg[80];

/* Widgets */
static ui_widget_t s_list_touch;
static ui_widget_t s_view_touch;
static ui_widget_t *s_img_widgets[2 + 2];

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void img_on_file_list(uint8_t status, const file_entry_t *entries, uint8_t count);
static void img_on_cli_complete(const char *buf, uint16_t len, const char *tag);
static void img_decode_bmp(void);
static void img_render_preview(ui_rect_t *dirty);
static void img_set_bar(const char *msg);
static void img_invalidate_list(void);
static void img_invalidate_preview(void);

static uart_cli_cb_t s_img_cb = {
    .on_file_list = img_on_file_list,
    .on_cli_complete = img_on_cli_complete,
};

/*=============================================================================
 *  LE Read Utilities
 *=============================================================================*/

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

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

static bool img_is_bmp_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    return (strcmp(dot, ".bmp") == 0 || strcmp(dot, ".BMP") == 0);
}

static void img_request_get(const char *filename)
{
    s_bmp_loaded = false;
    s_bmp = NULL;
    s_bmp_len = 0;
    s_bmp_truncated = false;
    s_pan_x = 0;
    s_pan_y = 0;
    s_cli_phase = 1;

    char cmd[80];
    snprintf(cmd, sizeof(cmd), "cat \"%s\"", filename);
    UART_SendCLI(cmd);

    char msg[60];
    snprintf(msg, sizeof(msg), "Loading %s...", filename);
    img_set_bar(msg);
    img_invalidate_preview();
}

/*=============================================================================
 *  CLI Response
 *=============================================================================*/

static void img_on_file_list(uint8_t status, const file_entry_t *entries, uint8_t count)
{
    (void)status;
    s_file_count = 0;
    s_file_scroll = 0;
    s_file_selected = -1;

    for (uint8_t i = 0; i < count && s_file_count < IMG_MAX_FILES; i++) {
        if (entries[i].attr & FILE_ATTR_IS_DIR) continue;
        if (!img_is_bmp_ext(entries[i].name)) continue;
        strncpy(s_files[s_file_count].name, entries[i].name,
                sizeof(s_files[s_file_count].name) - 1);
        s_files[s_file_count].name[sizeof(s_files[s_file_count].name) - 1] = '\0';
        s_files[s_file_count].size = entries[i].size;
        s_file_count++;
    }

    if (s_file_count > 0) {
        char msg[40];
        snprintf(msg, sizeof(msg), "%d BMP file(s)", s_file_count);
        img_set_bar(msg);
    } else {
        img_set_bar("No BMP files in \\BMP");
    }
    img_invalidate_list();
}

static void img_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    (void)buf;
    if (s_cli_phase != 1) return;
    if (tag && strcmp(tag, "cat") != 0) return;
    s_cli_phase = 0;

    /* Zero-copy: parse directly from the CLI buffer */
    s_bmp = (const uint8_t *)UART_GetCLIBuf();
    s_bmp_len = UART_GetCLIBufLen();

    /* Detect truncation against the directory-listed size */
    if (s_file_selected >= 0 && s_file_selected < s_file_count &&
        s_files[s_file_selected].size > (uint32_t)(s_bmp_len + 64)) {
        s_bmp_truncated = true;
    }

    if (s_bmp_len >= BMP_FILE_HEADER_TOTAL) {
        img_decode_bmp();
    } else {
        img_set_bar("BMP data too small or transfer incomplete");
        s_bmp_loaded = false;
        img_invalidate_preview();
    }
}

/*=============================================================================
 *  BMP Decode
 *=============================================================================*/

static void img_decode_bmp(void)
{
    if (s_bmp[0] != 'B' || s_bmp[1] != 'M') {
        s_bmp_loaded = false;
        img_set_bar("Not a valid BMP file");
        img_invalidate_preview();
        return;
    }

    s_bmp_data_offset = read_u32_le(&s_bmp[10]);
    s_bmp_width  = read_i32_le(&s_bmp[18]);
    s_bmp_height = read_i32_le(&s_bmp[22]);
    s_bmp_bpp    = read_u16_le(&s_bmp[28]);

    s_bmp_top_down = false;
    if (s_bmp_height < 0) {
        s_bmp_height = -s_bmp_height;
        s_bmp_top_down = true;
    }

    if (s_bmp_width <= 0 || s_bmp_height <= 0 || s_bmp_data_offset >= s_bmp_len) {
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
    snprintf(msg, sizeof(msg), "%ldx%ld %dbpp%s",
             (long)s_bmp_width, (long)s_bmp_height, s_bmp_bpp,
             s_bmp_truncated ? " [TRUNC]" : "");
    img_set_bar(msg);
    img_invalidate_preview();
}

/*=============================================================================
 *  BMP Rendering (1bpp output)
 *=============================================================================*/

/* Luminance of a BGR triple -> 1bpp */
static ui_color_t luma_to_bw(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t luma = (uint16_t)r * 3 + (uint16_t)g * 6 + (uint16_t)b;  /* ~BT.601 x10 */
    return (luma >= 1280) ? UI_COLOR_WHITE : UI_COLOR_BLACK;
}

static void img_render_preview(ui_rect_t *dirty)
{
    ui_rect_t preview_bg = {IMG_PREVIEW_X, IMG_PREVIEW_Y, IMG_PREVIEW_W, IMG_PREVIEW_H};
    ui_draw_fill_rect(&preview_bg, UI_COLOR_WHITE);
    ui_draw_rect_border(&preview_bg, UI_COLOR_BLACK, 1);

    if (!s_bmp_loaded) {
        const char *msg = (s_bmp_len > 0 || s_cli_phase == 1) ? "Loading..." : "Select an image";
        int16_t tw = ui_text_width(msg, &font_montserrat_16);
        ui_draw_text(IMG_PREVIEW_X + (IMG_PREVIEW_W - tw) / 2,
                     IMG_PREVIEW_Y + IMG_PREVIEW_H / 2 - 8,
                     msg, &font_montserrat_16, UI_COLOR_BLACK);
        return;
    }

    /* Integer zoom (up to 8x) */
    int32_t scale = 1;
    while (s_bmp_width * (scale + 1) <= IMG_PREVIEW_W - 20 &&
           s_bmp_height * (scale + 1) <= IMG_PREVIEW_H - 20 &&
           scale < 8) {
        scale++;
    }

    int32_t render_w = s_bmp_width * scale;
    int32_t render_h = s_bmp_height * scale;

    /* Origin: centered, then panned (clamped so image covers preview) */
    int32_t ox = IMG_PREVIEW_X + (IMG_PREVIEW_W - render_w) / 2 + s_pan_x;
    int32_t oy = IMG_PREVIEW_Y + (IMG_PREVIEW_H - render_h) / 2 + s_pan_y;

    /* Row stride */
    uint32_t row_stride;
    if (s_bmp_bpp == 24)
        row_stride = ((uint32_t)s_bmp_width * 3 + 3) & ~3u;
    else if (s_bmp_bpp == 8)
        row_stride = ((uint32_t)s_bmp_width + 3) & ~3u;
    else /* 1bpp */
        row_stride = (((uint32_t)s_bmp_width + 31) / 32) * 4;

    /* Palette (8bpp: 256 entries; 1bpp: 2 entries) after the 54B header */
    const uint8_t *palette = NULL;
    if (s_bmp_bpp == 8 && s_bmp_data_offset >= 54 + 256 * 4)
        palette = &s_bmp[54];
    else if (s_bmp_bpp == 1 && s_bmp_data_offset >= 54 + 2 * 4)
        palette = &s_bmp[54];

    /* Visible source-pixel window (clip to preview area) */
    int32_t vis_x0 = IMG_PREVIEW_X, vis_y0 = IMG_PREVIEW_Y;
    int32_t vis_x1 = IMG_PREVIEW_X + IMG_PREVIEW_W;
    int32_t vis_y1 = IMG_PREVIEW_Y + IMG_PREVIEW_H;

    for (int32_t img_y = 0; img_y < s_bmp_height; img_y++) {
        int32_t sy0 = oy + img_y * scale;
        int32_t sy1 = sy0 + scale;
        if (sy1 <= vis_y0 || sy0 >= vis_y1) continue;

        int32_t data_row = s_bmp_top_down ? img_y : (s_bmp_height - 1 - img_y);
        uint32_t row_offset = s_bmp_data_offset + (uint32_t)data_row * row_stride;

        for (int32_t img_x = 0; img_x < s_bmp_width; img_x++) {
            int32_t sx0 = ox + img_x * scale;
            int32_t sx1 = sx0 + scale;
            if (sx1 <= vis_x0 || sx0 >= vis_x1) continue;

            ui_color_t px_color = UI_COLOR_BLACK;

            if (s_bmp_bpp == 24) {
                uint32_t off = row_offset + (uint32_t)img_x * 3;
                if (off + 2 < s_bmp_len) {
                    px_color = luma_to_bw(s_bmp[off + 2], s_bmp[off + 1], s_bmp[off]);
                }
            } else if (s_bmp_bpp == 8) {
                uint32_t off = row_offset + (uint32_t)img_x;
                if (off < s_bmp_len) {
                    if (palette) {
                        uint8_t idx = s_bmp[off];
                        px_color = luma_to_bw(palette[idx * 4 + 2],
                                              palette[idx * 4 + 1],
                                              palette[idx * 4 + 0]);
                    } else {
                        px_color = (s_bmp[off] >= 128) ? UI_COLOR_WHITE : UI_COLOR_BLACK;
                    }
                }
            } else { /* 1bpp */
                uint32_t off = row_offset + (uint32_t)img_x / 8;
                uint8_t bit = 7 - ((uint32_t)img_x % 8);
                if (off < s_bmp_len) {
                    bool set = (s_bmp[off] >> bit) & 1;
                    if (palette) {
                        /* Honour palette: index 1 colour decides */
                        px_color = set ? luma_to_bw(palette[1 * 4 + 2],
                                                    palette[1 * 4 + 1],
                                                    palette[1 * 4 + 0])
                                       : luma_to_bw(palette[0 * 4 + 2],
                                                    palette[0 * 4 + 1],
                                                    palette[0 * 4 + 0]);
                    } else {
                        px_color = set ? UI_COLOR_WHITE : UI_COLOR_BLACK;
                    }
                }
            }

            if (scale == 1) {
                ui_draw_pixel((int16_t)sx0, (int16_t)sy0, px_color);
            } else {
                ui_rect_t px = {(int16_t)sx0, (int16_t)sy0, (int16_t)scale, (int16_t)scale};
                ui_draw_fill_rect(&px, px_color);
            }
        }
    }

    /* Image border */
    if (render_w <= IMG_PREVIEW_W - 2 && render_h <= IMG_PREVIEW_H - 2) {
        ui_rect_t img_border = {(int16_t)(ox - 1), (int16_t)(oy - 1),
                                (int16_t)(render_w + 2), (int16_t)(render_h + 2)};
        ui_draw_rect_border(&img_border, UI_COLOR_BLACK, 1);
    }
}

/*=============================================================================
 *  Touch Handlers
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_CLICK) {
        int16_t rel_y = e->touch.y - IMG_LIST_Y;
        int16_t idx = rel_y / IMG_ITEM_H - 1 + s_file_scroll;  /* -1 for header */
        if (idx >= 0 && idx < s_file_count) {
            s_file_selected = idx;
            img_invalidate_list();
            img_request_get(s_files[idx].name);
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

static void view_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (!s_bmp_loaded) return;

    /* Pan large images with swipe */
    if (e->type == UI_EVENT_SWIPE_LEFT)  { s_pan_x -= 40; img_invalidate_preview(); }
    else if (e->type == UI_EVENT_SWIPE_RIGHT) { s_pan_x += 40; img_invalidate_preview(); }
    else if (e->type == UI_EVENT_SWIPE_UP)    { s_pan_y -= 40; img_invalidate_preview(); }
    else if (e->type == UI_EVENT_SWIPE_DOWN)  { s_pan_y += 40; img_invalidate_preview(); }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void img_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetCLICallbacks(&s_img_cb);

    if (s_pending_open) {
        /* Opened from the Files app: Core CWD is already the file's dir */
        s_pending_open = false;
        img_request_get(s_pending_name);
    } else {
        UART_RequestFileList("\\BMP");
        img_set_bar("Loading BMP list...");
    }
    ui_page_invalidate_all();
}

static void img_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearCLICallbacks();
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
        ui_draw_fill_rect(&list_bg, UI_COLOR_WHITE);
        ui_draw_rect_border(&list_bg, UI_COLOR_BLACK, 1);

        /* Header */
        char hdr_text[24];
        snprintf(hdr_text, sizeof(hdr_text), "BMP (%d)", s_file_count);
        ui_draw_text(IMG_LIST_X + 10, IMG_LIST_Y + 10, hdr_text,
                     &font_montserrat_12, UI_COLOR_BLACK);
        ui_draw_hline(IMG_LIST_X, IMG_LIST_Y + IMG_ITEM_H, IMG_LIST_W, UI_COLOR_BLACK);

        /* File items */
        for (int16_t vis = 0; vis < IMG_VISIBLE_ITEMS; vis++) {
            int16_t idx = s_file_scroll + vis;
            if (idx >= s_file_count) break;

            int16_t item_y = IMG_LIST_Y + (vis + 1) * IMG_ITEM_H;
            if (item_y + IMG_ITEM_H > IMG_LIST_Y + IMG_LIST_H) break;

            bool sel = (idx == s_file_selected);
            ui_rect_t item_rect = {IMG_LIST_X + 1, item_y, IMG_LIST_W - 2, IMG_ITEM_H};
            if (sel) ui_draw_fill_rect(&item_rect, UI_COLOR_BLACK);
            ui_color_t fg = sel ? UI_COLOR_WHITE : UI_COLOR_BLACK;

            ui_draw_text(IMG_LIST_X + 8, item_y + 10, "B",
                         &font_montserrat_12, fg);
            ui_draw_text(IMG_LIST_X + 28, item_y + 10, s_files[idx].name,
                         &font_montserrat_12, fg);
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
            ui_draw_fill_rect(&thumb, UI_COLOR_BLACK);
        }
    }

    /* Right panel: image preview */
    if (dirty_bot > IMG_PREVIEW_Y && dirty->x + dirty->w > IMG_PREVIEW_X) {
        img_render_preview(dirty);
    }

    /* Status bar */
    if (dirty_bot > IMG_BAR_Y) {
        ui_rect_t bar_bg = {0, IMG_BAR_Y, UI_SCREEN_WIDTH, IMG_BAR_H};
        ui_draw_fill_rect(&bar_bg, UI_COLOR_WHITE);
        ui_draw_hline(0, IMG_BAR_Y, UI_SCREEN_WIDTH, UI_COLOR_BLACK);
        ui_draw_text(12, IMG_BAR_Y + 8, s_bar_msg, &font_montserrat_12, UI_COLOR_BLACK);
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
 *  Public API
 *=============================================================================*/

void app_images_open_file(const char *name)
{
    strncpy(s_pending_name, name, sizeof(s_pending_name) - 1);
    s_pending_name[sizeof(s_pending_name) - 1] = '\0';
    s_pending_open = true;
}

void app_images_init(void)
{
    ui_app_page_init(&s_app_img, "Images", 0x103);

    s_file_count = 0;
    s_file_scroll = 0;
    s_file_selected = -1;
    s_cli_phase = 0;
    s_bmp = NULL;
    s_bmp_len = 0;
    s_bmp_truncated = false;
    s_bmp_loaded = false;
    s_pending_open = false;
    s_pan_x = 0;
    s_pan_y = 0;
    strcpy(s_bar_msg, "Ready");

    int widx = 0;

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

    /* Preview touch area (panning) */
    {
        ui_rect_t r = {IMG_PREVIEW_X, IMG_PREVIEW_Y, IMG_PREVIEW_W, IMG_PREVIEW_H};
        ui_widget_init(&s_view_touch, &r);
        s_view_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_view_touch.event_cb = view_touch_event;
        s_img_widgets[widx++] = &s_view_touch;
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
