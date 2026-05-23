/********************************** (C) COPYRIGHT *******************************
* File Name          : app_editor.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/05/23
* Description        : Text Editor / Viewer app.
*                      Displays text file content received from Core via bulk
*                      transfer.  Supports scroll and status display.
********************************************************************************/
#include "app_editor.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define EDIT_STATUS_H       28
#define EDIT_STATUS_Y       (UI_SCREEN_HEIGHT - EDIT_STATUS_H)
#define EDIT_GUTTER_W       48
#define EDIT_TEXT_X         EDIT_GUTTER_W
#define EDIT_TEXT_Y         APP_TITLE_BAR_H
#define EDIT_TEXT_W         (UI_SCREEN_WIDTH - EDIT_GUTTER_W)
#define EDIT_TEXT_H         (EDIT_STATUS_Y - APP_TITLE_BAR_H)
#define EDIT_LINE_H         18
#define EDIT_VISIBLE_LINES  (EDIT_TEXT_H / EDIT_LINE_H)
#define EDIT_BUF_SIZE       4096
#define EDIT_MAX_LINES      256

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define EDIT_BG             UI_HEX(0xFAFAFA)
#define EDIT_GUTTER_BG      UI_HEX(0xF0F0F0)
#define EDIT_GUTTER_TEXT    UI_HEX(0xAAAAAA)
#define EDIT_TEXT_COLOR     UI_HEX(0x333333)
#define EDIT_STATUS_BG      UI_HEX(0xECEFF1)
#define EDIT_STATUS_TEXT    UI_HEX(0x607D8B)
#define EDIT_BORDER         UI_HEX(0xE0E0E0)

/*=============================================================================
 *  Editor State
 *=============================================================================*/

typedef struct {
    char file_name[32];
    char file_path[64];
    char content[EDIT_BUF_SIZE];
    uint16_t content_len;
    uint16_t line_offsets[EDIT_MAX_LINES];
    uint16_t line_count;
    int16_t scroll_line;
    bool is_modified;
    bool is_loading;
    bool has_content;
} editor_state_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_app_editor;
static ui_button_t btn_save;
static ui_widget_t s_text_touch;
static ui_widget_t *s_editor_widgets[5];
static editor_state_t s_ed;
static char s_status_buf[80];

/*=============================================================================
 *  Line Parsing
 *=============================================================================*/

static void editor_parse_lines(void)
{
    s_ed.line_count = 0;
    s_ed.line_offsets[0] = 0;
    s_ed.line_count = 1;
    for (uint16_t i = 0; i < s_ed.content_len && s_ed.line_count < EDIT_MAX_LINES; i++) {
        if (s_ed.content[i] == '\n' && i + 1 < s_ed.content_len) {
            s_ed.line_offsets[s_ed.line_count] = i + 1;
            s_ed.line_count++;
        }
    }
}

static void editor_update_status(void)
{
    if (s_ed.is_loading)
        snprintf(s_status_buf, sizeof(s_status_buf), "Loading...");
    else if (!s_ed.has_content)
        snprintf(s_status_buf, sizeof(s_status_buf), "No file loaded");
    else
        snprintf(s_status_buf, sizeof(s_status_buf), "%d lines | %d chars%s",
                 s_ed.line_count, s_ed.content_len,
                 s_ed.is_modified ? " [Modified]" : "");
}

/*=============================================================================
 *  Protocol Callbacks
 *=============================================================================*/

static void on_bulk_data_received(const uint8_t *data, uint16_t len, bool is_last)
{
    uint16_t space = EDIT_BUF_SIZE - 1 - s_ed.content_len;
    uint16_t copy_len = (len < space) ? len : space;
    if (copy_len > 0) {
        memcpy(&s_ed.content[s_ed.content_len], data, copy_len);
        s_ed.content_len += copy_len;
    }
    if (is_last) {
        s_ed.content[s_ed.content_len] = '\0';
        s_ed.is_loading = false;
        s_ed.has_content = true;
        s_ed.is_modified = false;
        s_ed.scroll_line = 0;
        editor_parse_lines();
        editor_update_status();
        ui_page_invalidate_all();
    }
}

static void on_bulk_complete_cb(bool success, uint32_t total_size)
{
    (void)total_size;
    s_ed.is_loading = false;
    if (success && s_ed.content_len > 0) {
        s_ed.content[s_ed.content_len] = '\0';
        s_ed.has_content = true;
        s_ed.is_modified = false;
        s_ed.scroll_line = 0;
        editor_parse_lines();
    }
    editor_update_status();
    ui_page_invalidate_all();
}

static uart_app_callbacks_t s_editor_callbacks = {
    .on_file_list = NULL,
    .on_file_op_result = NULL,
    .on_play_music_result = NULL,
    .on_bulk_data = on_bulk_data_received,
    .on_bulk_complete = on_bulk_complete_cb,
};

/*=============================================================================
 *  File Operations
 *=============================================================================*/

void app_editor_open_file(const char *path, const char *name)
{
    memset(s_ed.content, 0, sizeof(s_ed.content));
    s_ed.content_len = 0;
    s_ed.line_count = 0;
    s_ed.scroll_line = 0;
    s_ed.is_modified = false;
    s_ed.is_loading = true;
    s_ed.has_content = false;

    strncpy(s_ed.file_name, name, sizeof(s_ed.file_name) - 1);
    s_ed.file_name[sizeof(s_ed.file_name) - 1] = '\0';
    strncpy(s_ed.file_path, path, sizeof(s_ed.file_path) - 1);
    s_ed.file_path[sizeof(s_ed.file_path) - 1] = '\0';

    ui_label_set_text(&s_app_editor.lbl_title, s_ed.file_name);
    UART_SetAppCallbacks(&s_editor_callbacks);
    UART_RequestFileRead(s_ed.file_path);
    editor_update_status();
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void editor_draw_gutter(void)
{
    ui_rect_t gbg = {0, EDIT_TEXT_Y, EDIT_GUTTER_W, EDIT_TEXT_H};
    ui_draw_fill_rect(&gbg, EDIT_GUTTER_BG);
    ui_draw_vline(EDIT_GUTTER_W - 1, EDIT_TEXT_Y, EDIT_TEXT_H, EDIT_BORDER);

    for (int i = 0; i < EDIT_VISIBLE_LINES; i++) {
        int ln = s_ed.scroll_line + i + 1;
        if (ln > s_ed.line_count) break;
        char nb[8];
        snprintf(nb, sizeof(nb), "%d", ln);
        int16_t y = EDIT_TEXT_Y + i * EDIT_LINE_H + 2;
        int16_t nw = ui_text_width(nb, &font_montserrat_12);
        ui_draw_text(EDIT_GUTTER_W - nw - 8, y, nb, &font_montserrat_12, EDIT_GUTTER_TEXT);
    }
}

static void editor_draw_text_area(void)
{
    ui_rect_t tbg = {EDIT_TEXT_X, EDIT_TEXT_Y, EDIT_TEXT_W, EDIT_TEXT_H};
    ui_draw_fill_rect(&tbg, EDIT_BG);

    if (!s_ed.has_content) {
        ui_draw_text_in_rect(&tbg,
            s_ed.is_loading ? "Loading file..." : "No file loaded",
            &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 0x11);
        return;
    }

    for (int i = 0; i < EDIT_VISIBLE_LINES; i++) {
        int li = s_ed.scroll_line + i;
        if (li >= s_ed.line_count) break;

        uint16_t start = s_ed.line_offsets[li];
        uint16_t end = (li + 1 < s_ed.line_count) ?
                        s_ed.line_offsets[li + 1] : s_ed.content_len;
        char lb[128];
        uint16_t ll = end - start;
        if (ll > sizeof(lb) - 1) ll = sizeof(lb) - 1;
        memcpy(lb, &s_ed.content[start], ll);
        lb[ll] = '\0';
        while (ll > 0 && (lb[ll - 1] == '\n' || lb[ll - 1] == '\r'))
            lb[--ll] = '\0';

        int16_t y = EDIT_TEXT_Y + i * EDIT_LINE_H + 2;
        ui_draw_text(EDIT_TEXT_X + 8, y, lb, &font_montserrat_12, EDIT_TEXT_COLOR);
    }
}

static void editor_draw_status(void)
{
    ui_rect_t sbg = {0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_STATUS_H};
    ui_draw_fill_rect(&sbg, EDIT_STATUS_BG);
    ui_draw_hline(0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_BORDER);
    ui_draw_text(12, EDIT_STATUS_Y + 6, s_ed.file_name,
                 &font_montserrat_12, EDIT_STATUS_TEXT);
    int16_t sw = ui_text_width(s_status_buf, &font_montserrat_12);
    ui_draw_text(UI_SCREEN_WIDTH - sw - 12, EDIT_STATUS_Y + 6,
                 s_status_buf, &font_montserrat_12, EDIT_STATUS_TEXT);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void editor_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_editor_callbacks);
    editor_update_status();
    ui_page_invalidate_all();
}

static void editor_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page; (void)dirty;
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);
    editor_draw_gutter();
    editor_draw_text_area();
    editor_draw_status();
}

/*=============================================================================
 *  Widget Event Handlers
 *=============================================================================*/

static void text_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_SWIPE_UP) {
        int max_s = s_ed.line_count - EDIT_VISIBLE_LINES;
        if (max_s < 0) max_s = 0;
        s_ed.scroll_line += 5;
        if (s_ed.scroll_line > max_s) s_ed.scroll_line = max_s;
        ui_page_invalidate_all();
    } else if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_ed.scroll_line -= 5;
        if (s_ed.scroll_line < 0) s_ed.scroll_line = 0;
        ui_page_invalidate_all();
    }
}

static void btn_save_click(ui_widget_t *w)
{
    (void)w;
    if (s_ed.has_content && s_ed.is_modified) {
        UART_SetAppCallbacks(&s_editor_callbacks);
        UART_RequestFileSave(s_ed.file_path);
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void app_editor_init(void)
{
    ui_app_page_init(&s_app_editor, "Editor", 0x102);
    memset(&s_ed, 0, sizeof(s_ed));
    strcpy(s_ed.file_name, "(no file)");
    editor_update_status();

    ui_rect_t r_save = {UI_SCREEN_WIDTH - 80, 6, 64, 28};
    ui_button_init(&btn_save, &r_save, "Save", &font_montserrat_12);
    ui_button_set_callback(&btn_save, btn_save_click);
    ui_button_set_colors(&btn_save, UI_COLOR_WHITE, UI_COLOR_SECONDARY, UI_COLOR_PRIMARY);
    btn_save.radius = 8;

    ui_rect_t touch_rect = {EDIT_TEXT_X, EDIT_TEXT_Y, EDIT_TEXT_W, EDIT_TEXT_H};
    ui_widget_init(&s_text_touch, &touch_rect);
    s_text_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_text_touch.event_cb = text_touch_event;

    s_editor_widgets[0] = (ui_widget_t *)&s_app_editor.btn_back;
    s_editor_widgets[1] = (ui_widget_t *)&s_app_editor.lbl_title;
    s_editor_widgets[2] = (ui_widget_t *)&btn_save;
    s_editor_widgets[3] = &s_text_touch;

    ui_page_set_widgets(&s_app_editor.page, s_editor_widgets, 4);
    ui_page_set_callbacks(&s_app_editor.page, editor_page_enter, NULL, editor_page_draw, NULL);
    ui_page_register(&s_app_editor.page);
}

ui_page_t *app_editor_get_page(void) { return &s_app_editor.page; }
