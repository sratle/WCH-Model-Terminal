/********************************** (C) COPYRIGHT *******************************
* File Name          : app_editor.c
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/25
* Description        : Text Editor app (V4.0 full-featured).
*                      Read/write files via CLI passthrough.
*                      Full keyboard editing: insert, delete, cursor movement.
*                      Mouse click to position cursor, scroll wheel.
*                      Line numbers, status bar, cursor blink.
*                      Save via CLI "echo" redirect.
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
#define EDIT_MAX_UNDO       64
#define EDIT_TAB_SIZE       4
#define EDIT_MAX_LINE_LEN   256

/* Cursor blink timing */
#define CURSOR_BLINK_ON_MS  530
#define CURSOR_BLINK_OFF_MS 470

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
#define EDIT_CURSOR_COLOR   UI_HEX(0x4FC3F7)
#define EDIT_SELECTION_BG   UI_HEX(0xBBDEFB)
#define EDIT_LINE_HIGHLIGHT UI_HEX(0xF5F5F5)
#define EDIT_MODIFIED_COLOR UI_HEX(0xFF8A65)
#define EDIT_SAVED_COLOR    UI_HEX(0x81C784)

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
    uint16_t sel_start;     /* Start of selection (min) */
    uint16_t sel_end;       /* End of selection (max) */
    bool has_selection;

    /* Modification tracking */
    bool is_modified;
    bool is_loading;
    bool has_content;
    bool save_sent;
    int16_t save_chunks_remaining;  /* pending write frames during multi-frame save */

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

/* Invalidate the text area (gutter + text content) */
static void editor_invalidate_text_area(void)
{
    ui_rect_t r = {0, EDIT_TEXT_Y, UI_SCREEN_WIDTH, EDIT_TEXT_H};
    ui_page_invalidate(&r);
}

/* Invalidate the status bar */
static void editor_invalidate_status(void)
{
    ui_rect_t r = {0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_STATUS_H};
    ui_page_invalidate(&r);
}

/* Invalidate a specific line in the text area */
static void editor_invalidate_line(int16_t line_idx)
{
    if (line_idx < s_ed.scroll_line || line_idx >= s_ed.scroll_line + EDIT_VISIBLE_LINES) return;
    int16_t y = EDIT_TEXT_Y + (line_idx - s_ed.scroll_line) * EDIT_LINE_H;
    ui_rect_t r = {0, y, UI_SCREEN_WIDTH, EDIT_LINE_H};
    ui_page_invalidate(&r);
}

/* Invalidate the cursor region (current line + small area around cursor) */
static void editor_invalidate_cursor(void)
{
    if (s_ed.cursor_line < s_ed.scroll_line ||
        s_ed.cursor_line >= s_ed.scroll_line + EDIT_VISIBLE_LINES) return;
    int16_t y = EDIT_TEXT_Y + (s_ed.cursor_line - s_ed.scroll_line) * EDIT_LINE_H;
    ui_rect_t r = {EDIT_GUTTER_W, y, UI_SCREEN_WIDTH - EDIT_GUTTER_W, EDIT_LINE_H};
    ui_page_invalidate(&r);
}

/* Invalidate text area + status bar (for content changes) */
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

/* Update cursor_line and cursor_col from cursor_pos */
static void editor_update_cursor_coords(void)
{
    /* Binary search for the line containing cursor_pos */
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

    /* Fix: if cursor_pos is exactly at the start of the next line (i.e., the
     * current line ends with '\n' and cursor is positioned right after it),
     * the cursor should be on the next line, not at the end of the current one.
     * Without this, pressing Enter leaves the cursor visually on the old line. */
    if (s_ed.cursor_line + 1 < s_ed.line_count &&
        s_ed.cursor_pos == s_ed.line_offsets[s_ed.cursor_line + 1]) {
        s_ed.cursor_line++;
    }

    /* Column is offset within the line */
    uint16_t line_start = s_ed.line_offsets[s_ed.cursor_line];
    s_ed.cursor_col = s_ed.cursor_pos - line_start;
}

/* Ensure cursor line is visible in the scroll viewport */
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
        /* Shift stack down */
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
        /* Undo insert = delete the inserted chars */
        if (r->pos + r->len <= s_ed.content_len) {
            memmove(&s_ed.content[r->pos], &s_ed.content[r->pos + r->len],
                    s_ed.content_len - r->pos - r->len + 1);
            s_ed.content_len -= r->len;
        }
        s_ed.cursor_pos = r->pos;
        break;
    case UNDO_DELETE:
        /* Undo delete = re-insert the deleted chars */
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

    /* Clear selection if active */
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

    /* Insert character at cursor */
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
        /* Delete selection */
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
        /* Delete character at cursor (forward delete) */
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
        /* Delete selection (same as forward delete) */
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
    /* Ensure both old and new cursor lines are invalidated.
     * editor_insert_char already calls editor_invalidate_content(),
     * but explicitly invalidating both lines guarantees the old cursor
     * is erased and the new cursor line is redrawn even if the full
     * text area invalidation is somehow incomplete. */
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
        /* Try to maintain column position */
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
    /* Position before the newline character */
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

static void on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    /* File content received via CLI "cat" command */
    if (!s_ed.save_sent) {
        uint16_t space = EDIT_BUF_SIZE - 1 - s_ed.content_len;
        uint16_t copy_len = (len < space) ? len : space;
        if (copy_len > 0) {
            memcpy(&s_ed.content[s_ed.content_len], output, copy_len);
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
    } else {
        /* Save response: since we mark save complete immediately after
         * sending all frames, just ignore any late CLI responses. */
        (void)output; (void)len; (void)truncated; (void)is_last;
    }
}

static uart_app_callbacks_t s_editor_callbacks = {
    .on_file_list = NULL,
    .on_cli_response = on_cli_response,
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
    s_ed.save_chunks_remaining = 0;
    s_ed.undo_top = 0;

    strncpy(s_ed.file_name, name, sizeof(s_ed.file_name) - 1);
    s_ed.file_name[sizeof(s_ed.file_name) - 1] = '\0';
    strncpy(s_ed.file_path, path, sizeof(s_ed.file_path) - 1);
    s_ed.file_path[sizeof(s_ed.file_path) - 1] = '\0';

    ui_label_set_text(&s_app_editor.lbl_title, s_ed.file_name);
    UART_SetAppCallbacks(&s_editor_callbacks);
    UART_RequestFileRead(s_ed.file_path);
    editor_update_status();
}

static void editor_save_file(void)
{
    if (!s_ed.has_content || s_ed.save_sent) return;

    UART_SetAppCallbacks(&s_editor_callbacks);
    s_ed.save_sent = true;
    editor_update_status();     /* Show "Saving..." immediately */
    editor_invalidate_status();

    /* Save via CLI write command with multi-frame support:
     *   write -s "filepath" <data>   (create/truncate + first chunk)
     *   write -a <data>              (append chunks)
     *   write -e [data]              (close file)
     *
     * PROTO_MAX_DATA_LEN = 255, UART_SendCLI uses 1 byte for DISP_EXT_CLI,
     * so max command string = 254 bytes.
     */
    char cmd[PROTO_MAX_DATA_LEN + 32];
    int path_len = strlen(s_ed.file_path);
    int max_cmd = PROTO_MAX_DATA_LEN - 1; /* 254 bytes */

    /* First frame overhead: "write -s \"" (10) + path + "\" " (2) = 12 + path_len */
    int first_overhead = 12 + path_len;
    int first_avail = max_cmd - first_overhead;
    if (first_avail < 0) first_avail = 0;

    /* Append/end overhead: "write -a " or "write -e " = 9 bytes */
    int append_overhead = 9;
    int append_avail = max_cmd - append_overhead;  /* 245 bytes per chunk */

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
        printf("[editor] save frame1: len=%d cmd=\"%.40s...\"\r\n", pos, cmd);
        UART_SendCLI(cmd);
        offset += chunk;
        remaining -= chunk;
    }

    /* --- Middle frames: write -a <data> (only if remaining > append_avail) --- */
    while (remaining > (uint16_t)append_avail) {
        int pos = 0;
        memcpy(&cmd[pos], "write -a ", append_overhead); pos += append_overhead;
        memcpy(&cmd[pos], &s_ed.content[offset], (uint16_t)append_avail);
        pos += append_avail;
        cmd[pos] = '\0';
        printf("[editor] save frame_mid: len=%d\r\n", pos);
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
        printf("[editor] save frame_end: len=%d remaining=%d\r\n", pos, remaining);
        UART_SendCLI(cmd);
    }

    /* All frames sent synchronously. Core processes CLI commands in order.
     * Mark save complete immediately — response tracking across multiple
     * UART_SendCLI calls is unreliable due to cli_resp_reset(). */
    s_ed.save_sent = false;
    s_ed.save_chunks_remaining = 0;
    s_ed.is_modified = false;
    editor_update_status();
    editor_invalidate_status();
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
        ui_color_t num_clr = EDIT_GUTTER_TEXT;
        /* Highlight current line number */
        if (s_ed.has_content && (s_ed.scroll_line + i) == s_ed.cursor_line)
            num_clr = UI_COLOR_PRIMARY;
        ui_draw_text(EDIT_GUTTER_W - nw - 8, y, nb, &font_montserrat_12, num_clr);
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

    /* Current line highlight */
    if (s_ed.cursor_line >= s_ed.scroll_line &&
        s_ed.cursor_line < s_ed.scroll_line + EDIT_VISIBLE_LINES) {
        int16_t hl_y = EDIT_TEXT_Y + (s_ed.cursor_line - s_ed.scroll_line) * EDIT_LINE_H;
        ui_rect_t hl_rect = {EDIT_TEXT_X, hl_y, EDIT_TEXT_W, EDIT_LINE_H};
        ui_draw_fill_rect(&hl_rect, EDIT_LINE_HIGHLIGHT);
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

        /* Selection highlight for this line */
        if (s_ed.has_selection) {
            uint16_t sel_s = s_ed.sel_start;
            uint16_t sel_e = s_ed.sel_end;
            /* Check if this line intersects with selection */
            if (start < sel_e && end > sel_s) {
                uint16_t line_sel_start = (sel_s > start) ? sel_s : start;
                uint16_t line_sel_end = (sel_e < end) ? sel_e : end;
                /* Calculate pixel positions */
                int16_t sel_x_start = EDIT_TEXT_X + 8;
                int16_t sel_x_end = EDIT_TEXT_X + 8;
                /* Measure text up to sel_start and sel_end */
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
                ui_draw_fill_rect(&sel_rect, EDIT_SELECTION_BG);
            }
        }

        ui_draw_text(EDIT_TEXT_X + 8, y, lb, &font_montserrat_12, EDIT_TEXT_COLOR);
    }

    /* Draw cursor */
    if (s_ed.cursor_visible && s_ed.has_content &&
        s_ed.cursor_line >= s_ed.scroll_line &&
        s_ed.cursor_line < s_ed.scroll_line + EDIT_VISIBLE_LINES) {
        int16_t cur_y = EDIT_TEXT_Y + (s_ed.cursor_line - s_ed.scroll_line) * EDIT_LINE_H;

        /* Calculate cursor X position by measuring text width up to cursor column */
        uint16_t line_start = s_ed.line_offsets[s_ed.cursor_line];
        char before_cursor[EDIT_MAX_LINE_LEN];
        uint16_t bclen = s_ed.cursor_pos - line_start;
        if (bclen > sizeof(before_cursor) - 1) bclen = sizeof(before_cursor) - 1;
        memcpy(before_cursor, &s_ed.content[line_start], bclen);
        before_cursor[bclen] = '\0';
        int16_t cur_x = EDIT_TEXT_X + 8 + ui_text_width(before_cursor, &font_montserrat_12);

        /* Draw cursor as a thin vertical bar */
        ui_draw_vline(cur_x, cur_y + 1, EDIT_LINE_H - 2, EDIT_CURSOR_COLOR);
        /* Also draw a small block at the bottom for visibility */
        ui_rect_t cur_block = {cur_x - 1, cur_y + EDIT_LINE_H - 4, 3, 3};
        ui_draw_fill_rect(&cur_block, EDIT_CURSOR_COLOR);
    }
}

static void editor_draw_status(void)
{
    ui_rect_t sbg = {0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_STATUS_H};
    ui_draw_fill_rect(&sbg, EDIT_STATUS_BG);
    ui_draw_hline(0, EDIT_STATUS_Y, UI_SCREEN_WIDTH, EDIT_BORDER);

    /* File name with modification indicator */
    ui_color_t name_clr = EDIT_STATUS_TEXT;
    if (s_ed.is_modified)
        name_clr = EDIT_MODIFIED_COLOR;
    else if (s_ed.has_content && !s_ed.is_loading)
        name_clr = EDIT_SAVED_COLOR;

    ui_draw_text(12, EDIT_STATUS_Y + 6, s_ed.file_name,
                 &font_montserrat_12, name_clr);

    /* Status info on the right */
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

    /* Click to position cursor */
    if (e->type == UI_EVENT_CLICK) {
        int16_t rel_y = e->pos.y - EDIT_TEXT_Y;
        int16_t rel_x = e->pos.x - EDIT_TEXT_X - 8;
        if (rel_y < 0 || rel_y >= EDIT_TEXT_H) return;
        if (rel_x < 0) rel_x = 0;

        int16_t target_line = s_ed.scroll_line + rel_y / EDIT_LINE_H;
        if (target_line < 0) target_line = 0;
        if (target_line >= s_ed.line_count) target_line = s_ed.line_count - 1;

        /* Find the character position closest to rel_x */
        uint16_t line_start = s_ed.line_offsets[target_line];
        uint16_t line_end = (target_line + 1 < s_ed.line_count) ?
                            s_ed.line_offsets[target_line + 1] : s_ed.content_len;

        /* Measure character by character to find closest position */
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
        /* Also check position after last character */
        {
            int16_t dist = rel_x - acc_width;
            if (dist < 0) dist = -dist;
            if (dist < best_dist) {
                best_pos = line_start + line_len;
                /* If line ends with newline, position before it */
                if (best_pos > line_start && s_ed.content[best_pos - 1] == '\n')
                    best_pos--;
            }
        }

        /* Handle shift+click for selection */
        if (e->mouse_buttons & 0x80 || e->key_modifiers & UI_MOD_LSHIFT) {
            /* Extend selection */
            if (!s_ed.has_selection) {
                s_ed.sel_start = s_ed.cursor_pos;
                s_ed.sel_end = s_ed.cursor_pos;
                s_ed.has_selection = true;
            }
            if (best_pos < s_ed.sel_start)
                s_ed.sel_start = best_pos;
            else
                s_ed.sel_end = best_pos;
        } else {
            s_ed.has_selection = false;
        }

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

    bool shift = (e->key_modifiers & (UI_MOD_LSHIFT | UI_MOD_RSHIFT)) != 0;
    bool ctrl = (e->key_modifiers & (UI_MOD_LCTRL | UI_MOD_RCTRL)) != 0;

    /* Handle arrow key events (these come as specific event types) */
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
        /* ESC: if selection, clear it; otherwise let page handle back */
        if (s_ed.has_selection) {
            s_ed.has_selection = false;
            editor_invalidate_content();
            return;
        }
        return;  /* Let the page system handle back navigation */
    default:
        break;
    }

    /* Handle KEY_DOWN / KEY_CLICK for other keys */
    if (e->type != UI_EVENT_KEY_DOWN && e->type != UI_EVENT_KEY_CLICK) return;

    /* Ctrl+key shortcuts (using char_code for letter identification) */
    if (ctrl && e->char_code) {
        switch (e->char_code) {
        case 's':  /* Ctrl+S: Save */
            editor_save_file();
            return;
        case 'z':  /* Ctrl+Z: Undo */
            editor_undo();
            return;
        case 'a':  /* Ctrl+A: Select all */
            editor_select_all();
            return;
        case 'c':  /* Ctrl+C: Copy (not implemented) */
            return;
        case 'v':  /* Ctrl+V: Paste (not implemented) */
            return;
        case 'x':  /* Ctrl+X: Cut (not implemented) */
            return;
        default:
            break;
        }
    }

    /* Home / End keys */
    switch (e->key_code) {
    case UI_KEY_HOME:
        editor_move_cursor_home(shift);
        return;
    case UI_KEY_END:
        editor_move_cursor_end(shift);
        return;
    }

    /* Backspace / Delete / Tab — KEY_DOWN (immediate) + KEY_LONG_REPEAT (held repeat) */
    if (e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) {
        /* Backspace via char_code */
        if (e->char_code == 0x08) {
            editor_backspace();
            return;
        }
        /* Delete via char_code (0x7F DEL) */
        if (e->char_code == 0x7F) {
            editor_delete_char();
            return;
        }
    }
    /* Tab (no repeat) */
    if (e->type == UI_EVENT_KEY_DOWN && e->char_code == 0x09) {
        for (int t = 0; t < EDIT_TAB_SIZE; t++)
            editor_insert_char(' ');
        return;
    }

    /* Printable characters via char_code (KEY_DOWN + KEY_LONG_REPEAT for held repeat) */
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

    /* Only handle keyboard and core-key events */
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    /* Reset cursor blink on any key activity */
    s_ed.cursor_visible = true;
    s_ed.cursor_blink_ms = 0;

    editor_handle_key_event(e);
    return true;  /* Consume all key events */
}

/*=============================================================================
 *  Page Update (per-frame logic — keyboard handled by on_page_event now)
 *=============================================================================*/

static void editor_page_update(ui_page_t *page)
{
    (void)page;

    /* Cursor blink */
    s_ed.cursor_blink_ms += 16;  /* Approximate per-frame delta */
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
    /* Keyboard events are now handled by editor_page_event (on_page_event callback). */
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
    ui_page_set_update_cb(&s_app_editor.page, editor_page_update);
    ui_page_set_event_cb(&s_app_editor.page, editor_page_event);
    ui_page_register(&s_app_editor.page);
}

ui_page_t *app_editor_get_page(void) { return &s_app_editor.page; }
