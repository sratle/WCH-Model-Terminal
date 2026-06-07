/********************************** (C) COPYRIGHT *******************************
* File Name          : app_subdisplay.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : Sub-display control app (V2.0).
*                      Mode switching (status/image) via CLI passthrough.
*                      BMP file browser with send-to-subdisplay.
*                      System status viewer (lsstatus output).
********************************************************************************/
#include "app_subdisplay.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define SD_LEFT_W           370
#define SD_RIGHT_X          SD_LEFT_W

/* Mode buttons */
#define SD_MODE_Y           (APP_TITLE_BAR_H + 10)
#define SD_MODE_BTN_W       160
#define SD_MODE_BTN_H       34
#define SD_MODE_GAP         8

/* Refresh button */
#define SD_REFRESH_Y        (SD_MODE_Y + SD_MODE_BTN_H + SD_MODE_GAP + 8)
#define SD_REFRESH_W        160
#define SD_REFRESH_H        32

/* BMP file list */
#define SD_LIST_LABEL_Y     (SD_REFRESH_Y + SD_REFRESH_H + 14)
#define SD_LIST_Y           (SD_LIST_LABEL_Y + 16)
#define SD_LIST_H           (UI_SCREEN_HEIGHT - SD_LIST_Y - 70)
#define SD_LIST_X           8
#define SD_LIST_W           (SD_LEFT_W - 16)
#define SD_ITEM_H           30
#define SD_VISIBLE_ITEMS    (SD_LIST_H / SD_ITEM_H)

/* Send button */
#define SD_SEND_Y           (SD_LIST_Y + SD_LIST_H + 6)
#define SD_SEND_W           (SD_LEFT_W - 16)
#define SD_SEND_H           32

/* Right panel: status area */
#define SD_STATUS_LABEL_Y   (APP_TITLE_BAR_H + 10)
#define SD_STATUS_Y         (SD_STATUS_LABEL_Y + 18)
#define SD_STATUS_H         (UI_SCREEN_HEIGHT - SD_STATUS_Y - 34)
#define SD_STATUS_LINE_H    16

/* Status bar */
#define SD_BAR_Y            (UI_SCREEN_HEIGHT - 30)
#define SD_BAR_H            30

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define SD_BG               UI_COLOR_BG_MAIN
#define SD_LIST_BG          UI_COLOR_BG_CARD
#define SD_LIST_SEL         UI_HEX(0xE0F2F1)
#define SD_LIST_ALT         UI_HEX(0xFAFAFA)
#define SD_ITEM_TEXT        UI_COLOR_TEXT_PRIMARY
#define SD_ITEM_SIZE        UI_COLOR_TEXT_SECONDARY
#define SD_STATUS_BG        UI_HEX(0xF8F8F8)
#define SD_STATUS_TEXT      UI_HEX(0x444444)
#define SD_STATUS_HDR       UI_HEX(0x333333)
#define SD_BAR_BG           UI_HEX(0xE8E8E8)
#define SD_BAR_TEXT          UI_HEX(0x555555)
#define SD_BORDER           UI_HEX(0xE0E0E0)
#define SD_LABEL            UI_COLOR_TEXT_SECONDARY

/*=============================================================================
 *  State
 *=============================================================================*/

#define SD_MAX_BMP_FILES    32
#define SD_CLI_BUF_SIZE     2048
#define SD_STATUS_BUF_SIZE  2048

typedef struct {
    char     name[20];
    uint32_t size;
    bool     is_dir;
} sd_file_entry_t;

static ui_app_page_t s_app_sd;

/* BMP file list */
static sd_file_entry_t s_bmp_files[SD_MAX_BMP_FILES];
static uint8_t  s_bmp_count;
static int16_t  s_bmp_scroll;
static int16_t  s_bmp_selected;

/* CLI response assembly */
static char    s_cli_buf[SD_CLI_BUF_SIZE];
static uint16_t s_cli_len;
static bool    s_cli_assembling;
static bool    s_cli_expect_bmp_ls;     /* expecting bmp ls response */
static bool    s_cli_expect_status;     /* expecting lsstatus response */

/* System status text (from lsstatus) */
static char    s_status_text[SD_STATUS_BUF_SIZE];
static uint16_t s_status_len;
static int16_t  s_status_scroll;

/* Status bar message */
static char    s_bar_msg[80];

/* Widgets */
static ui_button_t  btn_mode_status, btn_mode_image;
static ui_button_t  btn_refresh;
static ui_widget_t  s_list_touch;
static ui_button_t  btn_send;
static ui_widget_t  s_status_touch;
static ui_widget_t *s_sd_widgets[2 + 7];  /* back_btn + title + 6 app widgets */

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void sd_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last);
static void sd_parse_bmp_ls(const char *text, uint16_t len);
static void sd_parse_status(const char *text, uint16_t len);
static void sd_set_bar(const char *msg);
static void sd_invalidate_list(void);
static void sd_invalidate_status_area(void);
static void sd_invalidate_bar(void);

static uart_app_callbacks_t s_sd_callbacks = {
    .on_cli_response = sd_on_cli_response,
    .on_file_list    = NULL,
    .on_cwd_notify   = NULL,
};

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void sd_set_bar(const char *msg)
{
    strncpy(s_bar_msg, msg, sizeof(s_bar_msg) - 1);
    s_bar_msg[sizeof(s_bar_msg) - 1] = '\0';
    sd_invalidate_bar();
}

static void sd_invalidate_list(void)
{
    ui_rect_t r = {SD_LIST_X, SD_LIST_Y, SD_LIST_W, SD_LIST_H};
    ui_page_invalidate(&r);
}

static void sd_invalidate_status_area(void)
{
    ui_rect_t r = {SD_RIGHT_X, SD_STATUS_Y, UI_SCREEN_WIDTH - SD_RIGHT_X, SD_STATUS_H};
    ui_page_invalidate(&r);
}

static void sd_invalidate_bar(void)
{
    ui_rect_t r = {0, SD_BAR_Y, UI_SCREEN_WIDTH, SD_BAR_H};
    ui_page_invalidate(&r);
}

static void sd_clear_bmp_list(void)
{
    s_bmp_count = 0;
    s_bmp_scroll = 0;
    s_bmp_selected = -1;
    sd_invalidate_list();
}

static void sd_request_bmp_ls(void)
{
    sd_clear_bmp_list();
    s_cli_expect_bmp_ls = true;
    s_cli_expect_status = false;
    UART_SendCLI("bmp ls");
    sd_set_bar("Loading BMP files...");
}

static void sd_request_status(void)
{
    s_status_len = 0;
    s_status_text[0] = '\0';
    s_status_scroll = 0;
    s_cli_expect_status = true;
    s_cli_expect_bmp_ls = false;
    UART_SendCLI("lsstatus");
    sd_set_bar("Loading system status...");
    sd_invalidate_status_area();
}

/*=============================================================================
 *  CLI Response
 *=============================================================================*/

static void sd_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    if (!s_cli_assembling) {
        s_cli_len = 0;
        s_cli_assembling = true;
    }

    uint16_t space = SD_CLI_BUF_SIZE - 1 - s_cli_len;
    uint16_t copy_len = (len < space) ? len : space;
    if (copy_len > 0) {
        memcpy(&s_cli_buf[s_cli_len], output, copy_len);
        s_cli_len += copy_len;
    }
    s_cli_buf[s_cli_len] = '\0';

    if (!is_last)
        return;

    s_cli_assembling = false;

    if (s_cli_expect_bmp_ls) {
        sd_parse_bmp_ls(s_cli_buf, s_cli_len);
        s_cli_expect_bmp_ls = false;
    } else if (s_cli_expect_status) {
        sd_parse_status(s_cli_buf, s_cli_len);
        s_cli_expect_status = false;
    } else {
        /* Generic response — show in status bar */
        char msg[78];
        uint16_t slen = s_cli_len;
        if (slen > 77) slen = 77;
        memcpy(msg, s_cli_buf, slen);
        msg[slen] = '\0';
        for (uint16_t i = 0; i < slen; i++) {
            if (msg[i] == '\r' || msg[i] == '\n') msg[i] = ' ';
        }
        sd_set_bar(msg);
    }
}

static void sd_parse_bmp_ls(const char *text, uint16_t len)
{
    sd_clear_bmp_list();

    const char *p = text;
    const char *end = text + len;

    while (p < end && s_bmp_count < SD_MAX_BMP_FILES) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n')
            eol++;

        /* Parse line: "  [FILE] filename  (NNN bytes)" or "  [DIR] dirname" */
        char line[128];
        uint16_t llen = (uint16_t)(eol - p);
        if (llen > 127) llen = 127;
        memcpy(line, p, llen);
        line[llen] = '\0';

        const char *file_tag = strstr(line, "[FILE]");
        const char *dir_tag = strstr(line, "[DIR]");

        if (file_tag || dir_tag) {
            const char *name_start = file_tag ? file_tag + 6 : dir_tag + 5;
            while (*name_start == ' ') name_start++;

            if (file_tag) {
                s_bmp_files[s_bmp_count].is_dir = false;
                /* Extract name (up to first double-space or '(') */
                int nlen = 0;
                const char *np = name_start;
                while (*np && *np != '(' && nlen < 19) {
                    s_bmp_files[s_bmp_count].name[nlen++] = *np++;
                }
                /* Trim trailing spaces */
                while (nlen > 0 && s_bmp_files[s_bmp_count].name[nlen - 1] == ' ')
                    nlen--;
                s_bmp_files[s_bmp_count].name[nlen] = '\0';

                /* Extract size from "(NNN bytes)" */
                const char *paren = strstr(line, "(");
                if (paren) {
                    s_bmp_files[s_bmp_count].size = (uint32_t)atol(paren + 1);
                }
            } else {
                s_bmp_files[s_bmp_count].is_dir = true;
                s_bmp_files[s_bmp_count].size = 0;
                int nlen = 0;
                while (name_start[nlen] && name_start[nlen] != '\r' &&
                       name_start[nlen] != '\n' && nlen < 19) {
                    s_bmp_files[s_bmp_count].name[nlen] = name_start[nlen];
                    nlen++;
                }
                while (nlen > 0 && s_bmp_files[s_bmp_count].name[nlen - 1] == ' ')
                    nlen--;
                s_bmp_files[s_bmp_count].name[nlen] = '\0';
            }
            s_bmp_count++;
        }

        p = eol;
        while (p < end && (*p == '\r' || *p == '\n'))
            p++;
    }

    if (s_bmp_count > 0) {
        char msg[40];
        snprintf(msg, sizeof(msg), "Found %d BMP file(s)", s_bmp_count);
        sd_set_bar(msg);
    } else {
        sd_set_bar("No BMP files found");
    }
    sd_invalidate_list();
}

static void sd_parse_status(const char *text, uint16_t len)
{
    uint16_t cpy = (len < SD_STATUS_BUF_SIZE - 1) ? len : SD_STATUS_BUF_SIZE - 1;
    memcpy(s_status_text, text, cpy);
    s_status_text[cpy] = '\0';
    s_status_len = cpy;
    s_status_scroll = 0;

    sd_invalidate_status_area();
    sd_set_bar("System status loaded");
}

/*=============================================================================
 *  List Touch Handler
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int16_t rel_y = e->pos.y - SD_LIST_Y + s_bmp_scroll * SD_ITEM_H;
        int16_t idx = rel_y / SD_ITEM_H;
        if (idx >= 0 && idx < s_bmp_count) {
            int16_t old = s_bmp_selected;
            s_bmp_selected = idx;
            if (old != idx) sd_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_bmp_scroll + SD_VISIBLE_ITEMS < s_bmp_count) {
            s_bmp_scroll++;
            sd_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_bmp_scroll > 0) {
            s_bmp_scroll--;
            sd_invalidate_list();
        }
    }
}

/*=============================================================================
 *  Status Touch Handler (scroll)
 *=============================================================================*/

static void status_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_SWIPE_UP) {
        s_status_scroll += 3;
        int max_scroll = (int)s_status_len / 40;
        if (s_status_scroll > max_scroll) s_status_scroll = max_scroll;
        sd_invalidate_status_area();
    }
    else if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_status_scroll -= 3;
        if (s_status_scroll < 0) s_status_scroll = 0;
        sd_invalidate_status_area();
    }
}

/*=============================================================================
 *  Button Callbacks
 *=============================================================================*/

static void btn_mode_status_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("subdisp mode 0");
    sd_set_bar("SubDisplay: Status mode");
}

static void btn_mode_image_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("subdisp mode 1");
    sd_set_bar("SubDisplay: Image mode");
}

static void btn_refresh_click(ui_widget_t *w)
{
    (void)w;
    sd_request_status();
}

static void btn_send_click(ui_widget_t *w)
{
    (void)w;
    if (s_bmp_selected < 0 || s_bmp_selected >= s_bmp_count) {
        sd_set_bar("Select a BMP file first");
        return;
    }
    if (s_bmp_files[s_bmp_selected].is_dir) {
        sd_set_bar("Cannot send a directory");
        return;
    }

    char cmd[64];
    /* Strip extension if present for the CLI command */
    char name_no_ext[20];
    strncpy(name_no_ext, s_bmp_files[s_bmp_selected].name, 19);
    name_no_ext[19] = '\0';
    char *dot = strchr(name_no_ext, '.');
    if (dot) *dot = '\0';

    snprintf(cmd, sizeof(cmd), "bmp get %s sub", name_no_ext);
    UART_SendCLI(cmd);

    char msg[60];
    snprintf(msg, sizeof(msg), "Sending %s to SubDisplay...", s_bmp_files[s_bmp_selected].name);
    sd_set_bar(msg);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void sd_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_sd_callbacks);
    sd_request_bmp_ls();
    /* Status loaded on demand via Refresh Status button */
}

static void sd_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearAppCallbacks();
}

static void sd_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Left panel */
    if (dirty_top < UI_SCREEN_HEIGHT - SD_BAR_H && dirty->x < SD_LEFT_W) {
        /* Section labels */
        ui_draw_text(12, SD_MODE_Y - 2, "Display Mode", &font_montserrat_12, SD_LABEL);
        ui_draw_text(12, SD_LIST_LABEL_Y, "BMP Files", &font_montserrat_12, SD_LABEL);

        /* List background */
        if (dirty_bot > SD_LIST_Y) {
            ui_rect_t list_bg = {SD_LIST_X, SD_LIST_Y, SD_LIST_W, SD_LIST_H};
            ui_draw_fill_rect(&list_bg, SD_LIST_BG);
            ui_draw_rect_border(&list_bg, SD_BORDER, 1);

            /* List items */
            for (int16_t vis = 0; vis < SD_VISIBLE_ITEMS; vis++) {
                int16_t idx = s_bmp_scroll + vis;
                if (idx >= s_bmp_count) break;

                int16_t item_y = SD_LIST_Y + vis * SD_ITEM_H;
                if (item_y + SD_ITEM_H > SD_LIST_Y + SD_LIST_H) break;

                ui_rect_t item_rect = {SD_LIST_X + 1, item_y, SD_LIST_W - 2, SD_ITEM_H};

                if (idx == s_bmp_selected) {
                    ui_draw_fill_rect(&item_rect, SD_LIST_SEL);
                } else if (idx & 1) {
                    ui_draw_fill_rect(&item_rect, SD_LIST_ALT);
                }

                /* Icon indicator */
                const char *icon = s_bmp_files[idx].is_dir ? "[D]" : "[F]";
                ui_color_t icon_color = s_bmp_files[idx].is_dir ? UI_HEX(0xFFA726) : UI_HEX(0x66BB6A);
                ui_draw_text(SD_LIST_X + 8, item_y + 8, icon, &font_montserrat_12, icon_color);

                /* File name */
                ui_draw_text(SD_LIST_X + 36, item_y + 8, s_bmp_files[idx].name,
                             &font_montserrat_12, SD_ITEM_TEXT);

                /* File size */
                if (!s_bmp_files[idx].is_dir && s_bmp_files[idx].size > 0) {
                    char size_text[16];
                    snprintf(size_text, sizeof(size_text), "%luB", (unsigned long)s_bmp_files[idx].size);
                    int16_t sw = ui_text_width(size_text, &font_montserrat_12);
                    ui_draw_text(SD_LIST_X + SD_LIST_W - sw - 10, item_y + 8,
                                 size_text, &font_montserrat_12, SD_ITEM_SIZE);
                }
            }

            /* Scroll indicator */
            if (s_bmp_count > SD_VISIBLE_ITEMS) {
                int16_t sb_h = SD_LIST_H;
                int16_t thumb_h = sb_h * SD_VISIBLE_ITEMS / (s_bmp_count + 1);
                if (thumb_h < 16) thumb_h = 16;
                int16_t thumb_y = SD_LIST_Y +
                                  (sb_h - thumb_h) * s_bmp_scroll /
                                  (s_bmp_count - SD_VISIBLE_ITEMS + 1);
                ui_rect_t thumb = {SD_LIST_X + SD_LIST_W - 5, thumb_y, 3, thumb_h};
                ui_draw_fill_round_rect(&thumb, 1, UI_HEX(0xBDBDBD));
            }
        }
    }

    /* Right panel: system status */
    if (dirty_bot > SD_STATUS_Y && dirty->x + dirty->w > SD_RIGHT_X) {
        ui_draw_text(SD_RIGHT_X + 12, SD_STATUS_LABEL_Y, "System Status",
                     &font_montserrat_12, SD_LABEL);

        ui_rect_t status_bg = {SD_RIGHT_X + 4, SD_STATUS_Y,
                               UI_SCREEN_WIDTH - SD_RIGHT_X - 8, SD_STATUS_H};
        ui_draw_fill_rect(&status_bg, SD_STATUS_BG);
        ui_draw_rect_border(&status_bg, SD_BORDER, 1);

        /* Render status text lines */
        if (s_status_len > 0) {
            const char *p = s_status_text;
            int16_t line_idx = 0;
            int16_t ty = SD_STATUS_Y + 4;
            int16_t max_y = SD_STATUS_Y + SD_STATUS_H - SD_STATUS_LINE_H;

            while (*p && ty <= max_y) {
                /* Skip lines for scroll offset */
                if (line_idx >= s_status_scroll) {
                    /* Extract one line */
                    char line[80];
                    int llen = 0;
                    while (*p && *p != '\r' && *p != '\n' && llen < 79) {
                        line[llen++] = *p++;
                    }
                    line[llen] = '\0';

                    /* Skip \r\n */
                    if (*p == '\r') p++;
                    if (*p == '\n') p++;

                    /* Section headers (lines starting with ===) */
                    ui_color_t text_color = SD_STATUS_TEXT;
                    if (strstr(line, "===") != NULL) {
                        text_color = SD_STATUS_HDR;
                    } else if (line[0] == ' ' && line[1] == ' ') {
                        text_color = UI_COLOR_TEXT_SECONDARY;
                    }

                    ui_draw_text(SD_RIGHT_X + 10, ty, line,
                                 &font_montserrat_12, text_color);
                    ty += SD_STATUS_LINE_H;
                } else {
                    /* Skip this line */
                    while (*p && *p != '\r' && *p != '\n') p++;
                    if (*p == '\r') p++;
                    if (*p == '\n') p++;
                }
                line_idx++;
            }
        } else {
            ui_draw_text(SD_RIGHT_X + 10, SD_STATUS_Y + 8,
                         "Loading...", &font_montserrat_12, SD_STATUS_TEXT);
        }
    }

    /* Status bar */
    if (dirty_bot > SD_BAR_Y) {
        ui_rect_t bar_bg = {0, SD_BAR_Y, UI_SCREEN_WIDTH, SD_BAR_H};
        ui_draw_fill_rect(&bar_bg, SD_BAR_BG);
        ui_draw_text(12, SD_BAR_Y + 8, s_bar_msg, &font_montserrat_12, SD_BAR_TEXT);
    }
}

static bool sd_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_bmp_selected < s_bmp_count - 1) {
            s_bmp_selected++;
            if (s_bmp_selected >= s_bmp_scroll + SD_VISIBLE_ITEMS)
                s_bmp_scroll++;
            sd_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_bmp_selected > 0) {
            s_bmp_selected--;
            if (s_bmp_selected < s_bmp_scroll)
                s_bmp_scroll--;
            sd_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_OK) {
        if (s_bmp_selected >= 0 && s_bmp_selected < s_bmp_count) {
            btn_send_click(NULL);
        }
        return true;
    }

    return false;
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void app_subdisplay_init(void)
{
    ui_app_page_init(&s_app_sd, "SubDisp", 0x10A);

    /* State */
    s_bmp_count = 0;
    s_bmp_scroll = 0;
    s_bmp_selected = -1;
    s_cli_len = 0;
    s_cli_assembling = false;
    s_cli_expect_bmp_ls = false;
    s_cli_expect_status = false;
    s_status_len = 0;
    s_status_scroll = 0;
    s_status_text[0] = '\0';
    strcpy(s_bar_msg, "Ready");

    int widx = 0;

    /* Back button and title (from ui_app_page_t) */
    s_sd_widgets[widx++] = (ui_widget_t *)&s_app_sd.btn_back;
    s_sd_widgets[widx++] = (ui_widget_t *)&s_app_sd.lbl_title;

    /* Mode buttons */
    {
        ui_rect_t r1 = {12, SD_MODE_Y, SD_MODE_BTN_W, SD_MODE_BTN_H};
        ui_button_init(&btn_mode_status, &r1, "Status Mode", &font_montserrat_12);
        ui_button_set_callback(&btn_mode_status, btn_mode_status_click);
        ui_button_set_colors(&btn_mode_status, UI_COLOR_BG_CARD, UI_COLOR_PRIMARY, UI_COLOR_TEXT_PRIMARY);
        btn_mode_status.radius = 8;
        s_sd_widgets[widx++] = (ui_widget_t *)&btn_mode_status;

        ui_rect_t r2 = {12 + SD_MODE_BTN_W + SD_MODE_GAP, SD_MODE_Y, SD_MODE_BTN_W, SD_MODE_BTN_H};
        ui_button_init(&btn_mode_image, &r2, "Image Mode", &font_montserrat_12);
        ui_button_set_callback(&btn_mode_image, btn_mode_image_click);
        ui_button_set_colors(&btn_mode_image, UI_COLOR_BG_CARD, UI_COLOR_PRIMARY, UI_COLOR_TEXT_PRIMARY);
        btn_mode_image.radius = 8;
        s_sd_widgets[widx++] = (ui_widget_t *)&btn_mode_image;
    }

    /* Refresh button */
    {
        ui_rect_t r = {12, SD_REFRESH_Y, SD_REFRESH_W, SD_REFRESH_H};
        ui_button_init(&btn_refresh, &r, "Refresh Status", &font_montserrat_12);
        ui_button_set_callback(&btn_refresh, btn_refresh_click);
        ui_button_set_colors(&btn_refresh, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
        btn_refresh.radius = 8;
        s_sd_widgets[widx++] = (ui_widget_t *)&btn_refresh;
    }

    /* BMP list touch */
    {
        ui_rect_t r = {SD_LIST_X, SD_LIST_Y, SD_LIST_W, SD_LIST_H};
        ui_widget_init(&s_list_touch, &r);
        s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_list_touch.event_cb = list_touch_event;
        s_sd_widgets[widx++] = &s_list_touch;
    }

    /* Send button */
    {
        ui_rect_t r = {SD_LIST_X, SD_SEND_Y, SD_SEND_W, SD_SEND_H};
        ui_button_init(&btn_send, &r, "Send to SubDisplay", &font_montserrat_12);
        ui_button_set_callback(&btn_send, btn_send_click);
        ui_button_set_colors(&btn_send, UI_COLOR_PRIMARY, UI_HEX(0x5BA8A8), UI_COLOR_WHITE);
        btn_send.radius = 8;
        s_sd_widgets[widx++] = (ui_widget_t *)&btn_send;
    }

    /* Status area touch (for scrolling) */
    {
        ui_rect_t r = {SD_RIGHT_X + 4, SD_STATUS_Y, UI_SCREEN_WIDTH - SD_RIGHT_X - 8, SD_STATUS_H};
        ui_widget_init(&s_status_touch, &r);
        s_status_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_status_touch.event_cb = status_touch_event;
        s_sd_widgets[widx++] = &s_status_touch;
    }

    ui_page_set_widgets(&s_app_sd.page, s_sd_widgets, (uint16_t)widx);
    ui_page_set_callbacks(&s_app_sd.page, sd_page_enter, sd_page_exit, sd_page_draw, NULL);
    ui_page_set_event_cb(&s_app_sd.page, sd_page_event);
    ui_page_register(&s_app_sd.page);
}

ui_page_t *app_subdisplay_get_page(void)
{
    return &s_app_sd.page;
}
