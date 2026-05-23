/********************************** (C) COPYRIGHT *******************************
* File Name          : app_file.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/05/23
* Description        : File Manager app.
*                      Browse directories, view files, create/delete.
*                      Communicates with Core via UART protocol.
********************************************************************************/
#include "app_file.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define FILE_TOOLBAR_H      40
#define FILE_TOOLBAR_Y      APP_TITLE_BAR_H
#define FILE_LIST_Y         (FILE_TOOLBAR_Y + FILE_TOOLBAR_H)
#define FILE_LIST_H         (UI_SCREEN_HEIGHT - FILE_LIST_Y)
#define FILE_ITEM_H         40
#define FILE_VISIBLE_ITEMS  (FILE_LIST_H / FILE_ITEM_H)
#define FILE_MAX_ENTRIES    FILE_LIST_MAX_ENTRIES
#define FILE_PATH_MAX       64
#define TB_BTN_W            60
#define TB_BTN_H            32
#define TB_BTN_Y            (FILE_TOOLBAR_Y + 4)

#define FILE_BG             UI_COLOR_BG_MAIN
#define FILE_ITEM_BG        UI_COLOR_BG_CARD
#define FILE_ITEM_SEL       UI_HEX(0xE8F5E9)
#define FILE_ITEM_ALT       UI_HEX(0xFAFAFA)
#define FILE_DIR_COLOR      UI_HEX(0xFFA726)
#define FILE_FILE_COLOR     UI_HEX(0x42A5F5)
#define FILE_SIZE_COLOR     UI_COLOR_TEXT_SECONDARY
#define FILE_BORDER         UI_HEX(0xE0E0E0)

/*=============================================================================
 *  File Manager State
 *=============================================================================*/

typedef struct {
    char path[FILE_PATH_MAX];
    file_entry_t entries[FILE_MAX_ENTRIES];
    int16_t count;
    int16_t selected;
    int16_t scroll_offset;
    bool loading;
    bool data_ready;
    uint8_t last_status;
} file_state_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_app_file;
static ui_widget_t s_list_touch;
static ui_button_t btn_up, btn_new_folder, btn_delete, btn_refresh;
static ui_widget_t *s_file_widgets[8];
static file_state_t s_fs;
static char s_status_text[80];
static char s_path_display[FILE_PATH_MAX + 8];

/* 防抖：同一目录 2 秒内不重复请求 */
static uint32_t s_last_request_ms = 0;
static char s_last_request_path[FILE_PATH_MAX];

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void file_request_list(void);
static void file_sort_entries(void);
static void file_update_status(void);
static void file_draw_list(void);
static void file_draw_toolbar(void);
static void file_enter_path(const char *name);
static void file_go_up(void);

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static bool file_is_wav(const char *name)
{
    int len = strlen(name);
    if (len < 5) return false;
    const char *ext = name + len - 4;
    return (ext[0] == '.' &&
            (ext[1] == 'w' || ext[1] == 'W') &&
            (ext[2] == 'a' || ext[2] == 'A') &&
            (ext[3] == 'v' || ext[3] == 'V'));
}

static bool file_is_text(const char *name)
{
    int len = strlen(name);
    if (len < 3) return false;
    const char *ext = name + len - 4;
    if (ext >= name && ext[0] == '.') {
        if ((ext[1] == 't' || ext[1] == 'T') && (ext[2] == 'x' || ext[2] == 'X') &&
            (ext[3] == 't' || ext[3] == 'T')) return true;
        if ((ext[1] == 'j' || ext[1] == 'J') && (ext[2] == 's' || ext[2] == 'S') &&
            (ext[3] == 'o' || ext[3] == 'O')) return true;
    }
    ext = name + len - 3;
    if (ext >= name && ext[0] == '.') {
        if ((ext[1] == 'm' || ext[1] == 'M') && (ext[2] == 'd' || ext[2] == 'D')) return true;
    }
    ext = name + len - 2;
    if (ext >= name && ext[0] == '.') {
        if (ext[1] == 'c' || ext[1] == 'C') return true;
        if (ext[1] == 'h' || ext[1] == 'H') return true;
    }
    return false;
}

static void file_format_size(uint32_t size, char *buf, int buf_len)
{
    if (size < 1024)
        snprintf(buf, buf_len, "%u B", (unsigned)size);
    else if (size < 1024 * 1024)
        snprintf(buf, buf_len, "%u.%u KB", (unsigned)(size / 1024),
                 (unsigned)((size % 1024) * 10 / 1024));
    else
        snprintf(buf, buf_len, "%u.%u MB", (unsigned)(size / (1024 * 1024)),
                 (unsigned)(((size / 1024) % 1024) * 10 / 1024));
}

static int file_cmp_name(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

static void file_sort_entries(void)
{
    for (int i = 0; i < s_fs.count - 1; i++) {
        for (int j = i + 1; j < s_fs.count; j++) {
            bool di = (s_fs.entries[i].attr & FILE_ATTR_IS_DIR) != 0;
            bool dj = (s_fs.entries[j].attr & FILE_ATTR_IS_DIR) != 0;
            bool swap = false;
            if (di != dj) swap = dj;
            else if (file_cmp_name(s_fs.entries[i].name, s_fs.entries[j].name) > 0) swap = true;
            if (swap) {
                file_entry_t tmp = s_fs.entries[i];
                s_fs.entries[i] = s_fs.entries[j];
                s_fs.entries[j] = tmp;
            }
        }
    }
}

/*=============================================================================
 *  Protocol Callbacks
 *=============================================================================*/

static void on_file_list_received(uint8_t status, const file_entry_t *entries, uint8_t count)
{
    s_fs.loading = false;
    s_fs.data_ready = true;
    s_fs.last_status = status;
    if (status == 0x00) {
        s_fs.count = (count > FILE_MAX_ENTRIES) ? FILE_MAX_ENTRIES : count;
        memcpy(s_fs.entries, entries, s_fs.count * sizeof(file_entry_t));
        file_sort_entries();
    } else {
        s_fs.count = 0;
    }
    s_fs.selected = -1;
    s_fs.scroll_offset = 0;
    file_update_status();
    ui_page_invalidate_all();
}

static void on_file_op_done(bool success, uint8_t error_code)
{
    (void)error_code;
    s_fs.loading = false;
    if (success) file_request_list();
    ui_page_invalidate_all();
}

static void on_cd_done(bool success, const char *cwd)
{
    if (success) {
        /* Update display-side path from Core's CWD */
        if (cwd[0] == '\\')
            strncpy(s_fs.path, cwd + 1, FILE_PATH_MAX - 1);
        else
            strncpy(s_fs.path, cwd, FILE_PATH_MAX - 1);
        s_fs.path[FILE_PATH_MAX - 1] = '\0';
        /* Strip trailing backslash for consistent display */
        int len = strlen(s_fs.path);
        if (len > 0 && s_fs.path[len - 1] == '\\') s_fs.path[len - 1] = '\0';
        /* Now request the file list for the new CWD */
        file_request_list();
    } else {
        s_fs.loading = false;
        snprintf(s_status_text, sizeof(s_status_text), "CD failed");
        ui_page_invalidate_all();
    }
}

static uart_app_callbacks_t s_file_callbacks = {
    .on_file_list = on_file_list_received,
    .on_file_op_result = on_file_op_done,
    .on_play_music_result = NULL,
    .on_bulk_data = NULL,
    .on_bulk_complete = NULL,
    .on_cd_result = on_cd_done,
    .on_cli_response = NULL,
};

/*=============================================================================
 *  File Operations
 *=============================================================================*/

static void file_request_list(void)
{
    /* 音乐播放中 CH378 被占用，提示用户 */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING ||
        g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        snprintf(s_status_text, sizeof(s_status_text), "Stop music first");
        s_fs.loading = false;
        s_fs.count = 0;
        ui_page_invalidate_all();
        return;
    }

    /* 防抖：同一路径 2 秒内不重复请求（除非强制刷新） */
    uint32_t now = ui_get_real_ms();
    if (strcmp(s_fs.path, s_last_request_path) == 0 &&
        (now - s_last_request_ms) < 2000 && s_fs.data_ready) {
        return;
    }

    s_fs.loading = true;
    s_fs.data_ready = false;
    s_last_request_ms = now;
    strncpy(s_last_request_path, s_fs.path, FILE_PATH_MAX - 1);
    s_last_request_path[FILE_PATH_MAX - 1] = '\0';
    UART_SetAppCallbacks(&s_file_callbacks);
    /* Request file list for Core's current CWD (empty path = list CWD) */
    UART_RequestFileList("");
    file_update_status();
    ui_page_invalidate_all();
}

static void file_enter_path(const char *name)
{
    /* Two-step: first CD into directory on Core, then list */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING ||
        g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        snprintf(s_status_text, sizeof(s_status_text), "Stop music first");
        ui_page_invalidate_all();
        return;
    }

    s_fs.loading = true;
    UART_SetAppCallbacks(&s_file_callbacks);
    UART_SendCD(name);
    file_update_status();
    ui_page_invalidate_all();
}

static void file_go_up(void)
{
    /* Two-step: CD .. on Core, then list */
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING ||
        g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        snprintf(s_status_text, sizeof(s_status_text), "Stop music first");
        ui_page_invalidate_all();
        return;
    }

    s_fs.loading = true;
    UART_SetAppCallbacks(&s_file_callbacks);
    UART_SendCD("..");
    file_update_status();
    ui_page_invalidate_all();
}

static void file_delete_selected(void)
{
    if (s_fs.selected < 0 || s_fs.selected >= s_fs.count) return;
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING ||
        g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        snprintf(s_status_text, sizeof(s_status_text), "Stop music first");
        ui_page_invalidate_all();
        return;
    }
    file_entry_t *e = &s_fs.entries[s_fs.selected];
    char fullpath[FILE_PATH_MAX];
    int plen = strlen(s_fs.path);
    snprintf(fullpath, sizeof(fullpath), "%s%s%s", s_fs.path,
             (plen > 0 && s_fs.path[plen - 1] != '\\') ? "\\" : "", e->name);
    s_fs.loading = true;
    UART_SetAppCallbacks(&s_file_callbacks);
    if (e->attr & FILE_ATTR_IS_DIR)
        UART_SendFileOperation(FILE_OP_DELETE_DIR, fullpath);
    else
        UART_SendFileOperation(FILE_OP_DELETE_FILE, fullpath);
    file_update_status();
    ui_page_invalidate_all();
}

static void file_create_folder(void)
{
    if (g_disp_state.music_state == MUSIC_STATE_PLAYING ||
        g_disp_state.music_state == MUSIC_STATE_PAUSED) {
        snprintf(s_status_text, sizeof(s_status_text), "Stop music first");
        ui_page_invalidate_all();
        return;
    }
    char fullpath[FILE_PATH_MAX];
    int plen = strlen(s_fs.path);
    snprintf(fullpath, sizeof(fullpath), "%s%s%s", s_fs.path,
             (plen > 0 && s_fs.path[plen - 1] != '\\') ? "\\" : "", "NewFolder");
    s_fs.loading = true;
    UART_SetAppCallbacks(&s_file_callbacks);
    UART_SendFileOperation(FILE_OP_MKDIR, fullpath);
    file_update_status();
    ui_page_invalidate_all();
}

static void file_open_selected(void)
{
    if (s_fs.selected < 0 || s_fs.selected >= s_fs.count) return;
    file_entry_t *e = &s_fs.entries[s_fs.selected];
    if (e->attr & FILE_ATTR_IS_DIR) { file_enter_path(e->name); return; }

    char fullpath[FILE_PATH_MAX];
    int plen = strlen(s_fs.path);
    snprintf(fullpath, sizeof(fullpath), "%s%s%s", s_fs.path,
             (plen > 0 && s_fs.path[plen - 1] != '\\') ? "\\" : "", e->name);
    if (file_is_wav(e->name)) {
        UART_SendPlayMusic(fullpath);
    } else if (file_is_text(e->name)) {
        UART_RequestFileRead(fullpath);
    }
}

static void file_update_status(void)
{
    if (s_fs.loading)
        snprintf(s_status_text, sizeof(s_status_text), "Loading...");
    else if (!s_fs.data_ready)
        snprintf(s_status_text, sizeof(s_status_text), "Ready");
    else if (s_fs.last_status != 0)
        snprintf(s_status_text, sizeof(s_status_text), "Error %d", s_fs.last_status);
    else
        snprintf(s_status_text, sizeof(s_status_text), "%d items", s_fs.count);

    if (s_fs.path[0] == '\0')
        snprintf(s_path_display, sizeof(s_path_display), "\\ (Root)");
    else
        snprintf(s_path_display, sizeof(s_path_display), "\\%s", s_fs.path);
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void file_draw_toolbar(void)
{
    ui_rect_t tb_bg = {0, FILE_TOOLBAR_Y, UI_SCREEN_WIDTH, FILE_TOOLBAR_H};
    ui_draw_fill_rect(&tb_bg, UI_HEX(0xF0F0F0));
    ui_draw_text(10, FILE_TOOLBAR_Y + 12, s_path_display,
                 &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
    ui_draw_hline(0, FILE_TOOLBAR_Y + FILE_TOOLBAR_H - 1, UI_SCREEN_WIDTH, FILE_BORDER);
}

static void file_draw_list(void)
{
    ui_rect_t list_bg = {0, FILE_LIST_Y, UI_SCREEN_WIDTH, FILE_LIST_H};
    ui_draw_fill_rect(&list_bg, FILE_BG);

    if (s_fs.loading) {
        ui_draw_text_in_rect(&list_bg, "Loading...", &font_montserrat_16,
                             UI_COLOR_TEXT_SECONDARY, 0x11);
        return;
    }
    if (s_fs.count == 0) {
        ui_draw_text_in_rect(&list_bg, "Empty directory", &font_montserrat_16,
                             UI_COLOR_TEXT_SECONDARY, 0x11);
        return;
    }

    for (int i = 0; i < FILE_VISIBLE_ITEMS && (i + s_fs.scroll_offset) < s_fs.count; i++) {
        int idx = i + s_fs.scroll_offset;
        file_entry_t *e = &s_fs.entries[idx];
        int16_t y = FILE_LIST_Y + i * FILE_ITEM_H;
        ui_rect_t item_rect = {0, y, UI_SCREEN_WIDTH, FILE_ITEM_H};
        ui_color_t bg = (idx == s_fs.selected) ? FILE_ITEM_SEL :
                        (i % 2 == 0) ? FILE_ITEM_BG : FILE_ITEM_ALT;
        ui_draw_fill_rect(&item_rect, bg);
        ui_draw_hline(10, y + FILE_ITEM_H - 1, UI_SCREEN_WIDTH - 20, FILE_BORDER);

        bool is_dir = (e->attr & FILE_ATTR_IS_DIR) != 0;
        ui_color_t icon_color = is_dir ? FILE_DIR_COLOR : FILE_FILE_COLOR;
        ui_draw_text(16, y + 12, is_dir ? "[D]" : "[F]", &font_montserrat_12, icon_color);
        ui_draw_text(52, y + 6, e->name, &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);

        if (!is_dir && e->size > 0) {
            char size_buf[16];
            file_format_size(e->size, size_buf, sizeof(size_buf));
            ui_draw_text(52, y + 24, size_buf, &font_montserrat_12, FILE_SIZE_COLOR);
        }
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void file_page_enter(ui_page_t *page)
{
    (void)page;
    if (!s_fs.data_ready && !s_fs.loading) {
        s_fs.path[0] = '\0';
        file_update_status();
        file_request_list();
    }
    UART_SetAppCallbacks(&s_file_callbacks);
    ui_page_invalidate_all();
}

static void file_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page; (void)dirty;
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);
    file_draw_toolbar();
    file_draw_list();
}

/*=============================================================================
 *  Widget Event Handlers
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_SWIPE_UP) {
        int max_scroll = s_fs.count - FILE_VISIBLE_ITEMS;
        if (max_scroll < 0) max_scroll = 0;
        s_fs.scroll_offset += 3;
        if (s_fs.scroll_offset > max_scroll) s_fs.scroll_offset = max_scroll;
        ui_page_invalidate_all();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_fs.scroll_offset -= 3;
        if (s_fs.scroll_offset < 0) s_fs.scroll_offset = 0;
        ui_page_invalidate_all();
        return;
    }
    if (e->type != UI_EVENT_CLICK) return;
    if (s_fs.count == 0) return;
    int16_t rel_y = e->pos.y - FILE_LIST_Y;
    if (rel_y < 0 || rel_y >= FILE_LIST_H) return;
    int item_idx = s_fs.scroll_offset + rel_y / FILE_ITEM_H;
    if (item_idx < 0 || item_idx >= s_fs.count) return;
    if (item_idx == s_fs.selected) {
        file_open_selected();
    } else {
        s_fs.selected = item_idx;
        ui_page_invalidate_all();
    }
}

static void btn_up_click(ui_widget_t *w) { (void)w; if (!s_fs.loading) file_go_up(); }
static void btn_new_folder_click(ui_widget_t *w) { (void)w; if (!s_fs.loading) file_create_folder(); }
static void btn_delete_click(ui_widget_t *w) { (void)w; if (!s_fs.loading && s_fs.selected >= 0) file_delete_selected(); }
static void btn_refresh_click(ui_widget_t *w) { (void)w; file_request_list(); }

/*=============================================================================
 *  Public API
 *=============================================================================*/

void app_file_init(void)
{
    ui_app_page_init(&s_app_file, "Files", 0x101);
    memset(&s_fs, 0, sizeof(s_fs));
    file_update_status();

    ui_rect_t r_up = {10, TB_BTN_Y, TB_BTN_W, TB_BTN_H};
    ui_button_init(&btn_up, &r_up, "Up", &font_montserrat_12);
    ui_button_set_callback(&btn_up, btn_up_click);
    ui_button_set_colors(&btn_up, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
    btn_up.radius = 8;

    ui_rect_t r_newf = {80, TB_BTN_Y, TB_BTN_W + 20, TB_BTN_H};
    ui_button_init(&btn_new_folder, &r_newf, "+Dir", &font_montserrat_12);
    ui_button_set_callback(&btn_new_folder, btn_new_folder_click);
    ui_button_set_colors(&btn_new_folder, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
    btn_new_folder.radius = 8;

    ui_rect_t r_delf = {180, TB_BTN_Y, TB_BTN_W + 10, TB_BTN_H};
    ui_button_init(&btn_delete, &r_delf, "Del", &font_montserrat_12);
    ui_button_set_callback(&btn_delete, btn_delete_click);
    ui_button_set_colors(&btn_delete, UI_HEX(0xFFEBEE), UI_COLOR_ACCENT, UI_COLOR_DANGER);
    btn_delete.radius = 8;

    ui_rect_t r_ref = {260, TB_BTN_Y, TB_BTN_W, TB_BTN_H};
    ui_button_init(&btn_refresh, &r_ref, "Ref", &font_montserrat_12);
    ui_button_set_callback(&btn_refresh, btn_refresh_click);
    ui_button_set_colors(&btn_refresh, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
    btn_refresh.radius = 8;

    ui_rect_t list_rect = {0, FILE_LIST_Y, UI_SCREEN_WIDTH, FILE_LIST_H};
    ui_widget_init(&s_list_touch, &list_rect);
    s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_list_touch.event_cb = list_touch_event;

    s_file_widgets[0] = (ui_widget_t *)&s_app_file.btn_back;
    s_file_widgets[1] = (ui_widget_t *)&s_app_file.lbl_title;
    s_file_widgets[2] = (ui_widget_t *)&btn_up;
    s_file_widgets[3] = (ui_widget_t *)&btn_new_folder;
    s_file_widgets[4] = (ui_widget_t *)&btn_delete;
    s_file_widgets[5] = (ui_widget_t *)&btn_refresh;
    s_file_widgets[6] = &s_list_touch;

    ui_page_set_widgets(&s_app_file.page, s_file_widgets, 7);
    ui_page_set_callbacks(&s_app_file.page, file_page_enter, NULL, file_page_draw, NULL);
    ui_page_register(&s_app_file.page);
}

ui_page_t *app_file_get_page(void) { return &s_app_file.page; }
