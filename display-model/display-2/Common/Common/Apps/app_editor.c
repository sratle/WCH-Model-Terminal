/********************************** (C) COPYRIGHT *******************************
* File Name          : app_editor.c
* Author             : E-ink Model Team (ported from Display-1 V4.0)
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : Text Editor app for E-ink (1bpp).
*                      Read/write files via CLI passthrough ("cat" / "write").
*                      Full keyboard editing: insert, delete, cursor movement.
*                      Tap to position cursor, swipe/scroll to scroll.
*                      Line numbers, status bar, cursor blink (real dt).
*
*                      E-ink adaptations:
*                        - 1bpp palette (no gray tones / selection = inverted)
*                        - 4 KB edit buffer, 32 undo records (RAM discipline)
*                        - blink/scroll driven by real TIM2 milliseconds
********************************************************************************/
#include "app_editor.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (648x480)
 *=============================================================================*/

#define EDIT_STATUS_H       28
#define EDIT_STATUS_Y       (UI_SCREEN_HEIGHT - EDIT_STATUS_H)
#define EDIT_GUTTER_W       44
#define EDIT_TEXT_X         EDIT_GUTTER_W
#define EDIT_TEXT_Y         APP_TITLE_BAR_H
#define EDIT_TEXT_W         (UI_SCREEN_WIDTH - EDIT_GUTTER_W)
#define EDIT_TEXT_H         (EDIT_STATUS_Y - APP_TITLE_BAR_H)
#define EDIT_LINE_H         18
#define EDIT_VISIBLE_LINES  (EDIT_TEXT_H / EDIT_LINE_H)
#define EDIT_BUF_SIZE       4096
#define EDIT_MAX_LINES      256
#define EDIT_MAX_UNDO       32
#define EDIT_TAB_SIZE       4
#define EDIT_MAX_LINE_LEN   128

/* Cursor blink timing */
#define CURSOR_BLINK_ON_MS  530
#define CURSOR_BLINK_OFF_MS 470

/*=============================================================================
 *  Undo Record
 *=============================================================================*/

typedef enum {
    UNDO_INSERT,    /* Inserted chars at pos */
    UNDO_DELETE,    /* Deleted chars at pos */
} undo_type_t;

typedef struct {
    undo_type_t type;
    uint16_t pos;           /* Position in content buffer */
    uint16_t len;           /* Number of chars affected */
    char data[32];          /* Small inline buffer for deleted text */
} undo_record_t;

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

    /* Cursor position (character offset in content) */
    uint16_t cursor_pos;

    /* Cursor display coordinates (derived from cursor_pos) */
    int16_t cursor_line;    /* Line index (0-based) */
    int16_t cursor_col;     /* Column (character offset within line) */

    /* Selection (selection_start != selection_end means active selection) */
    uint16_t sel_start;
    uint16_t sel_end;
    bool has_selection;

    /* Modification tracking */
    bool is_modified;
    bool is_loading;
    bool has_content;
    bool save_sent;

    /* Cursor blink */
    uint32_t cursor_blink_ms;
    bool cursor_visible;

    /* Undo stack */
    undo_record_t undo_stack[EDIT_MAX_UNDO];
    int16_t undo_top;
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
 *  Dirty Region Helpers
 *=============================================================================*/

static void editor_invalidate_text_area(void)
{
    ui_rect_t r = {0, EDIT_TEXT_Y, UI_SCREEN_WIDTH, EDIT_TEXT_H};
    ui_page_invalidate(&r);
}

static void editor_invalidate_status(void)
{
    ui_rect_t r = {0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_STATUS_H};
    ui_page_invalidate(&r);
}

static void editor_invalidate_line(int16_t line_idx)
{
    if (line_idx < s_ed.scroll_line || line_idx >= s_ed.scroll_line + EDIT_VISIBLE_LINES) return;
    int16_t y = EDIT_TEXT_Y + (line_idx - s_ed.scroll_line) * EDIT_LINE_H;
    ui_rect_t r = {0, y, UI_SCREEN_WIDTH, EDIT_LINE_H};
    ui_page_invalidate(&r);
}

static void editor_invalidate_cursor(void)
{
    if (s_ed.cursor_line < s_ed.scroll_line ||
        s_ed.cursor_line >= s_ed.scroll_line + EDIT_VISIBLE_LINES) return;
    int16_t y = EDIT_TEXT_Y + (s_ed.cursor_line - s_ed.scroll_line) * EDIT_LINE_H;
    ui_rect_t r = {EDIT_GUTTER_W, y, UI_SCREEN_WIDTH - EDIT_GUTTER_W, EDIT_LINE_H};
    ui_page_invalidate(&r);
}

static void editor_invalidate_content(void)
{
    editor_invalidate_text_area();
    editor_invalidate_status();
}

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

static void editor_update_cursor_coords(void)
{
    int16_t lo = 0, hi = s_ed.line_count - 1;
    s_ed.cursor_line = 0;
    while (lo <= hi) {
        int16_t mid = (lo + hi) / 2;
        uint16_t line_start = s_ed.line_offsets[mid];
        uint16_t line_end = (mid + 1 < s_ed.line_count) ?
                            s_ed.line_offsets[mid + 1] : s_ed.content_len;
        if (s_ed.cursor_pos >= line_start && s_ed.cursor_pos <= line_end) {
            s_ed.cursor_line = mid;
            break;
        }
        if (s_ed.cursor_pos < line_start)
            hi = mid - 1;
        else
            lo = mid + 1;
    }

    /* Cursor exactly at a line start belongs to that next line */
    if (s_ed.cursor_line + 1 < s_ed.line_count &&
        s_ed.cursor_pos == s_ed.line_offsets[s_ed.cursor_line + 1]) {
        s_ed.cursor_line++;
    }

    uint16_t line_start = s_ed.line_offsets[s_ed.cursor_line];
    s_ed.cursor_col = s_ed.cursor_pos - line_start;
}

static void editor_ensure_cursor_visible(void)
{
    if (s_ed.cursor_line < s_ed.scroll_line)
        s_ed.scroll_line = s_ed.cursor_line;
    else if (s_ed.cursor_line >= s_ed.scroll_line + EDIT_VISIBLE_LINES)
        s_ed.scroll_line = s_ed.cursor_line - EDIT_VISIBLE_LINES + 1;
    int max_scroll = s_ed.line_count - EDIT_VISIBLE_LINES;
    if (max_scroll < 0) max_scroll = 0;
    if (s_ed.scroll_line > max_scroll) s_ed.scroll_line = max_scroll;
    if (s_ed.scroll_line < 0) s_ed.scroll_line = 0;
}

static void editor_update_status(void)
{
    if (s_ed.is_loading)
        snprintf(s_status_buf, sizeof(s_status_buf), "Loading...");
    else if (!s_ed.has_content)
        snprintf(s_status_buf, sizeof(s_status_buf), "No file loaded");
    else if (s_ed.save_sent)
        snprintf(s_status_buf, sizeof(s_status_buf), "Saving...");
    else
        snprintf(s_status_buf, sizeof(s_status_buf), "Ln %d, Col %d | %d lines | %d chars%s",
                 s_ed.cursor_line + 1, s_ed.cursor_col + 1,
                 s_ed.line_count, s_ed.content_len,
                 s_ed.is_modified ? " *" : "");
}

/*=============================================================================
 *  Undo System
 *=============================================================================*/

static void editor_push_undo(undo_type_t type, uint16_t pos, uint16_t len, const char *data)
{
    if (s_ed.undo_top >= EDIT_MAX_UNDO) {
        memmove(&s_ed.undo_stack[0], &s_ed.undo_stack[1],
                (EDIT_MAX_UNDO - 1) * sizeof(undo_record_t));
        s_ed.undo_top = EDIT_MAX_UNDO - 1;
    }
    undo_record_t *r = &s_ed.undo_stack[s_ed.undo_top++];
    r->type = type;
    r->pos = pos;
    r->len = (len > sizeof(r->data)) ? sizeof(r->data) : len;
    if (data && r->len > 0)
        memcpy(r->data, data, r->len);
}

static void editor_undo(void)
{
    if (s_ed.undo_top <= 0) return;
    undo_record_t *r = &s_ed.undo_stack[--s_ed.undo_top];

    switch (r->type) {
    case UNDO_INSERT:
        if (r->pos + r->len <= s_ed.content_len) {
            memmove(&s_ed.content[r->pos], &s_ed.content[r->pos + r->len],
                    s_ed.content_len - r->pos - r->len + 1);
            s_ed.content_len -= r->len;
        }
        s_ed.cursor_pos = r->pos;
        break;
    case UNDO_DELETE:
        if (s_ed.content_len + r->len < EDIT_BUF_SIZE) {
            memmove(&s_ed.content[r->pos + r->len], &s_ed.content[r->pos],
                    s_ed.content_len - r->pos + 1);
            memcpy(&s_ed.content[r->pos], r->data, r->len);
            s_ed.content_len += r->len;
        }
        s_ed.cursor_pos = r->pos + r->len;
        break;
    }

    s_ed.is_modified = true;
    editor_parse_lines();
    editor_update_cursor_coords();
    editor_ensure_cursor_visible();
    editor_update_status();
    editor_invalidate_content();
}

static void editor_insert_char(char ch)
{
    if (s_ed.content_len >= EDIT_BUF_SIZE - 1) return;

    if (s_ed.has_selection) {
        uint16_t start = s_ed.sel_start;
        uint16_t end = s_ed.sel_end;
        uint16_t del_len = end - start;
        editor_push_undo(UNDO_DELETE, start, del_len, &s_ed.content[start]);
        memmove(&s_ed.content[start], &s_ed.content[end],
                s_ed.content_len - end + 1);
        s_ed.content_len -= del_len;
        s_ed.cursor_pos = start;
        s_ed.has_selection = false;
    }

    editor_push_undo(UNDO_INSERT, s_ed.cursor_pos, 1, &ch);
    memmove(&s_ed.content[s_ed.cursor_pos + 1], &s_ed.content[s_ed.cursor_pos],
            s_ed.content_len - s_ed.cursor_pos + 1);
    s_ed.content[s_ed.cursor_pos] = ch;
    s_ed.cursor_pos++;
    s_ed.content_len++;

    s_ed.is_modified = true;
    editor_parse_lines();
    editor_update_cursor_coords();
    editor_ensure_cursor_visible();
    editor_update_status();
    editor_invalidate_content();
}

static void editor_delete_char(void)
{
    if (s_ed.has_selection) {
        uint16_t start = s_ed.sel_start;
        uint16_t end = s_ed.sel_end;
        uint16_t del_len = end - start;
        editor_push_undo(UNDO_DELETE, start, del_len, &s_ed.content[start]);
        memmove(&s_ed.content[start], &s_ed.content[end],
                s_ed.content_len - end + 1);
        s_ed.content_len -= del_len;
        s_ed.cursor_pos = start;
        s_ed.has_selection = false;
    } else if (s_ed.cursor_pos < s_ed.content_len) {
        char ch = s_ed.content[s_ed.cursor_pos];
        editor_push_undo(UNDO_DELETE, s_ed.cursor_pos, 1, &ch);
        memmove(&s_ed.content[s_ed.cursor_pos], &s_ed.content[s_ed.cursor_pos + 1],
                s_ed.content_len - s_ed.cursor_pos);
        s_ed.content_len--;
    }

    s_ed.is_modified = true;
    editor_parse_lines();
    editor_update_cursor_coords();
    editor_ensure_cursor_visible();
    editor_update_status();
    editor_invalidate_content();
}

static void editor_backspace(void)
{
    if (s_ed.has_selection) {
        editor_delete_char();
        return;
    }
    if (s_ed.cursor_pos > 0) {
        s_ed.cursor_pos--;
        char ch = s_ed.content[s_ed.cursor_pos];
        editor_push_undo(UNDO_DELETE, s_ed.cursor_pos, 1, &ch);
        memmove(&s_ed.content[s_ed.cursor_pos], &s_ed.content[s_ed.cursor_pos + 1],
                s_ed.content_len - s_ed.cursor_pos);
        s_ed.content_len--;
        s_ed.is_modified = true;
        editor_parse_lines();
        editor_update_cursor_coords();
        editor_ensure_cursor_visible();
        editor_update_status();
        editor_invalidate_content();
    }
}

static void editor_insert_newline(void)
{
    int16_t old_line = s_ed.cursor_line;
    editor_insert_char('\n');
    editor_invalidate_line(old_line);
    editor_invalidate_line(s_ed.cursor_line);
}

static void editor_move_cursor_left(bool extend_sel)
{
    int16_t old_line = s_ed.cursor_line;
    uint16_t old_pos = s_ed.cursor_pos;
    if (s_ed.cursor_pos > 0) {
        s_ed.cursor_pos--;
        editor_update_cursor_coords();
        editor_ensure_cursor_visible();
    }
    if (extend_sel) {
        if (!s_ed.has_selection) {
            s_ed.sel_start = old_pos;
            s_ed.sel_end = old_pos;
            s_ed.has_selection = true;
        }
        if (s_ed.cursor_pos < s_ed.sel_start)
            s_ed.sel_start = s_ed.cursor_pos;
        else
            s_ed.sel_end = s_ed.cursor_pos + 1;
    } else {
        s_ed.has_selection = false;
    }
    editor_update_status();
    editor_invalidate_line(old_line);
    editor_invalidate_line(s_ed.cursor_line);
    editor_invalidate_status();
}

static void editor_move_cursor_right(bool extend_sel)
{
    int16_t old_line = s_ed.cursor_line;
    uint16_t old_pos = s_ed.cursor_pos;
    if (s_ed.cursor_pos < s_ed.content_len) {
        s_ed.cursor_pos++;
        editor_update_cursor_coords();
        editor_ensure_cursor_visible();
    }
    if (extend_sel) {
        if (!s_ed.has_selection) {
            s_ed.sel_start = old_pos;
            s_ed.sel_end = old_pos;
            s_ed.has_selection = true;
        }
        if (s_ed.cursor_pos > s_ed.sel_end)
            s_ed.sel_end = s_ed.cursor_pos;
        else
            s_ed.sel_start = s_ed.cursor_pos;
    } else {
        s_ed.has_selection = false;
    }
    editor_update_status();
    editor_invalidate_line(old_line);
    editor_invalidate_line(s_ed.cursor_line);
    editor_invalidate_status();
}

static void editor_move_cursor_up(bool extend_sel)
{
    int16_t old_line = s_ed.cursor_line;
    uint16_t old_pos = s_ed.cursor_pos;
    if (s_ed.cursor_line > 0) {
        int16_t target_line = s_ed.cursor_line - 1;
        uint16_t line_start = s_ed.line_offsets[target_line];
        uint16_t line_end = (target_line + 1 < s_ed.line_count) ?
                            s_ed.line_offsets[target_line + 1] : s_ed.content_len;
        uint16_t target_col = s_ed.cursor_col;
        uint16_t line_len = line_end - line_start;
        if (line_len > 0 && s_ed.content[line_end - 1] == '\n') line_len--;
        if (target_col > line_len) target_col = line_len;
        s_ed.cursor_pos = line_start + target_col;
        editor_update_cursor_coords();
        editor_ensure_cursor_visible();
    }
    if (extend_sel) {
        if (!s_ed.has_selection) {
            s_ed.sel_start = old_pos;
            s_ed.sel_end = old_pos;
            s_ed.has_selection = true;
        }
        if (s_ed.cursor_pos < s_ed.sel_start)
            s_ed.sel_start = s_ed.cursor_pos;
        else
            s_ed.sel_end = s_ed.cursor_pos + 1;
    } else {
        s_ed.has_selection = false;
    }
    editor_update_status();
    editor_invalidate_line(old_line);
    editor_invalidate_line(s_ed.cursor_line);
    editor_invalidate_status();
}

static void editor_move_cursor_down(bool extend_sel)
{
    int16_t old_line = s_ed.cursor_line;
    uint16_t old_pos = s_ed.cursor_pos;
    if (s_ed.cursor_line < s_ed.line_count - 1) {
        int16_t target_line = s_ed.cursor_line + 1;
        uint16_t line_start = s_ed.line_offsets[target_line];
        uint16_t line_end = (target_line + 1 < s_ed.line_count) ?
                            s_ed.line_offsets[target_line + 1] : s_ed.content_len;
        uint16_t target_col = s_ed.cursor_col;
        uint16_t line_len = line_end - line_start;
        if (line_len > 0 && s_ed.content[line_end - 1] == '\n') line_len--;
        if (target_col > line_len) target_col = line_len;
        s_ed.cursor_pos = line_start + target_col;
        editor_update_cursor_coords();
        editor_ensure_cursor_visible();
    }
    if (extend_sel) {
        if (!s_ed.has_selection) {
            s_ed.sel_start = old_pos;
            s_ed.sel_end = old_pos;
            s_ed.has_selection = true;
        }
        if (s_ed.cursor_pos > s_ed.sel_end)
            s_ed.sel_end = s_ed.cursor_pos;
        else
            s_ed.sel_start = s_ed.cursor_pos;
    } else {
        s_ed.has_selection = false;
    }
    editor_update_status();
    editor_invalidate_line(old_line);
    editor_invalidate_line(s_ed.cursor_line);
    editor_invalidate_status();
}

static void editor_move_cursor_home(bool extend_sel)
{
    int16_t old_line = s_ed.cursor_line;
    uint16_t old_pos = s_ed.cursor_pos;
    s_ed.cursor_pos = s_ed.line_offsets[s_ed.cursor_line];
    editor_update_cursor_coords();
    if (extend_sel) {
        if (!s_ed.has_selection) {
            s_ed.sel_start = old_pos;
            s_ed.sel_end = old_pos;
            s_ed.has_selection = true;
        }
        if (s_ed.cursor_pos < s_ed.sel_start)
            s_ed.sel_start = s_ed.cursor_pos;
        else
            s_ed.sel_end = s_ed.cursor_pos + 1;
    } else {
        s_ed.has_selection = false;
    }
    editor_update_status();
    editor_invalidate_line(old_line);
    editor_invalidate_status();
}

static void editor_move_cursor_end(bool extend_sel)
{
    int16_t old_line = s_ed.cursor_line;
    uint16_t old_pos = s_ed.cursor_pos;
    uint16_t line_end = (s_ed.cursor_line + 1 < s_ed.line_count) ?
                        s_ed.line_offsets[s_ed.cursor_line + 1] : s_ed.content_len;
    if (line_end > 0 && s_ed.content[line_end - 1] == '\n')
        s_ed.cursor_pos = line_end - 1;
    else
        s_ed.cursor_pos = line_end;
    editor_update_cursor_coords();
    if (extend_sel) {
        if (!s_ed.has_selection) {
            s_ed.sel_start = old_pos;
            s_ed.sel_end = old_pos;
            s_ed.has_selection = true;
        }
        if (s_ed.cursor_pos > s_ed.sel_end)
            s_ed.sel_end = s_ed.cursor_pos;
        else
            s_ed.sel_start = s_ed.cursor_pos;
    } else {
        s_ed.has_selection = false;
    }
    editor_update_status();
    editor_invalidate_line(old_line);
    editor_invalidate_status();
}

static void editor_select_all(void)
{
    s_ed.sel_start = 0;
    s_ed.sel_end = s_ed.content_len;
    s_ed.has_selection = true;
    s_ed.cursor_pos = s_ed.content_len;
    editor_update_cursor_coords();
    editor_update_status();
    editor_invalidate_content();
}

/*=============================================================================
 *  Protocol Callbacks
 *=============================================================================*/

static void editor_on_cli_stream(const char *chunk, uint16_t len, bool is_last)
{
    /* File content received via CLI "cat" command */
    if (!s_ed.save_sent) {
        uint16_t space = EDIT_BUF_SIZE - 1 - s_ed.content_len;
        uint16_t copy_len = (len < space) ? len : space;
        if (copy_len > 0) {
            memcpy(&s_ed.content[s_ed.content_len], chunk, copy_len);
            s_ed.content_len += copy_len;
        }

        if (is_last) {
            s_ed.content[s_ed.content_len] = '\0';
            s_ed.is_loading = false;
            s_ed.has_content = true;
            s_ed.is_modified = false;
            s_ed.scroll_line = 0;
            s_ed.cursor_pos = 0;
            s_ed.has_selection = false;
            s_ed.undo_top = 0;
            editor_parse_lines();
            editor_update_cursor_coords();
            editor_update_status();
            editor_invalidate_content();
        }
    }
}

static uart_cli_cb_t s_editor_cb = {
    .on_cli_stream = editor_on_cli_stream,
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
    s_ed.cursor_pos = 0;
    s_ed.cursor_line = 0;
    s_ed.cursor_col = 0;
    s_ed.has_selection = false;
    s_ed.is_modified = false;
    s_ed.is_loading = true;
    s_ed.has_content = false;
    s_ed.save_sent = false;
    s_ed.undo_top = 0;

    strncpy(s_ed.file_name, name, sizeof(s_ed.file_name) - 1);
    s_ed.file_name[sizeof(s_ed.file_name) - 1] = '\0';
    strncpy(s_ed.file_path, path, sizeof(s_ed.file_path) - 1);
    s_ed.file_path[sizeof(s_ed.file_path) - 1] = '\0';

    ui_label_set_text(&s_app_editor.lbl_title, s_ed.file_name);
    UART_SetCLICallbacks(&s_editor_cb);
    UART_RequestFileRead(s_ed.file_path);
    editor_update_status();
}

static void editor_save_file(void)
{
    if (!s_ed.has_content || s_ed.save_sent) return;

    UART_SetCLICallbacks(&s_editor_cb);
    s_ed.save_sent = true;
    editor_update_status();
    editor_invalidate_status();

    /* Save via CLI write command with multi-frame support:
     *   write -s "filepath" <data>   (create/truncate + first chunk)
     *   write -a <data>              (append chunks)
     *   write -e [data]              (close file)
     */
    char cmd[PROTO_MAX_DATA_LEN + 32];
    int path_len = strlen(s_ed.file_path);
    int max_cmd = PROTO_MAX_DATA_LEN - 1; /* 254 bytes */

    int first_overhead = 12 + path_len;
    int first_avail = max_cmd - first_overhead;
    if (first_avail < 0) first_avail = 0;

    int append_overhead = 9;
    int append_avail = max_cmd - append_overhead;

    uint16_t offset = 0;
    uint16_t remaining = s_ed.content_len;

    /* --- First frame: write -s "path" <data> --- */
    {
        int pos = 0;
        memcpy(&cmd[pos], "write -s \"", 10); pos += 10;
        memcpy(&cmd[pos], s_ed.file_path, path_len); pos += path_len;
        memcpy(&cmd[pos], "\" ", 2); pos += 2;

        uint16_t chunk = (remaining < (uint16_t)first_avail) ? remaining : (uint16_t)first_avail;
        if (chunk > 0) {
            memcpy(&cmd[pos], &s_ed.content[offset], chunk);
            pos += chunk;
        }
        cmd[pos] = '\0';
        UART_SendCLI(cmd);
        offset += chunk;
        remaining -= chunk;
    }

    /* --- Middle frames: write -a <data> --- */
    while (remaining > (uint16_t)append_avail) {
        int pos = 0;
        memcpy(&cmd[pos], "write -a ", append_overhead); pos += append_overhead;
        memcpy(&cmd[pos], &s_ed.content[offset], (uint16_t)append_avail);
        pos += append_avail;
        cmd[pos] = '\0';
        UART_SendCLI(cmd);
        offset += (uint16_t)append_avail;
        remaining -= (uint16_t)append_avail;
    }

    /* --- Final frame: write -e [remaining data] (close file) --- */
    {
        int pos = 0;
        memcpy(&cmd[pos], "write -e ", append_overhead); pos += append_overhead;
        if (remaining > 0) {
            memcpy(&cmd[pos], &s_ed.content[offset], remaining);
            pos += remaining;
        }
        cmd[pos] = '\0';
        UART_SendCLI(cmd);
    }

    s_ed.save_sent = false;
    s_ed.is_modified = false;
    editor_update_status();
    editor_invalidate_status();
}

/*=============================================================================
 *  Drawing (1bpp)
 *=============================================================================*/

static void editor_draw_gutter(void)
{
    ui_rect_t gbg = {0, EDIT_TEXT_Y, EDIT_GUTTER_W, EDIT_TEXT_H};
    ui_draw_fill_rect(&gbg, UI_COLOR_WHITE);
    ui_draw_vline(EDIT_GUTTER_W - 1, EDIT_TEXT_Y, EDIT_TEXT_H, UI_COLOR_BLACK);

    for (int i = 0; i < EDIT_VISIBLE_LINES; i++) {
        int ln = s_ed.scroll_line + i + 1;
        if (ln > s_ed.line_count) break;
        char nb[8];
        snprintf(nb, sizeof(nb), "%d", ln);
        int16_t y = EDIT_TEXT_Y + i * EDIT_LINE_H + 2;
        int16_t nw = ui_text_width(nb, &font_montserrat_12);
        ui_draw_text(EDIT_GUTTER_W - nw - 8, y, nb, &font_montserrat_12, UI_COLOR_BLACK);
    }

    /* Current-line marker triangle in the gutter */
    if (s_ed.has_content &&
        s_ed.cursor_line >= s_ed.scroll_line &&
        s_ed.cursor_line < s_ed.scroll_line + EDIT_VISIBLE_LINES) {
        int16_t my = EDIT_TEXT_Y + (s_ed.cursor_line - s_ed.scroll_line) * EDIT_LINE_H + 4;
        ui_draw_text(4, my, ">", &font_montserrat_12, UI_COLOR_BLACK);
    }
}

static void editor_draw_text_area(void)
{
    ui_rect_t tbg = {EDIT_TEXT_X, EDIT_TEXT_Y, EDIT_TEXT_W, EDIT_TEXT_H};
    ui_draw_fill_rect(&tbg, UI_COLOR_WHITE);

    if (!s_ed.has_content) {
        ui_draw_text_in_rect(&tbg,
            s_ed.is_loading ? "Loading file..." : "No file loaded",
            &font_montserrat_12, UI_COLOR_BLACK, 1);
        return;
    }

    /* Draw each visible line */
    for (int i = 0; i < EDIT_VISIBLE_LINES; i++) {
        int li = s_ed.scroll_line + i;
        if (li >= s_ed.line_count) break;

        uint16_t start = s_ed.line_offsets[li];
        uint16_t end = (li + 1 < s_ed.line_count) ?
                        s_ed.line_offsets[li + 1] : s_ed.content_len;
        char lb[EDIT_MAX_LINE_LEN];
        uint16_t ll = end - start;
        if (ll > sizeof(lb) - 1) ll = sizeof(lb) - 1;
        memcpy(lb, &s_ed.content[start], ll);
        lb[ll] = '\0';
        while (ll > 0 && (lb[ll - 1] == '\n' || lb[ll - 1] == '\r'))
            lb[--ll] = '\0';

        int16_t y = EDIT_TEXT_Y + i * EDIT_LINE_H + 2;

        /* Selection highlight for this line (inverted strip) */
        if (s_ed.has_selection) {
            uint16_t sel_s = s_ed.sel_start;
            uint16_t sel_e = s_ed.sel_end;
            if (start < sel_e && end > sel_s) {
                uint16_t line_sel_start = (sel_s > start) ? sel_s : start;
                uint16_t line_sel_end = (sel_e < end) ? sel_e : end;
                int16_t sel_x_start = EDIT_TEXT_X + 8;
                int16_t sel_x_end = EDIT_TEXT_X + 8;
                char tmp[EDIT_MAX_LINE_LEN];
                uint16_t before_len = line_sel_start - start;
                if (before_len > sizeof(tmp) - 1) before_len = sizeof(tmp) - 1;
                memcpy(tmp, lb, before_len);
                tmp[before_len] = '\0';
                sel_x_start += ui_text_width(tmp, &font_montserrat_12);

                uint16_t sel_len = line_sel_end - start;
                if (sel_len > sizeof(tmp) - 1) sel_len = sizeof(tmp) - 1;
                memcpy(tmp, lb, sel_len);
                tmp[sel_len] = '\0';
                sel_x_end += ui_text_width(tmp, &font_montserrat_12);

                ui_rect_t sel_rect = {sel_x_start, y - 1, sel_x_end - sel_x_start, EDIT_LINE_H - 2};
                ui_draw_fill_rect(&sel_rect, UI_COLOR_BLACK);

                /* Redraw selected text in white over the black strip */
                char sel_txt[EDIT_MAX_LINE_LEN];
                uint16_t st_off = line_sel_start - start;
                uint16_t st_len = line_sel_end - line_sel_start;
                if (st_len > sizeof(sel_txt) - 1) st_len = sizeof(sel_txt) - 1;
                memcpy(sel_txt, lb + st_off, st_len);
                sel_txt[st_len] = '\0';
                ui_draw_text(sel_x_start, y, sel_txt, &font_montserrat_12, UI_COLOR_WHITE);

                /* Draw non-selected prefix/suffix normally */
                if (st_off > 0) {
                    char pre[EDIT_MAX_LINE_LEN];
                    if (st_off > sizeof(pre) - 1) st_off = sizeof(pre) - 1;
                    memcpy(pre, lb, st_off);
                    pre[st_off] = '\0';
                    ui_draw_text(EDIT_TEXT_X + 8, y, pre, &font_montserrat_12, UI_COLOR_BLACK);
                }
                {
                    uint16_t suf_off = line_sel_end - start;
                    if (suf_off < ll) {
                        ui_draw_text(sel_x_end, y, lb + suf_off, &font_montserrat_12, UI_COLOR_BLACK);
                    }
                }
                continue;
            }
        }

        ui_draw_text(EDIT_TEXT_X + 8, y, lb, &font_montserrat_12, UI_COLOR_BLACK);
    }

    /* Draw cursor */
    if (s_ed.cursor_visible && s_ed.has_content &&
        s_ed.cursor_line >= s_ed.scroll_line &&
        s_ed.cursor_line < s_ed.scroll_line + EDIT_VISIBLE_LINES) {
        int16_t cur_y = EDIT_TEXT_Y + (s_ed.cursor_line - s_ed.scroll_line) * EDIT_LINE_H;

        uint16_t line_start = s_ed.line_offsets[s_ed.cursor_line];
        char before_cursor[EDIT_MAX_LINE_LEN];
        uint16_t bclen = s_ed.cursor_pos - line_start;
        if (bclen > sizeof(before_cursor) - 1) bclen = sizeof(before_cursor) - 1;
        memcpy(before_cursor, &s_ed.content[line_start], bclen);
        before_cursor[bclen] = '\0';
        int16_t cur_x = EDIT_TEXT_X + 8 + ui_text_width(before_cursor, &font_montserrat_12);

        ui_draw_vline(cur_x, cur_y + 1, EDIT_LINE_H - 2, UI_COLOR_BLACK);
        ui_rect_t cur_block = {cur_x - 1, cur_y + EDIT_LINE_H - 4, 3, 3};
        ui_draw_fill_rect(&cur_block, UI_COLOR_BLACK);
    }
}

static void editor_draw_status(void)
{
    ui_rect_t sbg = {0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_STATUS_H};
    ui_draw_fill_rect(&sbg, UI_COLOR_WHITE);
    ui_draw_hline(0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, UI_COLOR_BLACK);

    /* File name + modified marker */
    ui_draw_text(12, EDIT_STATUS_Y + 6, s_ed.file_name,
                 &font_montserrat_12, UI_COLOR_BLACK);
    if (s_ed.is_modified) {
        int16_t nw = ui_text_width(s_ed.file_name, &font_montserrat_12);
        ui_draw_text(12 + nw + 4, EDIT_STATUS_Y + 6, "*",
                     &font_montserrat_12, UI_COLOR_BLACK);
    }

    /* Status info on the right */
    int16_t sw = ui_text_width(s_status_buf, &font_montserrat_12);
    ui_draw_text(UI_SCREEN_WIDTH - sw - 12, EDIT_STATUS_Y + 6,
                 s_status_buf, &font_montserrat_12, UI_COLOR_BLACK);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void editor_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetCLICallbacks(&s_editor_cb);
    editor_update_status();
    ui_page_invalidate_all();
}

static void editor_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearCLICallbacks();
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
    if (!s_ed.has_content) return;

    /* Mouse wheel scrolling */
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        int max_s = s_ed.line_count - EDIT_VISIBLE_LINES;
        if (max_s < 0) max_s = 0;
        s_ed.scroll_line -= e->scroll_delta;
        if (s_ed.scroll_line > max_s) s_ed.scroll_line = max_s;
        if (s_ed.scroll_line < 0) s_ed.scroll_line = 0;
        editor_invalidate_text_area();
        return;
    }

    /* Touch swipe scrolling */
    if (e->type == UI_EVENT_SWIPE_UP) {
        int max_s = s_ed.line_count - EDIT_VISIBLE_LINES;
        if (max_s < 0) max_s = 0;
        s_ed.scroll_line += 3;
        if (s_ed.scroll_line > max_s) s_ed.scroll_line = max_s;
        editor_invalidate_text_area();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_ed.scroll_line -= 3;
        if (s_ed.scroll_line < 0) s_ed.scroll_line = 0;
        editor_invalidate_text_area();
        return;
    }

    /* Tap to position cursor */
    if (e->type == UI_EVENT_CLICK) {
        int16_t rel_y = e->touch.y - EDIT_TEXT_Y;
        int16_t rel_x = e->touch.x - EDIT_TEXT_X - 8;
        if (rel_y < 0 || rel_y >= EDIT_TEXT_H) return;
        if (rel_x < 0) rel_x = 0;

        int16_t target_line = s_ed.scroll_line + rel_y / EDIT_LINE_H;
        if (target_line < 0) target_line = 0;
        if (target_line >= s_ed.line_count) target_line = s_ed.line_count - 1;

        uint16_t line_start = s_ed.line_offsets[target_line];
        uint16_t line_end = (target_line + 1 < s_ed.line_count) ?
                            s_ed.line_offsets[target_line + 1] : s_ed.content_len;

        char line_buf[EDIT_MAX_LINE_LEN];
        uint16_t line_len = line_end - line_start;
        if (line_len > sizeof(line_buf) - 1) line_len = sizeof(line_buf) - 1;
        memcpy(line_buf, &s_ed.content[line_start], line_len);
        line_buf[line_len] = '\0';

        uint16_t best_pos = line_start;
        int16_t best_dist = rel_x;
        int16_t acc_width = 0;

        for (uint16_t ci = 0; ci < line_len; ci++) {
            char tmp[2] = {line_buf[ci], '\0'};
            int16_t char_w = ui_text_width(tmp, &font_montserrat_12);
            int16_t mid = acc_width + char_w / 2;
            int16_t dist = rel_x - mid;
            if (dist < 0) dist = -dist;
            if (dist < best_dist) {
                best_dist = dist;
                best_pos = line_start + ci;
            }
            acc_width += char_w;
        }
        {
            int16_t dist = rel_x - acc_width;
            if (dist < 0) dist = -dist;
            if (dist < best_dist) {
                best_pos = line_start + line_len;
                if (best_pos > line_start && s_ed.content[best_pos - 1] == '\n')
                    best_pos--;
            }
        }

        s_ed.has_selection = false;
        s_ed.cursor_pos = best_pos;
        int16_t old_cursor_line = s_ed.cursor_line;
        editor_update_cursor_coords();
        editor_ensure_cursor_visible();
        editor_update_status();
        editor_invalidate_line(old_cursor_line);
        editor_invalidate_line(s_ed.cursor_line);
        editor_invalidate_status();
    }
}

/* Keyboard event handler */
static void editor_handle_key_event(ui_event_t *e)
{
    if (!s_ed.has_content || s_ed.is_loading) return;

    bool shift = (e->key.modifiers & (UI_MOD_LSHIFT | UI_MOD_RSHIFT)) != 0;
    bool ctrl = (e->key.modifiers & (UI_MOD_LCTRL | UI_MOD_RCTRL)) != 0;

    switch (e->type) {
    case UI_EVENT_KEY_UP_ARROW:
        editor_move_cursor_up(shift);
        return;
    case UI_EVENT_KEY_DOWN_ARROW:
        editor_move_cursor_down(shift);
        return;
    case UI_EVENT_KEY_LEFT_ARROW:
        editor_move_cursor_left(shift);
        return;
    case UI_EVENT_KEY_RIGHT_ARROW:
        editor_move_cursor_right(shift);
        return;
    case UI_EVENT_KEY_OK:
        editor_insert_newline();
        return;
    case UI_EVENT_KEY_BACK:
        if (s_ed.has_selection) {
            s_ed.has_selection = false;
            editor_invalidate_content();
            return;
        }
        return;
    default:
        break;
    }

    if (e->type != UI_EVENT_KEY_DOWN && e->type != UI_EVENT_KEY_CLICK &&
        e->type != UI_EVENT_KEY_LONG_REPEAT) return;

    /* Ctrl+key shortcuts */
    if (ctrl && e->char_code) {
        switch (e->char_code) {
        case 's':  editor_save_file();  return;
        case 'z':  editor_undo();       return;
        case 'a':  editor_select_all(); return;
        default:   break;
        }
    }

    /* Home / End keys */
    if (e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_CLICK) {
        switch (e->key.code) {
        case UI_KEY_HOME:
            editor_move_cursor_home(shift);
            return;
        case UI_KEY_END:
            editor_move_cursor_end(shift);
            return;
        }
    }

    /* Backspace / Delete / Tab */
    if (e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) {
        if (e->char_code == 0x08) {
            editor_backspace();
            return;
        }
        if (e->char_code == 0x7F) {
            editor_delete_char();
            return;
        }
    }
    if (e->type == UI_EVENT_KEY_DOWN && e->char_code == 0x09) {
        for (int t = 0; t < EDIT_TAB_SIZE; t++)
            editor_insert_char(' ');
        return;
    }

    /* Printable characters */
    if ((e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) &&
        e->char_code >= 0x20 && e->char_code <= 0x7E) {
        editor_insert_char((char)e->char_code);
    }
}

static void btn_save_click(ui_widget_t *w)
{
    (void)w;
    editor_save_file();
}

/*=============================================================================
 *  Page Event Handler (keyboard intercept before widget dispatch)
 *=============================================================================*/

static bool editor_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    /* Reset cursor blink on any key activity */
    s_ed.cursor_visible = true;
    s_ed.cursor_blink_ms = 0;

    editor_handle_key_event(e);
    return true;
}

/*=============================================================================
 *  Page Update (cursor blink with real dt)
 *=============================================================================*/

static void editor_page_update(ui_page_t *page, uint32_t dt_ms)
{
    (void)page;

    s_ed.cursor_blink_ms += dt_ms;
    if (s_ed.cursor_visible) {
        if (s_ed.cursor_blink_ms >= CURSOR_BLINK_ON_MS) {
            s_ed.cursor_visible = false;
            s_ed.cursor_blink_ms = 0;
            editor_invalidate_cursor();
        }
    } else {
        if (s_ed.cursor_blink_ms >= CURSOR_BLINK_OFF_MS) {
            s_ed.cursor_visible = true;
            s_ed.cursor_blink_ms = 0;
            editor_invalidate_cursor();
        }
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
    s_ed.cursor_visible = true;
    editor_update_status();

    ui_rect_t r_save = {UI_SCREEN_WIDTH - 80, 6, 64, 28};
    ui_button_init(&btn_save, &r_save, "Save", &font_montserrat_12);
    ui_button_set_callback(&btn_save, btn_save_click);
    ui_button_set_colors(&btn_save, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
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
    ui_page_set_callbacks(&s_app_editor.page, editor_page_enter, editor_page_exit, editor_page_draw, NULL);
    ui_page_set_update_cb(&s_app_editor.page, editor_page_update);
    ui_page_set_event_cb(&s_app_editor.page, editor_page_event);
    ui_page_register(&s_app_editor.page);
}

ui_page_t *app_editor_get_page(void) { return &s_app_editor.page; }
