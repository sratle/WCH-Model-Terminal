/********************************** (C) COPYRIGHT *******************************
* File Name          : app_file.c
* Author             : E-ink Model Team (ported from Display-1 V4.0)
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : File Manager app for E-ink (1bpp).
*                      Browse directories, open music/text/image files via CLI.
*                      Context menu (rename, copy, stat, delete) via Menu button.
*                      Breadcrumb path bar, new file/folder creation.
*                      All file I/O routed through UART_SendCLI.
*
*                      E-ink adaptations:
*                        - 1bpp palette: selection = inverted, no gray tones
*                        - Context menu triggered by toolbar "Menu" button
*                          (touchpad has no right-click / long-press)
*                        - Image files open in the Images app
********************************************************************************/
#include "app_file.h"
#include "app_music.h"
#include "app_editor.h"
#include "app_images.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/miniui_page.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include "debug.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (648x480)
 *=============================================================================*/

#define FILE_TOOLBAR_H      40
#define FILE_TOOLBAR_Y      APP_TITLE_BAR_H
#define FILE_BREADCRUMB_H   28
#define FILE_BREADCRUMB_Y   (FILE_TOOLBAR_Y + FILE_TOOLBAR_H)
#define FILE_LIST_Y         (FILE_BREADCRUMB_Y + FILE_BREADCRUMB_H)
#define FILE_LIST_H         (UI_SCREEN_HEIGHT - FILE_LIST_Y)
#define FILE_ITEM_H         36
#define FILE_VISIBLE_ITEMS  (FILE_LIST_H / FILE_ITEM_H)
#define FILE_MAX_ENTRIES    FILE_LIST_MAX_ENTRIES
#define FILE_PATH_MAX       64
#define TB_BTN_W            56
#define TB_BTN_H            28
#define TB_BTN_Y            (FILE_TOOLBAR_Y + 6)

/* Context menu */
#define CTX_MENU_W          160
#define CTX_MENU_ITEM_H     32
#define CTX_MENU_MAX_ITEMS  5

/* Input dialog */
#define INPUT_DLG_W         360
#define INPUT_DLG_H         140

/*=============================================================================
 *  Context Menu State
 *=============================================================================*/

typedef enum {
    CTX_NONE = 0,
    CTX_RENAME,
    CTX_COPY,
    CTX_STAT,
    CTX_DELETE,
} ctx_action_t;

typedef struct {
    bool visible;
    int16_t x, y;
    int16_t selected;
    ctx_action_t actions[CTX_MENU_MAX_ITEMS];
    const char *labels[CTX_MENU_MAX_ITEMS];
    uint8_t count;
    int16_t target_idx;     /* Which file entry the context menu acts on */
} ctx_menu_t;

/*=============================================================================
 *  Input Dialog State
 *=============================================================================*/

typedef enum {
    INPUT_NONE = 0,
    INPUT_NEW_FOLDER,
    INPUT_NEW_FILE,
    INPUT_RENAME,
    INPUT_COPY,
} input_dlg_type_t;

typedef struct {
    bool visible;
    input_dlg_type_t type;
    char buffer[FILE_NAME_MAX_LEN + 1];
    uint8_t buf_len;
    char title[32];
    char hint[32];
    int16_t cursor_pos;     /* Cursor position in buffer */
} input_dlg_t;

/*=============================================================================
 *  File Manager State
 *=============================================================================*/

typedef struct {
    char path[FILE_PATH_MAX];       /* Real path from Core (via ls header / CWD_NOTIFY) */
    file_entry_t entries[FILE_MAX_ENTRIES];
    int16_t count;
    int16_t selected;
    int16_t scroll_offset;
    bool loading;
    bool data_ready;
    uint8_t last_status;
    uint8_t current_device;         /* 0=SD, 1=USB */
    ctx_menu_t ctx;
    input_dlg_t input;
    char stat_text[256];            /* Result of stat command */
    bool stat_visible;
} file_state_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_app_file;
static ui_widget_t s_list_touch;
static ui_button_t btn_up, btn_new_folder, btn_new_file, btn_delete, btn_refresh, btn_device, btn_menu;
static ui_widget_t *s_file_widgets[10];
static file_state_t s_fs;
static char s_status_text[80];
static char s_path_display[FILE_PATH_MAX + 16];

/* Double-click detection */
static uint32_t s_last_click_ms = 0;
#define DOUBLE_CLICK_MS  400

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void file_request_list(void);
static void file_sort_entries(void);
static void file_update_status(void);
static void file_draw_list(void);
static void file_draw_toolbar(void);
static void file_draw_breadcrumb(void);
static void file_draw_context_menu(void);
static void file_draw_input_dialog(void);
static void file_draw_stat_dialog(void);
static void file_enter_path(const char *name);
static void file_go_up(void);
static void file_go_root(void);
static void file_switch_device(void);
static void file_show_context_menu(int16_t x, int16_t y, int16_t item_idx);
static void file_hide_context_menu(void);
static void file_show_input_dialog(input_dlg_type_t type, const char *title, const char *hint);
static void file_hide_input_dialog(void);
static void file_input_dialog_confirm(void);
static void file_ctx_menu_execute(void);

/*=============================================================================
 *  Dirty Region Helpers
 *=============================================================================*/

/* Invalidate the file list area (toolbar + breadcrumb + list) */
static void file_invalidate_content(void)
{
    ui_rect_t r = {0, FILE_TOOLBAR_Y, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - FILE_TOOLBAR_Y};
    ui_page_invalidate(&r);
}

/* Invalidate a single list item row */
static void file_invalidate_item(int16_t idx)
{
    if (idx < s_fs.scroll_offset || idx >= s_fs.scroll_offset + FILE_VISIBLE_ITEMS) return;
    int16_t y = FILE_LIST_Y + (idx - s_fs.scroll_offset) * FILE_ITEM_H;
    ui_rect_t r = {0, y, UI_SCREEN_WIDTH, FILE_ITEM_H};
    ui_page_invalidate(&r);
}

/* Invalidate the breadcrumb bar */
static void file_invalidate_breadcrumb(void)
{
    ui_rect_t r = {0, FILE_BREADCRUMB_Y, UI_SCREEN_WIDTH, FILE_BREADCRUMB_H};
    ui_page_invalidate(&r);
}

/* Invalidate the scroll indicator region */
static void file_invalidate_scrollbar(void)
{
    ui_rect_t r = {UI_SCREEN_WIDTH - 8, FILE_LIST_Y, 8, FILE_LIST_H};
    ui_page_invalidate(&r);
}

/* Invalidate the context menu region */
static void file_invalidate_ctx_menu(void)
{
    ctx_menu_t *ctx = &s_fs.ctx;
    if (!ctx->visible) return;
    int16_t menu_h = ctx->count * CTX_MENU_ITEM_H;
    ui_rect_t r = {ctx->x - 2, ctx->y - 2, CTX_MENU_W + 7, menu_h + 7};
    ui_page_invalidate(&r);
}

/* Invalidate the input dialog region */
static void file_invalidate_input_dialog(void)
{
    ui_rect_t r = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_page_invalidate(&r);
}

/* Invalidate the stat dialog region */
static void file_invalidate_stat_dialog(void)
{
    ui_rect_t r = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_page_invalidate(&r);
}

/* Invalidate old and new selected items (for selection change) */
static void file_invalidate_selection_change(int16_t old_sel, int16_t new_sel)
{
    if (old_sel >= 0) file_invalidate_item(old_sel);
    if (new_sel >= 0) file_invalidate_item(new_sel);
    file_invalidate_scrollbar();
}

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
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    if (strlen(dot) < 2) return false;

    char ext[8];
    int elen = 0;
    dot++;
    while (*dot && elen < (int)sizeof(ext) - 1) {
        ext[elen++] = (*dot >= 'A' && *dot <= 'Z') ? (*dot + 32) : *dot;
        dot++;
    }
    ext[elen] = '\0';

    static const char *text_exts[] = {
        "txt", "json", "xml", "csv", "log", "md",
        "ini", "cfg", "conf", "dat",
        "c", "h", "cpp", "hpp", "py", "lua", "js",
        "html", "htm", "css",
        NULL
    };
    for (int i = 0; text_exts[i] != NULL; i++) {
        if (strcmp(ext, text_exts[i]) == 0) return true;
    }
    return false;
}

static bool file_is_image(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8];
    int elen = 0;
    dot++;
    while (*dot && elen < (int)sizeof(ext) - 1) {
        ext[elen++] = (*dot >= 'A' && *dot <= 'Z') ? (*dot + 32) : *dot;
        dot++;
    }
    ext[elen] = '\0';
    return (strcmp(ext, "bmp") == 0);
}

static const char *file_icon_text(const file_entry_t *e)
{
    if (e->attr & FILE_ATTR_IS_DIR) return "D";
    if (file_is_wav(e->name)) return "~";
    if (file_is_text(e->name)) return "#";
    if (file_is_image(e->name)) return "I";
    return "F";
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

/* Ensure selected item is visible in the scroll viewport */
static void file_ensure_selected_visible(void)
{
    if (s_fs.selected < 0) return;
    if (s_fs.selected < s_fs.scroll_offset)
        s_fs.scroll_offset = s_fs.selected;
    else if (s_fs.selected >= s_fs.scroll_offset + FILE_VISIBLE_ITEMS)
        s_fs.scroll_offset = s_fs.selected - FILE_VISIBLE_ITEMS + 1;
    int max_scroll = s_fs.count - FILE_VISIBLE_ITEMS;
    if (max_scroll < 0) max_scroll = 0;
    if (s_fs.scroll_offset > max_scroll) s_fs.scroll_offset = max_scroll;
    if (s_fs.scroll_offset < 0) s_fs.scroll_offset = 0;
}

/*=============================================================================
 *  Protocol Callbacks
 *=============================================================================*/

static void on_file_list_received(uint8_t status, const file_entry_t *entries, uint8_t count)
{
    printf("[FILE] list: status=%d count=%d\r\n", status, count);
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
    file_invalidate_content();
}

static void on_cwd_changed(const char *path)
{
    strncpy(s_fs.path, path, FILE_PATH_MAX - 1);
    s_fs.path[FILE_PATH_MAX - 1] = '\0';
    file_update_status();
    file_invalidate_breadcrumb();
}

static void file_on_cli_complete(const char *buf, uint16_t len, const char *tag);

static uart_cli_cb_t s_file_cb = {
    .on_file_list = on_file_list_received,
    .on_cli_complete = file_on_cli_complete,
    .on_cwd_notify = on_cwd_changed,
};

static void file_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    printf("[FILE] complete: tag='%s' len=%d\r\n", tag ? tag : "(null)", len);
    if (!tag) return;

    /* Handle stat response */
    if (strcmp(tag, "stat") == 0 && s_fs.stat_visible) {
        uint16_t copy = len;
        if (copy > sizeof(s_fs.stat_text) - 1) copy = sizeof(s_fs.stat_text) - 1;
        memcpy(s_fs.stat_text, buf, copy);
        s_fs.stat_text[copy] = '\0';
        file_invalidate_stat_dialog();
        return;
    }

    /* After file operations (mkdir/rm/copy/rename/device switch), refresh */
    s_fs.loading = true;
    s_fs.data_ready = false;
    UART_RequestFileList(NULL);
    file_invalidate_content();
}

/*=============================================================================
 *  File Operations
 *=============================================================================*/

static void file_request_list(void)
{
    s_fs.loading = true;
    s_fs.data_ready = false;
    UART_SetCLICallbacks(&s_file_cb);
    UART_RequestFileList(NULL);
    file_update_status();
    file_invalidate_content();
}

static void file_enter_path(const char *name)
{
    s_fs.loading = true;
    s_fs.data_ready = false;
    UART_SetCLICallbacks(&s_file_cb);
    UART_RequestFileList(name);
    file_update_status();
    file_invalidate_content();
}

static void file_go_up(void)
{
    s_fs.loading = true;
    s_fs.data_ready = false;
    UART_SetCLICallbacks(&s_file_cb);
    UART_RequestFileList("..");
    file_update_status();
    file_invalidate_content();
}

static void file_go_root(void)
{
    s_fs.loading = true;
    s_fs.data_ready = false;
    UART_SetCLICallbacks(&s_file_cb);
    UART_RequestFileList("\\");
    file_update_status();
    file_invalidate_content();
}

static void file_switch_device(void)
{
    s_fs.current_device = !s_fs.current_device;
    const char *dev_cmd = s_fs.current_device ? "device usb" : "device sd";

    s_fs.loading = true;
    s_fs.data_ready = false;
    s_fs.path[0] = '\0';
    UART_SetCLICallbacks(&s_file_cb);
    UART_SendCLI(dev_cmd);
    file_update_status();
    file_invalidate_content();
}

static void file_delete_selected(void)
{
    if (s_fs.selected < 0 || s_fs.selected >= s_fs.count) return;
    file_entry_t *e = &s_fs.entries[s_fs.selected];
    s_fs.loading = true;
    UART_SetCLICallbacks(&s_file_cb);
    if (e->attr & FILE_ATTR_IS_DIR)
        UART_SendFileOperation(FILE_OP_DELETE_DIR, e->name);
    else
        UART_SendFileOperation(FILE_OP_DELETE_FILE, e->name);
    file_update_status();
    file_invalidate_content();
}

static void file_create_folder(void)
{
    file_show_input_dialog(INPUT_NEW_FOLDER, "New Folder", "Folder name");
}

static void file_create_file(void)
{
    file_show_input_dialog(INPUT_NEW_FILE, "New File", "File name");
}

static void file_open_selected(void)
{
    if (s_fs.selected < 0 || s_fs.selected >= s_fs.count) return;
    file_entry_t *e = &s_fs.entries[s_fs.selected];
    if (e->attr & FILE_ATTR_IS_DIR) { file_enter_path(e->name); return; }

    if (file_is_wav(e->name)) {
        const char *wav_names[FILE_MAX_ENTRIES];
        int16_t wav_count = 0;
        int16_t wav_index = -1;
        for (int16_t i = 0; i < s_fs.count; i++) {
            if (file_is_wav(s_fs.entries[i].name)) {
                wav_names[wav_count] = s_fs.entries[i].name;
                if (i == s_fs.selected) wav_index = wav_count;
                wav_count++;
            }
        }
        app_music_set_playlist(wav_names, wav_count, wav_index);
        UART_SendPlayMusic(e->name);
        ui_page_push(app_music_get_page());
    } else if (file_is_text(e->name)) {
        app_editor_open_file(e->name, e->name);
        ui_page_push(app_editor_get_page());
    } else if (file_is_image(e->name)) {
        app_images_open_file(e->name);
        ui_page_push(app_images_get_page());
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

    const char *dev_name = s_fs.current_device ? "USB" : "SD";
    if (s_fs.path[0] == '\0')
        snprintf(s_path_display, sizeof(s_path_display), "[%s] \\", dev_name);
    else
        snprintf(s_path_display, sizeof(s_path_display), "[%s] %s", dev_name, s_fs.path);

    btn_device.text = s_fs.current_device ? "USB" : "SD";
    btn_device.base.flags |= UI_WIDGET_FLAG_DIRTY;
}

/*=============================================================================
 *  Context Menu
 *=============================================================================*/

static void file_show_context_menu(int16_t x, int16_t y, int16_t item_idx)
{
    if (item_idx < 0 || item_idx >= s_fs.count) return;

    ctx_menu_t *ctx = &s_fs.ctx;
    ctx->visible = true;
    ctx->selected = -1;
    ctx->target_idx = item_idx;
    ctx->count = 0;

    /* Always show stat */
    ctx->actions[ctx->count] = CTX_STAT;
    ctx->labels[ctx->count] = "Properties";
    ctx->count++;

    /* Copy */
    ctx->actions[ctx->count] = CTX_COPY;
    ctx->labels[ctx->count] = "Copy";
    ctx->count++;

    /* Rename (only for files, CH378 mv limitation) */
    file_entry_t *e = &s_fs.entries[item_idx];
    if (!(e->attr & FILE_ATTR_IS_DIR)) {
        ctx->actions[ctx->count] = CTX_RENAME;
        ctx->labels[ctx->count] = "Rename";
        ctx->count++;
    }

    /* Delete */
    ctx->actions[ctx->count] = CTX_DELETE;
    ctx->labels[ctx->count] = "Delete";
    ctx->count++;

    /* Position: ensure menu stays on screen */
    int16_t menu_h = ctx->count * CTX_MENU_ITEM_H;
    ctx->x = x;
    ctx->y = y;
    if (ctx->x + CTX_MENU_W > UI_SCREEN_WIDTH) ctx->x = UI_SCREEN_WIDTH - CTX_MENU_W;
    if (ctx->y + menu_h > UI_SCREEN_HEIGHT) ctx->y = UI_SCREEN_HEIGHT - menu_h;
    if (ctx->x < 0) ctx->x = 0;
    if (ctx->y < FILE_LIST_Y) ctx->y = FILE_LIST_Y;

    file_invalidate_ctx_menu();
}

static void file_hide_context_menu(void)
{
    ctx_menu_t *ctx = &s_fs.ctx;
    if (ctx->visible) {
        int16_t menu_h = ctx->count * CTX_MENU_ITEM_H;
        ui_rect_t r = {ctx->x - 2, ctx->y - 2, CTX_MENU_W + 7, menu_h + 7};
        ui_page_invalidate(&r);
    }
    s_fs.ctx.visible = false;
}

static void file_ctx_menu_execute(void)
{
    ctx_menu_t *ctx = &s_fs.ctx;
    if (ctx->selected < 0 || ctx->selected >= ctx->count) return;
    if (ctx->target_idx < 0 || ctx->target_idx >= s_fs.count) return;

    ctx_action_t action = ctx->actions[ctx->selected];
    file_entry_t *e = &s_fs.entries[ctx->target_idx];

    file_hide_context_menu();

    switch (action) {
    case CTX_STAT: {
        s_fs.stat_visible = true;
        s_fs.stat_text[0] = '\0';
        UART_SetCLICallbacks(&s_file_cb);
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "stat \"%s\"", e->name);
        UART_SendCLI(cmd);
        file_invalidate_stat_dialog();
        break;
    }
    case CTX_COPY:
        file_show_input_dialog(INPUT_COPY, "Copy", "New name");
        break;
    case CTX_RENAME:
        file_show_input_dialog(INPUT_RENAME, "Rename", "New name");
        break;
    case CTX_DELETE:
        file_delete_selected();
        break;
    default:
        break;
    }
}

/*=============================================================================
 *  Input Dialog
 *=============================================================================*/

static void file_show_input_dialog(input_dlg_type_t type, const char *title, const char *hint)
{
    input_dlg_t *dlg = &s_fs.input;
    dlg->visible = true;
    dlg->type = type;
    dlg->buf_len = 0;
    dlg->buffer[0] = '\0';
    dlg->cursor_pos = 0;
    strncpy(dlg->title, title, sizeof(dlg->title) - 1);
    dlg->title[sizeof(dlg->title) - 1] = '\0';
    strncpy(dlg->hint, hint, sizeof(dlg->hint) - 1);
    dlg->hint[sizeof(dlg->hint) - 1] = '\0';

    /* Pre-fill rename/copy with original name */
    if ((type == INPUT_RENAME || type == INPUT_COPY) &&
        s_fs.ctx.target_idx >= 0 && s_fs.ctx.target_idx < s_fs.count) {
        const char *orig = s_fs.entries[s_fs.ctx.target_idx].name;
        uint8_t olen = strlen(orig);
        if (olen > FILE_NAME_MAX_LEN) olen = FILE_NAME_MAX_LEN;
        memcpy(dlg->buffer, orig, olen);
        dlg->buffer[olen] = '\0';
        dlg->buf_len = olen;
        dlg->cursor_pos = olen;
    }

    file_invalidate_input_dialog();
}

static void file_hide_input_dialog(void)
{
    s_fs.input.visible = false;
    file_invalidate_input_dialog();
}

static void file_input_dialog_confirm(void)
{
    input_dlg_t *dlg = &s_fs.input;
    if (dlg->buf_len == 0) {
        file_hide_input_dialog();
        return;
    }

    UART_SetCLICallbacks(&s_file_cb);
    char cmd[256];

    switch (dlg->type) {
    case INPUT_NEW_FOLDER:
        snprintf(cmd, sizeof(cmd), "mkdir \"%s\"", dlg->buffer);
        UART_SendCLI(cmd);
        break;
    case INPUT_NEW_FILE:
        snprintf(cmd, sizeof(cmd), "touch \"%s\"", dlg->buffer);
        UART_SendCLI(cmd);
        break;
    case INPUT_RENAME:
        if (s_fs.ctx.target_idx >= 0 && s_fs.ctx.target_idx < s_fs.count) {
            const char *old_name = s_fs.entries[s_fs.ctx.target_idx].name;
            snprintf(cmd, sizeof(cmd), "mv \"%s\" \"%s\"", old_name, dlg->buffer);
            UART_SendCLI(cmd);
        }
        break;
    case INPUT_COPY:
        if (s_fs.ctx.target_idx >= 0 && s_fs.ctx.target_idx < s_fs.count) {
            const char *src_name = s_fs.entries[s_fs.ctx.target_idx].name;
            snprintf(cmd, sizeof(cmd), "cp \"%s\" \"%s\"", src_name, dlg->buffer);
            UART_SendCLI(cmd);
        }
        break;
    default:
        break;
    }

    file_hide_input_dialog();
}

/*=============================================================================
 *  Drawing (1bpp)
 *=============================================================================*/

static void file_draw_toolbar(void)
{
    ui_rect_t tb_bg = {0, FILE_TOOLBAR_Y, UI_SCREEN_WIDTH, FILE_TOOLBAR_H};
    ui_draw_fill_rect(&tb_bg, UI_COLOR_WHITE);
    ui_draw_hline(0, FILE_TOOLBAR_Y + FILE_TOOLBAR_H - 1, UI_SCREEN_WIDTH, UI_COLOR_BLACK);
}

static void file_draw_breadcrumb(void)
{
    ui_rect_t bg = {0, FILE_BREADCRUMB_Y, UI_SCREEN_WIDTH, FILE_BREADCRUMB_H};
    ui_draw_fill_rect(&bg, UI_COLOR_WHITE);
    ui_draw_hline(0, FILE_BREADCRUMB_Y + FILE_BREADCRUMB_H - 1, UI_SCREEN_WIDTH, UI_COLOR_BLACK);

    int16_t x = 12;
    int16_t y = FILE_BREADCRUMB_Y + 6;

    /* Device indicator */
    const char *dev = s_fs.current_device ? "USB" : "SD";
    ui_draw_text(x, y, dev, &font_montserrat_12, UI_COLOR_BLACK);
    x += ui_text_width(dev, &font_montserrat_12) + 4;
    ui_draw_text(x, y, "|", &font_montserrat_12, UI_COLOR_BLACK);
    x += ui_text_width("|", &font_montserrat_12) + 6;

    /* Root */
    ui_draw_text(x, y, "\\", &font_montserrat_12, UI_COLOR_BLACK);
    x += ui_text_width("\\", &font_montserrat_12) + 4;

    /* Path segments */
    if (s_fs.path[0] != '\0') {
        char path_copy[FILE_PATH_MAX];
        strncpy(path_copy, s_fs.path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';

        char *seg = path_copy;
        if (seg[0] == '\\') seg++;

        char *token = strtok(seg, "\\");
        while (token != NULL && x < UI_SCREEN_WIDTH - 20) {
            ui_draw_text(x, y, ">", &font_montserrat_12, UI_COLOR_BLACK);
            x += ui_text_width(">", &font_montserrat_12) + 4;
            ui_draw_text(x, y, token, &font_montserrat_12, UI_COLOR_BLACK);
            x += ui_text_width(token, &font_montserrat_12) + 4;
            token = strtok(NULL, "\\");
        }
    }

    /* Status on the right */
    int16_t sw = ui_text_width(s_status_text, &font_montserrat_12);
    ui_draw_text(UI_SCREEN_WIDTH - sw - 12, y, s_status_text,
                 &font_montserrat_12, UI_COLOR_BLACK);
}

static void file_draw_list(void)
{
    ui_rect_t list_bg = {0, FILE_LIST_Y, UI_SCREEN_WIDTH, FILE_LIST_H};
    ui_draw_fill_rect(&list_bg, UI_COLOR_WHITE);

    if (s_fs.loading) {
        ui_draw_text_in_rect(&list_bg, "Loading...", &font_montserrat_16,
                             UI_COLOR_BLACK, 1);
        return;
    }
    if (s_fs.count == 0) {
        ui_draw_text_in_rect(&list_bg, "Empty directory", &font_montserrat_16,
                             UI_COLOR_BLACK, 1);
        return;
    }

    for (int i = 0; i < FILE_VISIBLE_ITEMS && (i + s_fs.scroll_offset) < s_fs.count; i++) {
        int idx = i + s_fs.scroll_offset;
        file_entry_t *e = &s_fs.entries[idx];
        int16_t y = FILE_LIST_Y + i * FILE_ITEM_H;
        bool sel = (idx == s_fs.selected);
        ui_rect_t item_rect = {0, y, UI_SCREEN_WIDTH, FILE_ITEM_H};

        /* Selection = inverted row */
        if (sel) ui_draw_fill_rect(&item_rect, UI_COLOR_BLACK);
        ui_color_t fg = sel ? UI_COLOR_WHITE : UI_COLOR_BLACK;

        ui_draw_hline(12, y + FILE_ITEM_H - 1, UI_SCREEN_WIDTH - 24, UI_COLOR_BLACK);

        bool is_dir = (e->attr & FILE_ATTR_IS_DIR) != 0;
        const char *icon = file_icon_text(e);

        /* Icon badge: outline box with letter */
        ui_rect_t badge = {16, y + 8, 20, 20};
        if (sel) {
            ui_draw_round_rect_border(&badge, 4, UI_COLOR_WHITE, 1);
        } else {
            ui_draw_round_rect(&badge, 4, UI_COLOR_WHITE, UI_COLOR_BLACK, 1);
        }
        ui_draw_text(22, y + 10, icon, &font_montserrat_12, fg);

        /* File name */
        ui_draw_text(44, y + 4, e->name, &font_montserrat_16, fg);

        /* Size or type label */
        if (is_dir) {
            ui_draw_text(44, y + 22, "Folder", &font_montserrat_12, fg);
        } else if (e->size > 0) {
            char size_buf[16];
            file_format_size(e->size, size_buf, sizeof(size_buf));
            ui_draw_text(44, y + 22, size_buf, &font_montserrat_12, fg);
        }
    }

    /* Scroll indicator */
    if (s_fs.count > FILE_VISIBLE_ITEMS) {
        int16_t bar_h = FILE_LIST_H * FILE_VISIBLE_ITEMS / s_fs.count;
        if (bar_h < 20) bar_h = 20;
        int max_scroll = s_fs.count - FILE_VISIBLE_ITEMS;
        int16_t bar_y = FILE_LIST_Y;
        if (max_scroll > 0)
            bar_y += (FILE_LIST_H - bar_h) * s_fs.scroll_offset / max_scroll;
        ui_rect_t scroll_bar = {UI_SCREEN_WIDTH - 6, bar_y, 5, bar_h};
        ui_draw_fill_rect(&scroll_bar, UI_COLOR_BLACK);
    }
}

static void file_draw_context_menu(void)
{
    ctx_menu_t *ctx = &s_fs.ctx;
    if (!ctx->visible) return;

    int16_t menu_h = ctx->count * CTX_MENU_ITEM_H;

    /* Background */
    ui_rect_t bg = {ctx->x, ctx->y, CTX_MENU_W, menu_h};
    ui_draw_fill_rect(&bg, UI_COLOR_WHITE);
    ui_draw_rect_border(&bg, UI_COLOR_BLACK, 1);

    /* Items */
    for (int i = 0; i < ctx->count; i++) {
        int16_t iy = ctx->y + i * CTX_MENU_ITEM_H;
        ui_rect_t item_r = {ctx->x + 1, iy, CTX_MENU_W - 2, CTX_MENU_ITEM_H};

        bool sel = (i == ctx->selected);
        if (sel) ui_draw_fill_rect(&item_r, UI_COLOR_BLACK);

        ui_draw_text(ctx->x + 16, iy + 8, ctx->labels[i],
                     &font_montserrat_16, sel ? UI_COLOR_WHITE : UI_COLOR_BLACK);

        if (i < ctx->count - 1)
            ui_draw_hline(ctx->x + 8, iy + CTX_MENU_ITEM_H - 1,
                          CTX_MENU_W - 16, UI_COLOR_BLACK);
    }
}

static void file_draw_input_dialog(void)
{
    input_dlg_t *dlg = &s_fs.input;
    if (!dlg->visible) return;

    /* Light overlay (dotted effect via border; keep white for e-ink) */
    ui_rect_t screen = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&screen, UI_COLOR_WHITE);

    /* Dialog box centered */
    int16_t dx = (UI_SCREEN_WIDTH - INPUT_DLG_W) / 2;
    int16_t dy = (UI_SCREEN_HEIGHT - INPUT_DLG_H) / 2;
    ui_rect_t dlg_bg = {dx, dy, INPUT_DLG_W, INPUT_DLG_H};
    ui_draw_fill_round_rect(&dlg_bg, 8, UI_COLOR_WHITE);
    ui_draw_round_rect_border(&dlg_bg, 8, UI_COLOR_BLACK, 2);

    /* Title */
    ui_draw_text(dx + 20, dy + 14, dlg->title, &font_montserrat_16, UI_COLOR_BLACK);

    /* Input field */
    ui_rect_t field = {dx + 20, dy + 46, INPUT_DLG_W - 40, 32};
    ui_draw_fill_rect(&field, UI_COLOR_WHITE);
    ui_draw_rect_border(&field, UI_COLOR_BLACK, 1);

    /* Text in field */
    if (dlg->buf_len > 0) {
        ui_draw_text(field.x + 8, field.y + 8, dlg->buffer,
                     &font_montserrat_16, UI_COLOR_BLACK);
    } else {
        ui_draw_text(field.x + 8, field.y + 10, dlg->hint,
                     &font_montserrat_12, UI_COLOR_BLACK);
    }

    /* Cursor */
    {
        int16_t cx = field.x + 8;
        if (dlg->buf_len > 0)
            cx += ui_text_width(dlg->buffer, &font_montserrat_16);
        ui_draw_vline(cx, field.y + 4, field.h - 8, UI_COLOR_BLACK);
    }

    /* Buttons: Cancel | OK */
    int16_t btn_y = dy + INPUT_DLG_H - 44;
    ui_rect_t cancel_r = {dx + INPUT_DLG_W - 180, btn_y, 72, 30};
    ui_draw_round_rect(&cancel_r, 6, UI_COLOR_WHITE, UI_COLOR_BLACK, 1);
    ui_draw_text(cancel_r.x + 12, cancel_r.y + 6, "Cancel",
                 &font_montserrat_12, UI_COLOR_BLACK);

    ui_rect_t ok_r = {dx + INPUT_DLG_W - 96, btn_y, 72, 30};
    ui_draw_fill_round_rect(&ok_r, 6, UI_COLOR_BLACK);
    ui_draw_text(ok_r.x + 22, ok_r.y + 6, "OK",
                 &font_montserrat_12, UI_COLOR_WHITE);
}

static void file_draw_stat_dialog(void)
{
    if (!s_fs.stat_visible) return;

    ui_rect_t screen = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&screen, UI_COLOR_WHITE);

    int16_t dw = 400, dh = 280;
    int16_t dx = (UI_SCREEN_WIDTH - dw) / 2;
    int16_t dy = (UI_SCREEN_HEIGHT - dh) / 2;
    ui_rect_t dlg_bg = {dx, dy, dw, dh};
    ui_draw_fill_round_rect(&dlg_bg, 8, UI_COLOR_WHITE);
    ui_draw_round_rect_border(&dlg_bg, 8, UI_COLOR_BLACK, 2);

    /* Title */
    ui_draw_text(dx + 20, dy + 14, "Properties", &font_montserrat_16, UI_COLOR_BLACK);
    ui_draw_hline(dx + 16, dy + 38, dw - 32, UI_COLOR_BLACK);

    /* Stat text (multi-line) */
    if (s_fs.stat_text[0] != '\0') {
        const char *p = s_fs.stat_text;
        int16_t ly = dy + 48;
        char line_buf[64];
        while (*p && ly < dy + dh - 50) {
            int li = 0;
            while (*p && *p != '\n' && *p != '\r' && li < (int)sizeof(line_buf) - 1) {
                line_buf[li++] = *p++;
            }
            line_buf[li] = '\0';
            if (*p == '\r') p++;
            if (*p == '\n') p++;
            if (li > 0) {
                ui_draw_text(dx + 20, ly, line_buf, &font_montserrat_12, UI_COLOR_BLACK);
                ly += 16;
            }
        }
    } else {
        ui_draw_text(dx + 20, dy + 48, "Loading...", &font_montserrat_12, UI_COLOR_BLACK);
    }

    /* Close button */
    ui_rect_t close_r = {dx + dw - 96, dy + dh - 44, 72, 30};
    ui_draw_fill_round_rect(&close_r, 6, UI_COLOR_BLACK);
    ui_draw_text(close_r.x + 14, close_r.y + 6, "Close",
                 &font_montserrat_12, UI_COLOR_WHITE);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void file_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetCLICallbacks(&s_file_cb);
    if (!s_fs.data_ready && !s_fs.loading) {
        printf("[FILE] enter, requesting root\r\n");
        UART_RequestFileList("\\");
    }
    ui_page_invalidate_all();
}

static void file_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page; (void)dirty;
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);
    file_draw_toolbar();
    file_draw_breadcrumb();
    file_draw_list();
    file_draw_context_menu();
    file_draw_input_dialog();
    file_draw_stat_dialog();
}

/*=============================================================================
 *  Widget Event Handlers
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    /* If input dialog is open, handle dialog events */
    if (s_fs.input.visible) {
        if (e->type == UI_EVENT_CLICK) {
            int16_t dx = (UI_SCREEN_WIDTH - INPUT_DLG_W) / 2;
            int16_t dy = (UI_SCREEN_HEIGHT - INPUT_DLG_H) / 2;
            int16_t btn_y = dy + INPUT_DLG_H - 44;

            /* Cancel button */
            ui_rect_t cancel_r = {dx + INPUT_DLG_W - 180, btn_y, 72, 30};
            if (ui_widget_hit_test((ui_widget_t *)&cancel_r, e->touch.x, e->touch.y)) {
                file_hide_input_dialog();
                return;
            }
            /* OK button */
            ui_rect_t ok_r = {dx + INPUT_DLG_W - 96, btn_y, 72, 30};
            if (ui_widget_hit_test((ui_widget_t *)&ok_r, e->touch.x, e->touch.y)) {
                file_input_dialog_confirm();
                return;
            }
        }
        return;
    }

    /* If stat dialog is open, handle close */
    if (s_fs.stat_visible) {
        if (e->type == UI_EVENT_CLICK) {
            int16_t dw = 400, dh = 280;
            int16_t dx = (UI_SCREEN_WIDTH - dw) / 2;
            int16_t dy = (UI_SCREEN_HEIGHT - dh) / 2;
            ui_rect_t close_r = {dx + dw - 96, dy + dh - 44, 72, 30};
            if (ui_widget_hit_test((ui_widget_t *)&close_r, e->touch.x, e->touch.y)) {
                s_fs.stat_visible = false;
                file_invalidate_stat_dialog();
                return;
            }
        }
        return;
    }

    /* If context menu is open */
    if (s_fs.ctx.visible) {
        if (e->type == UI_EVENT_CLICK) {
            ctx_menu_t *ctx = &s_fs.ctx;
            int16_t rel_x = e->touch.x - ctx->x;
            int16_t rel_y = e->touch.y - ctx->y;
            if (rel_x >= 0 && rel_x < CTX_MENU_W && rel_y >= 0 &&
                rel_y < ctx->count * CTX_MENU_ITEM_H) {
                ctx->selected = rel_y / CTX_MENU_ITEM_H;
                file_ctx_menu_execute();
            } else {
                file_hide_context_menu();
            }
            return;
        }
        return;
    }

    /* Touch swipe scrolling */
    if (e->type == UI_EVENT_SWIPE_UP) {
        int max_scroll = s_fs.count - FILE_VISIBLE_ITEMS;
        if (max_scroll < 0) max_scroll = 0;
        s_fs.scroll_offset += 3;
        if (s_fs.scroll_offset > max_scroll) s_fs.scroll_offset = max_scroll;
        file_invalidate_content();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_fs.scroll_offset -= 3;
        if (s_fs.scroll_offset < 0) s_fs.scroll_offset = 0;
        file_invalidate_content();
        return;
    }

    /* Mouse wheel scrolling */
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        int max_scroll = s_fs.count - FILE_VISIBLE_ITEMS;
        if (max_scroll < 0) max_scroll = 0;
        s_fs.scroll_offset -= e->scroll_delta;
        if (s_fs.scroll_offset > max_scroll) s_fs.scroll_offset = max_scroll;
        if (s_fs.scroll_offset < 0) s_fs.scroll_offset = 0;
        file_invalidate_content();
        return;
    }

    /* Click / Double-click handling */
    if (e->type != UI_EVENT_CLICK && e->type != UI_EVENT_DOUBLE_CLICK) return;
    if (s_fs.count == 0) return;
    int16_t rel_y = e->touch.y - FILE_LIST_Y;
    if (rel_y < 0 || rel_y >= FILE_LIST_H) return;
    int item_idx = s_fs.scroll_offset + rel_y / FILE_ITEM_H;
    if (item_idx < 0 || item_idx >= s_fs.count) return;

    /* Right-click: context menu (external mouse, same as Display-1) */
    if (e->type == UI_EVENT_CLICK && (e->mouse_buttons & UI_MOUSE_BTN_RIGHT)) {
        s_fs.selected = item_idx;
        file_show_context_menu(e->touch.x, e->touch.y, item_idx);
        ui_input_consume_rightclick();   /* claim: don't also trigger global Back */
        return;
    }

    if (e->type == UI_EVENT_DOUBLE_CLICK) {
        s_fs.selected = item_idx;
        file_open_selected();
        return;
    }

    /* Single click: select or double-click detection */
    uint32_t now = ui_get_real_ms();
    if (item_idx == s_fs.selected && (now - s_last_click_ms) < DOUBLE_CLICK_MS) {
        file_open_selected();
        s_last_click_ms = 0;
        return;
    }

    int16_t old_sel = s_fs.selected;
    s_fs.selected = item_idx;
    s_last_click_ms = now;
    if (old_sel >= 0 && old_sel != item_idx) file_invalidate_item(old_sel);
    file_invalidate_item(item_idx);
}

/* Page-level event handler — keyboard navigation + dialog input */
static bool file_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    /* Only handle keyboard and core-key events */
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    /* --- Input dialog is open: consume ALL key events --- */
    if (s_fs.input.visible) {
        input_dlg_t *dlg = &s_fs.input;

        /* ESC/Back: cancel dialog */
        if (e->type == UI_EVENT_KEY_DOWN && e->key.code == UI_KEY_BACK) {
            file_hide_input_dialog();
            return true;
        }
        /* Enter: confirm dialog */
        if (e->type == UI_EVENT_KEY_DOWN && e->key.code == UI_KEY_OK) {
            file_input_dialog_confirm();
            return true;
        }
        /* Character input via char_code */
        if ((e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) &&
            e->char_code >= 0x20 && e->char_code <= 0x7E &&
            dlg->buf_len < FILE_NAME_MAX_LEN) {
            memmove(&dlg->buffer[dlg->cursor_pos + 1],
                    &dlg->buffer[dlg->cursor_pos],
                    dlg->buf_len - dlg->cursor_pos + 1);
            dlg->buffer[dlg->cursor_pos] = (char)e->char_code;
            dlg->cursor_pos++;
            dlg->buf_len++;
            file_invalidate_input_dialog();
            return true;
        }
        /* Backspace via char_code */
        if ((e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) &&
            e->char_code == 0x08) {
            if (dlg->cursor_pos > 0) {
                memmove(&dlg->buffer[dlg->cursor_pos - 1],
                        &dlg->buffer[dlg->cursor_pos],
                        dlg->buf_len - dlg->cursor_pos + 1);
                dlg->cursor_pos--;
                dlg->buf_len--;
            }
            file_invalidate_input_dialog();
            return true;
        }
        return true;
    }

    /* --- Stat dialog is open: any key closes it --- */
    if (s_fs.stat_visible) {
        if (e->type == UI_EVENT_KEY_CLICK || e->type == UI_EVENT_KEY_DOWN) {
            s_fs.stat_visible = false;
            file_invalidate_stat_dialog();
        }
        return true;
    }

    /* --- Context menu is open --- */
    if (s_fs.ctx.visible) {
        ctx_menu_t *ctx = &s_fs.ctx;
        if (e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) {
            switch (e->key.code) {
            case UI_KEY_UP:
                if (ctx->selected < 0) ctx->selected = ctx->count - 1;
                else if (ctx->selected > 0) ctx->selected--;
                file_invalidate_ctx_menu();
                break;
            case UI_KEY_DOWN:
                if (ctx->selected < 0) ctx->selected = 0;
                else if (ctx->selected < ctx->count - 1) ctx->selected++;
                file_invalidate_ctx_menu();
                break;
            default:
                break;
            }
        }
        if (e->type == UI_EVENT_KEY_CLICK) {
            if (e->key.code == UI_KEY_OK) {
                file_ctx_menu_execute();
            } else if (e->key.code == UI_KEY_BACK) {
                file_hide_context_menu();
            }
        }
        return true;
    }

    /* --- Normal list navigation --- */
    if (e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) {
        switch (e->key.code) {
        case UI_KEY_UP:
            if (s_fs.count > 0) {
                int16_t old_sel = s_fs.selected;
                if (s_fs.selected < 0) s_fs.selected = 0;
                else if (s_fs.selected > 0) s_fs.selected--;
                file_ensure_selected_visible();
                file_invalidate_selection_change(old_sel, s_fs.selected);
            }
            return true;
        case UI_KEY_DOWN:
            if (s_fs.count > 0) {
                int16_t old_sel = s_fs.selected;
                if (s_fs.selected < 0) s_fs.selected = 0;
                else if (s_fs.selected < s_fs.count - 1) s_fs.selected++;
                file_ensure_selected_visible();
                file_invalidate_selection_change(old_sel, s_fs.selected);
            }
            return true;
        default:
            break;
        }
    }

    if (e->type == UI_EVENT_KEY_CLICK) {
        switch (e->key.code) {
        case UI_KEY_OK:
            if (s_fs.selected >= 0) file_open_selected();
            return true;
        case UI_KEY_BACK:
            file_go_up();
            return true;
        case UI_KEY_HOME:
            file_go_root();
            return true;
        default:
            /* Menu key: open context menu */
            if (e->key.code == UI_KEY_MENU && s_fs.selected >= 0) {
                int16_t item_y = FILE_LIST_Y + (s_fs.selected - s_fs.scroll_offset) * FILE_ITEM_H;
                file_show_context_menu(200, item_y, s_fs.selected);
                return true;
            }
            break;
        }
    }

    return false;
}

static void btn_up_click(ui_widget_t *w) { (void)w; file_go_up(); }
static void btn_new_folder_click(ui_widget_t *w) { (void)w; if (!s_fs.loading) file_create_folder(); }
static void btn_new_file_click(ui_widget_t *w) { (void)w; if (!s_fs.loading) file_create_file(); }
static void btn_delete_click(ui_widget_t *w) { (void)w; if (!s_fs.loading && s_fs.selected >= 0) file_delete_selected(); }
static void btn_refresh_click(ui_widget_t *w) { (void)w; file_request_list(); }
static void btn_device_click(ui_widget_t *w) { (void)w; file_switch_device(); }
static void btn_menu_click(ui_widget_t *w)
{
    (void)w;
    if (s_fs.selected < 0 || s_fs.selected >= s_fs.count) return;
    int16_t item_y = FILE_LIST_Y + (s_fs.selected - s_fs.scroll_offset) * FILE_ITEM_H;
    file_show_context_menu(200, item_y, s_fs.selected);
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void app_file_init(void)
{
    ui_app_page_init(&s_app_file, "Files", 0x101);
    memset(&s_fs, 0, sizeof(s_fs));
    s_fs.current_device = 1;  /* Default to USB */
    file_update_status();

    ui_rect_t r_up = {10, TB_BTN_Y, TB_BTN_W, TB_BTN_H};
    ui_button_init(&btn_up, &r_up, "Up", &font_montserrat_12);
    ui_button_set_callback(&btn_up, btn_up_click);
    ui_button_set_colors(&btn_up, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_up.radius = 8;

    ui_rect_t r_dev = {74, TB_BTN_Y, TB_BTN_W, TB_BTN_H};
    ui_button_init(&btn_device, &r_dev, "USB", &font_montserrat_12);
    ui_button_set_callback(&btn_device, btn_device_click);
    ui_button_set_colors(&btn_device, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_device.radius = 8;

    ui_rect_t r_newf = {138, TB_BTN_Y, TB_BTN_W + 8, TB_BTN_H};
    ui_button_init(&btn_new_folder, &r_newf, "+Dir", &font_montserrat_12);
    ui_button_set_callback(&btn_new_folder, btn_new_folder_click);
    ui_button_set_colors(&btn_new_folder, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_new_folder.radius = 8;

    ui_rect_t r_newfile = {210, TB_BTN_Y, TB_BTN_W + 12, TB_BTN_H};
    ui_button_init(&btn_new_file, &r_newfile, "+File", &font_montserrat_12);
    ui_button_set_callback(&btn_new_file, btn_new_file_click);
    ui_button_set_colors(&btn_new_file, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_new_file.radius = 8;

    ui_rect_t r_delf = {288, TB_BTN_Y, TB_BTN_W, TB_BTN_H};
    ui_button_init(&btn_delete, &r_delf, "Del", &font_montserrat_12);
    ui_button_set_callback(&btn_delete, btn_delete_click);
    ui_button_set_colors(&btn_delete, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_delete.radius = 8;

    ui_rect_t r_menu = {352, TB_BTN_Y, TB_BTN_W + 6, TB_BTN_H};
    ui_button_init(&btn_menu, &r_menu, "Menu", &font_montserrat_12);
    ui_button_set_callback(&btn_menu, btn_menu_click);
    ui_button_set_colors(&btn_menu, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_menu.radius = 8;

    ui_rect_t r_ref = {424, TB_BTN_Y, TB_BTN_W, TB_BTN_H};
    ui_button_init(&btn_refresh, &r_ref, "Ref", &font_montserrat_12);
    ui_button_set_callback(&btn_refresh, btn_refresh_click);
    ui_button_set_colors(&btn_refresh, UI_COLOR_WHITE, UI_COLOR_BLACK, UI_COLOR_BLACK);
    btn_refresh.radius = 8;

    ui_rect_t touch_rect = {0, FILE_LIST_Y, UI_SCREEN_WIDTH, FILE_LIST_H};
    ui_widget_init(&s_list_touch, &touch_rect);
    s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_list_touch.event_cb = list_touch_event;

    s_file_widgets[0] = (ui_widget_t *)&s_app_file.btn_back;
    s_file_widgets[1] = (ui_widget_t *)&s_app_file.lbl_title;
    s_file_widgets[2] = (ui_widget_t *)&btn_up;
    s_file_widgets[3] = (ui_widget_t *)&btn_device;
    s_file_widgets[4] = (ui_widget_t *)&btn_new_folder;
    s_file_widgets[5] = (ui_widget_t *)&btn_new_file;
    s_file_widgets[6] = (ui_widget_t *)&btn_delete;
    s_file_widgets[7] = (ui_widget_t *)&btn_menu;
    s_file_widgets[8] = (ui_widget_t *)&btn_refresh;
    s_file_widgets[9] = &s_list_touch;

    ui_page_set_widgets(&s_app_file.page, s_file_widgets, 10);
    ui_page_set_callbacks(&s_app_file.page, file_page_enter, NULL, file_page_draw, NULL);
    ui_page_set_event_cb(&s_app_file.page, file_page_event);
    ui_page_register(&s_app_file.page);
}

ui_page_t *app_file_get_page(void) { return &s_app_file.page; }
