/********************************** (C) COPYRIGHT *******************************
* File Name          : app_ebook.c
* Author             : E-ink Model Team (ported from Display-1)
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : EBook reader app for E-ink (1bpp).
*                      Lists .txt/.md/.json files from the BOOK directory.
*                      Reads files via "cat" CLI command; content is parsed
*                      directly from the UART CLI response buffer (zero-copy,
*                      no 20KB private buffer — RAM discipline for 128KB).
*                      Pagination, light/dark theme, 3 font sizes.
********************************************************************************/
#include "app_ebook.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout (648x480)
 *=============================================================================*/

#define EB_LEFT_W           220
#define EB_LIST_X           10
#define EB_LIST_Y           (APP_TITLE_BAR_H + 8)
#define EB_LIST_W           (EB_LEFT_W - 20)
#define EB_LIST_H           (UI_SCREEN_HEIGHT - EB_LIST_Y - 48)
#define EB_ITEM_H           32
#define EB_HDR_H            28
#define EB_VISIBLE          ((EB_LIST_H - EB_HDR_H) / EB_ITEM_H)

#define EB_VIEW_X           EB_LEFT_W
#define EB_VIEW_Y           (APP_TITLE_BAR_H + 4)
#define EB_VIEW_W           (UI_SCREEN_WIDTH - EB_LEFT_W)
#define EB_VIEW_H           (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - 4 - 44)

/* Toolbar */
#define EB_TOOLBAR_Y        (UI_SCREEN_HEIGHT - 44)
#define EB_TOOLBAR_H        44
#define EB_TB_BTN_W         76
#define EB_TB_BTN_H         30

/*=============================================================================
 *  Themes (1bpp: only light & dark make sense on B/W e-ink)
 *=============================================================================*/

typedef struct {
    ui_color_t bg;
    ui_color_t text;        /* body text */
    ui_color_t accent;      /* headings, badges */
    ui_color_t border;
} eb_theme_t;

static const eb_theme_t s_themes[] = {
    /* 0: Light — black on white (preferred for e-ink) */
    { UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK, UI_COLOR_BLACK },
    /* 1: Dark — white on black (inverted; more refresh energy, for ambience) */
    { UI_COLOR_BLACK, UI_COLOR_WHITE, UI_COLOR_WHITE, UI_COLOR_WHITE },
};

#define EB_THEME_COUNT  2

/*=============================================================================
 *  Font sizes (mapped onto the two available fonts + line spacing)
 *=============================================================================*/

typedef struct {
    const ui_font_t *font;
    int16_t line_h;      /* total line height (including spacing) */
    int16_t pad_x;       /* horizontal padding for text area */
    int16_t pad_y;       /* top padding for text area */
} eb_font_cfg_t;

static const eb_font_cfg_t s_font_cfgs[] = {
    { &font_montserrat_12, 16, 10, 8 },   /* Small  */
    { &font_montserrat_16, 22, 12, 10 },  /* Medium */
    { &font_montserrat_16, 26, 14, 12 },  /* Large  */
};

#define EB_FONT_COUNT   3

/*=============================================================================
 *  State
 *=============================================================================*/

#define EB_MAX_FILES     24         /* RAM discipline: 24 x ~60B = ~1.4KB */
#define EB_MAX_NAME      48

typedef struct {
    char name[EB_MAX_NAME + 1];
    char ext[6];           /* ".txt", ".md", ".json" */
    uint32_t size;
} eb_file_t;

static ui_app_page_t s_app_eb;

static eb_file_t s_files[EB_MAX_FILES];
static uint8_t   s_file_count;
static int16_t   s_file_scroll;
static int16_t   s_file_selected;

/* File content — zero-copy view into the UART CLI response buffer.
 * Valid while no further CLI command is issued (page turns are local). */
static const char *s_content;
static uint16_t  s_content_len;
static bool      s_truncated;        /* file larger than CLI buffer */

/* CLI phase tracking */
static uint8_t  s_cli_phase;         /* 0=idle, 2=reading cat */

/* View state */
static bool     s_viewing;           /* true = showing file content */
static char     s_view_name[EB_MAX_NAME + 1];
static char     s_view_ext[6];
static uint8_t  s_theme_idx;
static uint8_t  s_font_idx;

/* Pagination */
static int16_t  s_page;              /* current page (0-based) */
static int16_t  s_total_pages;

/* Status */
static char     s_status[80];

/* Widgets */
static ui_widget_t  s_list_touch;
static ui_widget_t  s_view_touch;
static ui_button_t  btn_prev, btn_next, btn_theme, btn_font, btn_back_list;
static ui_widget_t *s_widgets[2 + 2 + 5]; /* back+title + touch*2 + buttons */

/* Forward declarations */
static void eb_on_cli_complete(const char *buf, uint16_t len, const char *tag);
static void eb_on_file_list(uint8_t status, const file_entry_t *entries, uint8_t count);
static void eb_set_status(const char *msg);
static void eb_invalidate_list(void);
static void eb_invalidate_view(void);
static void eb_invalidate_toolbar(void);
static void eb_calc_pages(void);
static void eb_open_file(int16_t idx);
static void eb_close_file(void);

static uart_cli_cb_t s_eb_cb;

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void eb_set_status(const char *msg)
{
    strncpy(s_status, msg, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
}

static void eb_invalidate_list(void)
{
    ui_rect_t r = {EB_LIST_X, EB_LIST_Y, EB_LIST_W, EB_LIST_H};
    ui_page_invalidate(&r);
}

static void eb_invalidate_view(void)
{
    ui_rect_t r = {EB_VIEW_X, EB_VIEW_Y, EB_VIEW_W, EB_VIEW_H};
    ui_page_invalidate(&r);
}

static void eb_invalidate_toolbar(void)
{
    ui_rect_t r = {0, EB_TOOLBAR_Y, UI_SCREEN_WIDTH, EB_TOOLBAR_H};
    ui_page_invalidate(&r);
}

static const eb_theme_t *eb_theme(void)
{
    return &s_themes[s_theme_idx];
}

static const eb_font_cfg_t *eb_font(void)
{
    return &s_font_cfgs[s_font_idx];
}

/*=============================================================================
 *  File extension check
 *=============================================================================*/

static bool eb_is_book_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    if (strcmp(dot, ".TXT") == 0 || strcmp(dot, ".txt") == 0) return true;
    if (strcmp(dot, ".MD") == 0   || strcmp(dot, ".md") == 0) return true;
    if (strcmp(dot, ".JSON") == 0 || strcmp(dot, ".json") == 0) return true;
    return false;
}

static void eb_get_ext(const char *name, char *ext_out)
{
    const char *dot = strrchr(name, '.');
    if (dot) {
        strncpy(ext_out, dot, 5);
        ext_out[5] = '\0';
    } else {
        ext_out[0] = '\0';
    }
}

/*=============================================================================
 *  Pagination — calculate page count based on content, font, and view area
 *=============================================================================*/

static void eb_calc_pages(void)
{
    if (!s_content || s_content_len == 0) {
        s_total_pages = 1;
        s_page = 0;
        return;
    }

    const eb_font_cfg_t *fc = eb_font();
    int16_t text_w = EB_VIEW_W - fc->pad_x * 2 - 16;  /* minus border/padding */
    int16_t text_h = EB_VIEW_H - fc->pad_y * 2 - 8;
    int16_t chars_per_line = text_w / 8;  /* ~8px per char */
    if (chars_per_line < 10) chars_per_line = 10;
    int16_t lines_per_page = text_h / fc->line_h;
    if (lines_per_page < 1) lines_per_page = 1;

    int16_t total_lines = 0;
    const char *p = s_content;
    const char *end = s_content + s_content_len;

    while (p < end) {
        const char *eol = p;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;

        int16_t line_len = (int16_t)(eol - p);
        int16_t wrap_lines = (line_len + chars_per_line - 1) / chars_per_line;
        if (wrap_lines < 1) wrap_lines = 1;
        total_lines += wrap_lines;

        p = eol;
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }

    s_total_pages = (total_lines + lines_per_page - 1) / lines_per_page;
    if (s_total_pages < 1) s_total_pages = 1;
    if (s_page >= s_total_pages) s_page = s_total_pages - 1;
    if (s_page < 0) s_page = 0;
}

/*=============================================================================
 *  Open / Close file
 *=============================================================================*/

static void eb_open_file(int16_t idx)
{
    if (idx < 0 || idx >= s_file_count) return;

    s_file_selected = idx;
    strncpy(s_view_name, s_files[idx].name, EB_MAX_NAME);
    s_view_name[EB_MAX_NAME] = '\0';
    strncpy(s_view_ext, s_files[idx].ext, 5);
    s_view_ext[5] = '\0';

    s_content = NULL;
    s_content_len = 0;
    s_truncated = false;
    s_cli_phase = 2;

    char cmd[80];
    snprintf(cmd, sizeof(cmd), "cat \"%s\"", s_files[idx].name);
    UART_SendCLI(cmd);

    eb_set_status("Loading...");
    s_viewing = true;
    s_page = 0;
    ui_page_invalidate_all();
}

static void eb_close_file(void)
{
    s_viewing = false;
    s_content = NULL;
    s_content_len = 0;
    s_page = 0;
    s_total_pages = 1;
    ui_page_invalidate_all();
}

/*=============================================================================
 *  File list callback (from UART module's built-in ls parser)
 *=============================================================================*/

static void eb_on_file_list(uint8_t status, const file_entry_t *entries, uint8_t count)
{
    (void)status;
    s_file_count = 0;

    for (uint8_t i = 0; i < count && s_file_count < EB_MAX_FILES; i++) {
        if (entries[i].attr & FILE_ATTR_IS_DIR) continue;
        if (!eb_is_book_ext(entries[i].name)) continue;

        strncpy(s_files[s_file_count].name, entries[i].name, EB_MAX_NAME);
        s_files[s_file_count].name[EB_MAX_NAME] = '\0';
        eb_get_ext(entries[i].name, s_files[s_file_count].ext);
        s_files[s_file_count].size = entries[i].size;
        s_file_count++;
    }

    char msg[48];
    snprintf(msg, sizeof(msg), "%d book(s)", s_file_count);
    eb_set_status(msg);
    eb_invalidate_list();
    eb_invalidate_toolbar();
}

static void eb_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    (void)buf;
    if (s_cli_phase != 2) return;
    (void)tag;

    /* cat response — zero-copy: parse directly from the CLI buffer */
    s_content = UART_GetCLIBuf();
    s_content_len = UART_GetCLIBufLen();
    s_cli_phase = 0;

    /* Detect truncation: CLI buffer is smaller than the real file */
    if (s_file_selected >= 0 && s_file_selected < s_file_count &&
        s_files[s_file_selected].size > (uint32_t)(s_content_len + 64)) {
        s_truncated = true;
    }

    eb_calc_pages();

    char msg[80];
    snprintf(msg, sizeof(msg), "%s (%d pages)%s",
             s_view_name, s_total_pages, s_truncated ? " [TRUNC]" : "");
    eb_set_status(msg);
    eb_invalidate_view();
    eb_invalidate_toolbar();
}

/*=============================================================================
 *  List touch handler
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (s_viewing) return;

    if (e->type == UI_EVENT_DOUBLE_CLICK || e->type == UI_EVENT_CLICK) {
        int16_t rel_y = e->touch.y - (EB_LIST_Y + EB_HDR_H) + s_file_scroll * EB_ITEM_H;
        int16_t idx = rel_y / EB_ITEM_H;
        if (idx >= 0 && idx < s_file_count) {
            s_file_selected = idx;
            eb_open_file(idx);
        }
    }
    else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_file_scroll + EB_VISIBLE < s_file_count) {
            s_file_scroll++;
            eb_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_file_scroll > 0) {
            s_file_scroll--;
            eb_invalidate_list();
        }
    }
}

/*=============================================================================
 *  View touch handler (swipe to turn pages)
 *=============================================================================*/

static void view_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (!s_viewing) return;

    if (e->type == UI_EVENT_SWIPE_LEFT || e->type == UI_EVENT_SWIPE_UP) {
        if (s_page < s_total_pages - 1) {
            s_page++;
            eb_invalidate_view();
            eb_invalidate_toolbar();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_RIGHT || e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_page > 0) {
            s_page--;
            eb_invalidate_view();
            eb_invalidate_toolbar();
        }
    }
    else if (e->type == UI_EVENT_CLICK) {
        /* Tap right half = next page, left half = prev page (e-reader style) */
        if (e->touch.x > EB_VIEW_X + EB_VIEW_W / 2) {
            if (s_page < s_total_pages - 1) {
                s_page++;
                eb_invalidate_view();
                eb_invalidate_toolbar();
            }
        } else {
            if (s_page > 0) {
                s_page--;
                eb_invalidate_view();
                eb_invalidate_toolbar();
            }
        }
    }
}

/*=============================================================================
 *  Button callbacks
 *=============================================================================*/

static void btn_prev_click(ui_widget_t *w)
{
    (void)w;
    if (s_viewing && s_page > 0) {
        s_page--;
        eb_invalidate_view();
        eb_invalidate_toolbar();
    }
}

static void btn_next_click(ui_widget_t *w)
{
    (void)w;
    if (s_viewing && s_page < s_total_pages - 1) {
        s_page++;
        eb_invalidate_view();
        eb_invalidate_toolbar();
    }
}

static void btn_theme_click(ui_widget_t *w)
{
    (void)w;
    s_theme_idx = (s_theme_idx + 1) % EB_THEME_COUNT;
    ui_page_invalidate_all();
}

static void btn_font_click(ui_widget_t *w)
{
    (void)w;
    s_font_idx = (s_font_idx + 1) % EB_FONT_COUNT;
    if (s_viewing) eb_calc_pages();
    ui_page_invalidate_all();
}

static void btn_back_list_click(ui_widget_t *w)
{
    (void)w;
    eb_close_file();
}

/*=============================================================================
 *  Page callbacks
 *=============================================================================*/

static void eb_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetCLICallbacks(&s_eb_cb);
    s_viewing = false;
    s_cli_phase = 0;
    s_file_count = 0;
    s_file_scroll = 0;
    s_file_selected = -1;
    s_content = NULL;
    s_content_len = 0;
    eb_set_status("Loading BOOK...");
    UART_RequestFileList("\\BOOK");
}

static void eb_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearCLICallbacks();
}

/*=============================================================================
 *  Text rendering helpers
 *=============================================================================*/

/* Draw a single line of text with char wrap within max_w.
 * Returns the Y position after the last drawn line. */
static int16_t eb_draw_text_line(int16_t x, int16_t y, int16_t max_w,
                                  const char *text, uint16_t text_len,
                                  const ui_font_t *font, ui_color_t color,
                                  int16_t line_h, int16_t max_y)
{
    if (text_len == 0) {
        return y + line_h;  /* empty line */
    }

    int16_t cx = x;
    const char *p = text;
    const char *end = text + text_len;
    const char *line_start = p;
    char buf[96];

    while (p < end) {
        /* Estimate char width (8px default; good enough for wrap) */
        int16_t char_w = 8;

        if (cx + char_w > x + max_w && p != line_start) {
            if (y + line_h > max_y) return y;
            uint16_t llen = (uint16_t)(p - line_start);
            if (llen > sizeof(buf) - 1) llen = sizeof(buf) - 1;
            memcpy(buf, line_start, llen);
            buf[llen] = '\0';
            ui_draw_text(x, y, buf, font, color);
            y += line_h;
            cx = x;
            line_start = p;
        }

        cx += char_w;
        p++;
    }

    if (p > line_start) {
        if (y + line_h > max_y) return y;
        uint16_t llen = (uint16_t)(p - line_start);
        if (llen > sizeof(buf) - 1) llen = sizeof(buf) - 1;
        memcpy(buf, line_start, llen);
        buf[llen] = '\0';
        ui_draw_text(x, y, buf, font, color);
        y += line_h;
    }

    return y;
}

/*=============================================================================
 *  Render file content for current page
 *=============================================================================*/

static void eb_render_content(const eb_theme_t *th, const eb_font_cfg_t *fc,
                               ui_rect_t *view_rect)
{
    if (!s_content || s_content_len == 0) {
        ui_draw_text(view_rect->x + fc->pad_x, view_rect->y + fc->pad_y,
                     "Loading...", fc->font, th->text);
        return;
    }

    int16_t text_x = view_rect->x + fc->pad_x;
    int16_t text_w = view_rect->w - fc->pad_x * 2;
    int16_t max_y = view_rect->y + view_rect->h - fc->pad_y;
    int16_t line_h = fc->line_h;
    int16_t lines_per_page = (view_rect->h - fc->pad_y * 2) / line_h;
    if (lines_per_page < 1) lines_per_page = 1;

    int16_t current_line = 0;
    int16_t target_page_start_line = s_page * lines_per_page;
    int16_t draw_y = view_rect->y + fc->pad_y;

    const char *p = s_content;
    const char *end = s_content + s_content_len;
    bool is_md = (strcmp(s_view_ext, ".md") == 0 || strcmp(s_view_ext, ".MD") == 0);
    bool is_json = (strcmp(s_view_ext, ".json") == 0 || strcmp(s_view_ext, ".JSON") == 0);

    /* Skip lines before current page */
    while (p < end && current_line < target_page_start_line) {
        const char *eol = p;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;

        int16_t line_len = (int16_t)(eol - p);
        int16_t chars_per_line = text_w / 8;
        if (chars_per_line < 10) chars_per_line = 10;
        int16_t wrap_lines = (line_len + chars_per_line - 1) / chars_per_line;
        if (wrap_lines < 1) wrap_lines = 1;
        current_line += wrap_lines;

        p = eol;
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }

    /* Draw lines for current page */
    int16_t lines_drawn = 0;

    while (p < end && lines_drawn < lines_per_page && draw_y + line_h <= max_y) {
        const char *eol = p;
        while (eol < end && *eol != '\n' && *eol != '\r') eol++;

        uint16_t line_len = (uint16_t)(eol - p);

        /* --- Markdown rendering --- */
        if (is_md && line_len > 0) {
            /* Heading: # ## ### — accent color + underline */
            if (p[0] == '#') {
                int heading_level = 0;
                while (heading_level < (int)line_len && p[heading_level] == '#')
                    heading_level++;
                if (heading_level <= 3) {
                    const char *h_text = p + heading_level;
                    while (h_text < eol && *h_text == ' ') h_text++;
                    uint16_t h_len = (uint16_t)(eol - h_text);

                    ui_draw_hline(text_x, draw_y + line_h, text_w, th->accent);

                    draw_y = eb_draw_text_line(text_x, draw_y, text_w,
                                               h_text, h_len,
                                               fc->font, th->accent,
                                               line_h, max_y);
                    lines_drawn++;
                    p = eol;
                    while (p < end && (*p == '\n' || *p == '\r')) p++;
                    continue;
                }
            }

            /* Blockquote: > — left bar + indent */
            if (p[0] == '>') {
                ui_draw_vline(text_x - 4, draw_y, line_h, th->accent);
                const char *q_text = p + 1;
                while (q_text < eol && *q_text == ' ') q_text++;
                uint16_t q_len = (uint16_t)(eol - q_text);
                draw_y = eb_draw_text_line(text_x + 6, draw_y, text_w - 6,
                                           q_text, q_len,
                                           fc->font, th->text,
                                           line_h, max_y);
                lines_drawn++;
                p = eol;
                while (p < end && (*p == '\n' || *p == '\r')) p++;
                continue;
            }

            /* List item: - or * — ASCII bullet marker */
            if ((p[0] == '-' || p[0] == '*') && line_len > 1 && p[1] == ' ') {
                ui_draw_text(text_x, draw_y, "*", fc->font, th->accent);
                const char *li_text = p + 2;
                uint16_t li_len = (uint16_t)(eol - li_text);
                draw_y = eb_draw_text_line(text_x + 14, draw_y, text_w - 14,
                                           li_text, li_len,
                                           fc->font, th->text,
                                           line_h, max_y);
                lines_drawn++;
                p = eol;
                while (p < end && (*p == '\n' || *p == '\r')) p++;
                continue;
            }

            /* Code block: 4-space or tab indent — inverted strip */
            if ((line_len >= 4 && p[0] == ' ' && p[1] == ' ' &&
                 p[2] == ' ' && p[3] == ' ') || p[0] == '\t') {
                ui_rect_t code_r = {text_x, draw_y, text_w, line_h};
                ui_draw_fill_rect(&code_r, th->text);   /* inverted bg */
                const char *c_text = (p[0] == '\t') ? p + 1 : p + 4;
                uint16_t c_len = (uint16_t)(eol - c_text);
                draw_y = eb_draw_text_line(text_x + 4, draw_y, text_w - 4,
                                           c_text, c_len,
                                           fc->font, th->bg,   /* inverted fg */
                                           line_h, max_y);
                lines_drawn++;
                p = eol;
                while (p < end && (*p == '\n' || *p == '\r')) p++;
                continue;
            }
        }

        /* --- JSON rendering: underline the key --- */
        if (is_json && line_len > 0) {
            const char *quote1 = memchr(p, '"', line_len);
            if (quote1) {
                const char *colon = memchr(quote1, ':', (size_t)(eol - quote1));
                if (colon) {
                    uint16_t pre_len = (uint16_t)(quote1 - p);
                    uint16_t key_len = (uint16_t)(colon - quote1 + 1);
                    uint16_t rest_len = (uint16_t)(eol - colon - 1);

                    int16_t cx = text_x;
                    char buf[96];

                    if (pre_len > 0) {
                        uint16_t cl = (pre_len < (uint16_t)sizeof(buf) - 1) ? pre_len : (uint16_t)sizeof(buf) - 1;
                        memcpy(buf, p, cl);
                        buf[cl] = '\0';
                        ui_draw_text(cx, draw_y, buf, fc->font, th->text);
                        cx += ui_text_width(buf, fc->font);
                    }

                    {
                        uint16_t cl = (key_len < (uint16_t)sizeof(buf) - 1) ? key_len : (uint16_t)sizeof(buf) - 1;
                        memcpy(buf, quote1, cl);
                        buf[cl] = '\0';
                        ui_draw_text(cx, draw_y, buf, fc->font, th->accent);
                        /* Key underline as the "highlight" on 1bpp */
                        ui_draw_hline(cx, draw_y + fc->font->height + 1,
                                      ui_text_width(buf, fc->font), th->accent);
                        cx += ui_text_width(buf, fc->font);
                    }

                    if (rest_len > 0 && colon + 1 < eol) {
                        const char *rest = colon + 1;
                        while (rest < eol && *rest == ' ') rest++;
                        uint16_t rl = (uint16_t)(eol - rest);
                        uint16_t cl = (rl < (uint16_t)sizeof(buf) - 1) ? rl : (uint16_t)sizeof(buf) - 1;
                        memcpy(buf, rest, cl);
                        buf[cl] = '\0';
                        ui_draw_text(cx, draw_y, buf, fc->font, th->text);
                    }

                    draw_y += line_h;
                    lines_drawn++;
                    p = eol;
                    while (p < end && (*p == '\n' || *p == '\r')) p++;
                    continue;
                }
            }
        }

        /* --- Plain text line --- */
        draw_y = eb_draw_text_line(text_x, draw_y, text_w,
                                   p, line_len,
                                   fc->font, th->text,
                                   line_h, max_y);
        lines_drawn++;

        p = eol;
        while (p < end && (*p == '\n' || *p == '\r')) p++;
    }

    /* Truncation marker on the last page */
    if (s_truncated && s_page >= s_total_pages - 1) {
        ui_draw_text(text_x, max_y - line_h, "[...]",
                     fc->font, th->accent);
    }
}

/*=============================================================================
 *  Page draw
 *=============================================================================*/

static void eb_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    const eb_theme_t *th = eb_theme();
    int16_t db = dirty->y + dirty->h;
    int16_t dl = dirty->x, dr = dirty->x + dirty->w;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Main background */
    if (db > APP_TITLE_BAR_H) {
        ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                        UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
        ui_draw_fill_rect(&bg, th->bg);
    }

    /* ---- Left panel: file list ---- */
    if (dl < EB_LEFT_W && db > EB_LIST_Y) {
        ui_rect_t lbg = {EB_LIST_X, EB_LIST_Y, EB_LIST_W, EB_LIST_H};
        ui_draw_fill_round_rect(&lbg, 6, th->bg);
        ui_draw_round_rect_border(&lbg, 6, th->border, 1);

        /* Header */
        char hdr_text[24];
        snprintf(hdr_text, sizeof(hdr_text), "Books (%d)", s_file_count);
        ui_draw_text(EB_LIST_X + 10, EB_LIST_Y + 7, hdr_text,
                     &font_montserrat_12, th->accent);
        ui_draw_hline(EB_LIST_X, EB_LIST_Y + EB_HDR_H, EB_LIST_W, th->border);

        /* Items */
        for (int16_t vis = 0; vis < EB_VISIBLE; vis++) {
            int16_t idx = s_file_scroll + vis;
            if (idx >= s_file_count) break;

            int16_t iy = EB_LIST_Y + EB_HDR_H + vis * EB_ITEM_H;
            if (iy + EB_ITEM_H > EB_LIST_Y + EB_LIST_H) break;

            bool sel = (idx == s_file_selected && !s_viewing);
            ui_rect_t ir = {EB_LIST_X + 3, iy, EB_LIST_W - 6, EB_ITEM_H - 2};
            if (sel) ui_draw_fill_round_rect(&ir, 4, th->text);
            ui_color_t fg = sel ? th->bg : th->text;

            /* Extension badge (outlined) */
            ui_rect_t badge = {EB_LIST_X + 6, iy + 6, 40, 18};
            ui_draw_round_rect_border(&badge, 3, fg, 1);
            ui_draw_text(EB_LIST_X + 10, iy + 9,
                         s_files[idx].ext, &font_montserrat_12, fg);

            /* Filename (truncated, extension stripped) */
            char display_name[EB_MAX_NAME + 1];
            strncpy(display_name, s_files[idx].name, EB_MAX_NAME);
            display_name[EB_MAX_NAME] = '\0';
            char *dot = strrchr(display_name, '.');
            if (dot) *dot = '\0';
            ui_draw_text(EB_LIST_X + 52, iy + 9, display_name,
                         &font_montserrat_12, fg);
        }

        /* Scroll indicator */
        if (s_file_count > EB_VISIBLE) {
            int16_t sb_h = EB_LIST_H - EB_HDR_H;
            int16_t th_h = sb_h * EB_VISIBLE / (s_file_count + 1);
            if (th_h < 16) th_h = 16;
            int16_t th_y = EB_LIST_Y + EB_HDR_H +
                (sb_h - th_h) * s_file_scroll / (s_file_count - EB_VISIBLE + 1);
            ui_rect_t thumb = {EB_LIST_X + EB_LIST_W - 6, th_y, 3, th_h};
            ui_draw_fill_rect(&thumb, th->accent);
        }
    }

    /* ---- Right panel: content viewer ---- */
    if (dr > EB_VIEW_X && db > EB_VIEW_Y) {
        ui_rect_t vr = {EB_VIEW_X, EB_VIEW_Y, EB_VIEW_W, EB_VIEW_H};
        ui_draw_fill_round_rect(&vr, 6, th->bg);
        ui_draw_round_rect_border(&vr, 6, th->border, 1);

        if (s_viewing) {
            eb_render_content(th, eb_font(), &vr);
        } else {
            ui_draw_text(EB_VIEW_X + 20, EB_VIEW_Y + 20,
                         "Select a book on the left",
                         &font_montserrat_16, th->text);
        }
    }

    /* ---- Toolbar ---- */
    if (db > EB_TOOLBAR_Y) {
        ui_rect_t tb = {0, EB_TOOLBAR_Y, UI_SCREEN_WIDTH, EB_TOOLBAR_H};
        ui_draw_fill_rect(&tb, th->bg);
        ui_draw_hline(0, EB_TOOLBAR_Y, UI_SCREEN_WIDTH, th->border);

        /* Page info */
        char page_info[32];
        if (s_viewing)
            snprintf(page_info, sizeof(page_info), "%d / %d", s_page + 1, s_total_pages);
        else
            snprintf(page_info, sizeof(page_info), "EBook");
        ui_draw_text(12, EB_TOOLBAR_Y + 14, page_info,
                     &font_montserrat_12, th->text);

        /* Status */
        ui_draw_text(100, EB_TOOLBAR_Y + 14, s_status,
                     &font_montserrat_12, th->text);
    }
}

/*=============================================================================
 *  Page event
 *=============================================================================*/

static bool eb_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    if (s_viewing) {
        /* In viewer mode */
        if (e->type == UI_EVENT_KEY_BACK ||
            (e->type == UI_EVENT_KEY_CLICK && e->key.code == UI_KEY_BACK)) {
            eb_close_file();
            return true;
        }
        if (e->type == UI_EVENT_KEY_DOWN_ARROW || e->type == UI_EVENT_KEY_RIGHT_ARROW) {
            if (s_page < s_total_pages - 1) {
                s_page++;
                eb_invalidate_view();
                eb_invalidate_toolbar();
            }
            return true;
        }
        if (e->type == UI_EVENT_KEY_UP_ARROW || e->type == UI_EVENT_KEY_LEFT_ARROW) {
            if (s_page > 0) {
                s_page--;
                eb_invalidate_view();
                eb_invalidate_toolbar();
            }
            return true;
        }
    } else {
        /* In list mode */
        if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
            if (s_file_selected < s_file_count - 1) {
                s_file_selected++;
                if (s_file_selected >= s_file_scroll + EB_VISIBLE) s_file_scroll++;
                eb_invalidate_list();
            }
            return true;
        }
        if (e->type == UI_EVENT_KEY_UP_ARROW) {
            if (s_file_selected > 0) {
                s_file_selected--;
                if (s_file_selected < s_file_scroll) s_file_scroll--;
                eb_invalidate_list();
            }
            return true;
        }
        if (e->type == UI_EVENT_KEY_OK) {
            if (s_file_selected >= 0) {
                eb_open_file(s_file_selected);
            }
            return true;
        }
    }

    return false;
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void app_ebook_init(void)
{
    ui_app_page_init(&s_app_eb, "EBook", 0x10D);

    s_content = NULL;
    s_content_len = 0;
    s_file_count = 0;
    s_file_scroll = 0;
    s_file_selected = -1;
    s_cli_phase = 0;
    s_viewing = false;
    s_theme_idx = 0;   /* light */
    s_font_idx = 1;    /* medium */
    s_page = 0;
    s_total_pages = 1;
    strcpy(s_status, "Ready");

    s_eb_cb.on_cli_complete = eb_on_cli_complete;
    s_eb_cb.on_file_list = eb_on_file_list;
    s_eb_cb.on_cwd_notify = NULL;

    int wi = 0;
    s_widgets[wi++] = (ui_widget_t *)&s_app_eb.btn_back;
    s_widgets[wi++] = (ui_widget_t *)&s_app_eb.lbl_title;

    /* List touch */
    {
        ui_rect_t r = {EB_LIST_X, EB_LIST_Y + EB_HDR_H, EB_LIST_W, EB_LIST_H - EB_HDR_H};
        ui_widget_init(&s_list_touch, &r);
        s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_list_touch.event_cb = list_touch_event;
    }
    s_widgets[wi++] = &s_list_touch;

    /* View touch (swipe/tap pages) */
    {
        ui_rect_t r = {EB_VIEW_X, EB_VIEW_Y, EB_VIEW_W, EB_VIEW_H};
        ui_widget_init(&s_view_touch, &r);
        s_view_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_view_touch.event_cb = view_touch_event;
    }
    s_widgets[wi++] = &s_view_touch;

    /* Toolbar buttons (right-aligned within 648px) */
    int16_t tb_y = EB_TOOLBAR_Y + 7;

    {
        ui_rect_t r = {198, tb_y, EB_TB_BTN_W, EB_TB_BTN_H};
        ui_button_init(&btn_prev, &r, "Prev", &font_montserrat_12);
        ui_button_set_callback(&btn_prev, btn_prev_click);
        ui_button_set_colors(&btn_prev, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
        btn_prev.radius = 6;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_prev;

    {
        ui_rect_t r = {284, tb_y, EB_TB_BTN_W, EB_TB_BTN_H};
        ui_button_init(&btn_next, &r, "Next", &font_montserrat_12);
        ui_button_set_callback(&btn_next, btn_next_click);
        ui_button_set_colors(&btn_next, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
        btn_next.radius = 6;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_next;

    {
        ui_rect_t r = {370, tb_y, EB_TB_BTN_W, EB_TB_BTN_H};
        ui_button_init(&btn_theme, &r, "Theme", &font_montserrat_12);
        ui_button_set_callback(&btn_theme, btn_theme_click);
        ui_button_set_colors(&btn_theme, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
        btn_theme.radius = 6;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_theme;

    {
        ui_rect_t r = {456, tb_y, EB_TB_BTN_W, EB_TB_BTN_H};
        ui_button_init(&btn_font, &r, "Font", &font_montserrat_12);
        ui_button_set_callback(&btn_font, btn_font_click);
        ui_button_set_colors(&btn_font, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
        btn_font.radius = 6;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_font;

    {
        ui_rect_t r = {542, tb_y, EB_TB_BTN_W, EB_TB_BTN_H};
        ui_button_init(&btn_back_list, &r, "Books", &font_montserrat_12);
        ui_button_set_callback(&btn_back_list, btn_back_list_click);
        ui_button_set_colors(&btn_back_list, UI_COLOR_BLACK, UI_COLOR_WHITE, UI_COLOR_WHITE);
        btn_back_list.radius = 6;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_back_list;

    ui_page_set_widgets(&s_app_eb.page, s_widgets, (uint16_t)wi);
    ui_page_set_callbacks(&s_app_eb.page, eb_page_enter, eb_page_exit, eb_page_draw, NULL);
    ui_page_set_event_cb(&s_app_eb.page, eb_page_event);
    ui_page_register(&s_app_eb.page);
}

ui_page_t *app_ebook_get_page(void)
{
    return &s_app_eb.page;
}
