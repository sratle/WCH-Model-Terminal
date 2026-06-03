/********************************** (C) COPYRIGHT *******************************
* File Name          : app_terminal.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/05/25
* Description        : Terminal app — interactive CLI shell on screen.
*                      Connects to Core CLI via UART CLI passthrough.
*                      Full keyboard input, command history, scrolling output.
*                      V2.0: pixel-level scrolling, streaming CLI output,
*                             proper \r\n handling, auto-scroll on new output.
********************************************************************************/
#include "app_terminal.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define TERM_LINE_H         16
#define TERM_MARGIN_X       8
#define TERM_TEXT_Y         (APP_TITLE_BAR_H)
#define TERM_TEXT_H         (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H)
#define TERM_PROMPT_LEN     2   /* "> " */
#define TERM_INPUT_MAX      128
#define TERM_HISTORY_MAX    16
#define TERM_OUTPUT_LINES   512
#define TERM_LINE_MAX       256
#define TERM_STATUS_H       20
#define TERM_CONTENT_H      (TERM_TEXT_H - TERM_STATUS_H)

/* Scroll speeds */
#define TERM_WHEEL_PX       32  /* pixels per mouse wheel notch */
#define TERM_SWIPE_LINES    3   /* lines per swipe gesture */

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define TERM_BG             UI_HEX(0x1E1E2E)
#define TERM_TEXT_COLOR     UI_HEX(0xCDD6F4)
#define TERM_PROMPT_COLOR   UI_HEX(0x89B4FA)
#define TERM_STATUS_BG      UI_HEX(0x181825)
#define TERM_STATUS_TEXT    UI_HEX(0x6C7086)
#define TERM_CURSOR_COLOR   UI_HEX(0xF5E0DC)
#define TERM_INPUT_COLOR    UI_HEX(0xA6E3A1)
#define TERM_ERROR_COLOR    UI_HEX(0xF38BA8)
#define TERM_BORDER         UI_HEX(0x313244)

/*=============================================================================
 *  Terminal Line Buffer
 *=============================================================================*/

typedef struct {
    char text[TERM_LINE_MAX];
    uint16_t len;
} term_line_t;

/*=============================================================================
 *  Terminal State
 *=============================================================================*/

typedef struct {
    /* Output buffer: circular array of lines */
    term_line_t lines[TERM_OUTPUT_LINES];
    uint16_t line_count;       /* Total lines written (wraps) */
    uint16_t line_head;        /* Index of oldest line in buffer */

    /* Pixel-level scroll: scroll_y = pixel offset from top of content.
     * Content height = (line_count + 1) * TERM_LINE_H  (+1 for input line)
     * scroll_y ranges from 0 to max_scroll_y.
     * 0 means top of content is at top of viewport.
     * max_scroll_y means bottom of content is at bottom of viewport. */
    int16_t scroll_y;

    /* Auto-scroll: when true, scroll_y tracks content bottom automatically.
     * Set to true initially and after user scrolls to bottom.
     * Set to false when user scrolls up away from bottom. */
    bool auto_scroll;

    /* Input line */
    char input[TERM_INPUT_MAX];
    uint16_t input_len;
    uint16_t cursor_pos;       /* Character position in input (0..input_len) */

    /* Command history */
    char history[TERM_HISTORY_MAX][TERM_INPUT_MAX];
    uint16_t history_count;
    int16_t history_idx;       /* -1 = not browsing history */

    /* Status */
    bool cli_busy;             /* Waiting for CLI response */
    char cwd[64];              /* Current working directory */

    /* Cursor blink */
    uint32_t cursor_blink_ms;
    bool cursor_visible;
} terminal_state_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_app_terminal;
static ui_widget_t s_term_touch;
static ui_widget_t *s_terminal_widgets[3]; /* back + title + touch */
static terminal_state_t s_term;

/*=============================================================================
 *  Scroll Helpers
 *=============================================================================*/

/* Total content height in pixels (output lines + input line) */
static int16_t term_content_height(void)
{
    return ((int16_t)s_term.line_count + 1) * TERM_LINE_H;
}

/* Maximum scroll_y value (clamped so bottom of content = bottom of viewport) */
static int16_t term_max_scroll(void)
{
    int16_t ch = term_content_height();
    int16_t max = ch - TERM_CONTENT_H;
    return (max > 0) ? max : 0;
}

/* Clamp scroll_y to valid range, update auto_scroll flag */
static void term_clamp_scroll(void)
{
    int16_t max = term_max_scroll();
    if (s_term.scroll_y > max) s_term.scroll_y = max;
    if (s_term.scroll_y < 0) s_term.scroll_y = 0;

    /* If scrolled to bottom, re-enable auto-scroll */
    if (s_term.scroll_y >= max) {
        s_term.auto_scroll = true;
    }
}

/* Scroll to bottom (for auto-scroll on new output) */
static void term_scroll_to_bottom(void)
{
    s_term.scroll_y = term_max_scroll();
    s_term.auto_scroll = true;
}

/*=============================================================================
 *  Dirty Region Helpers
 *=============================================================================*/

static void term_invalidate_all(void)
{
    ui_rect_t r = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, TERM_TEXT_H};
    ui_page_invalidate(&r);
}

static void term_invalidate_content(void)
{
    ui_rect_t r = {0, TERM_TEXT_Y, UI_SCREEN_WIDTH, TERM_CONTENT_H};
    ui_page_invalidate(&r);
}

static void term_invalidate_status(void)
{
    ui_rect_t r = {0, UI_SCREEN_HEIGHT - TERM_STATUS_H, UI_SCREEN_WIDTH, TERM_STATUS_H};
    ui_page_invalidate(&r);
}

static void term_invalidate_input_line(void)
{
    /* Calculate the pixel Y position of the input line on screen */
    int16_t input_y_in_content = (int16_t)s_term.line_count * TERM_LINE_H;
    int16_t input_y_on_screen = TERM_TEXT_Y + input_y_in_content - s_term.scroll_y;

    /* Check if input line is visible */
    if (input_y_on_screen + TERM_LINE_H <= TERM_TEXT_Y ||
        input_y_on_screen >= TERM_TEXT_Y + TERM_CONTENT_H)
        return;

    /* Clamp to content area */
    int16_t y = (input_y_on_screen > TERM_TEXT_Y) ? input_y_on_screen : TERM_TEXT_Y;
    int16_t bot = input_y_on_screen + TERM_LINE_H;
    int16_t content_bot = TERM_TEXT_Y + TERM_CONTENT_H;
    if (bot > content_bot) bot = content_bot;

    ui_rect_t r = {0, y, UI_SCREEN_WIDTH, (uint16_t)(bot - y)};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Line Buffer Management
 *=============================================================================*/

static term_line_t *term_get_line(uint16_t idx)
{
    /* idx is absolute (0..line_count-1). Map to circular buffer. */
    if (idx >= s_term.line_count) return NULL;
    uint16_t buf_idx;
    if (s_term.line_count <= TERM_OUTPUT_LINES) {
        buf_idx = idx;
    } else {
        buf_idx = (s_term.line_head + (idx - (s_term.line_count - TERM_OUTPUT_LINES)))
                  % TERM_OUTPUT_LINES;
    }
    return &s_term.lines[buf_idx];
}

static void term_add_line(const char *text, uint16_t len)
{
    uint16_t buf_idx = s_term.line_count % TERM_OUTPUT_LINES;
    if (s_term.line_count >= TERM_OUTPUT_LINES) {
        s_term.line_head = (s_term.line_head + 1) % TERM_OUTPUT_LINES;
    }
    term_line_t *line = &s_term.lines[buf_idx];
    uint16_t copy_len = (len < TERM_LINE_MAX - 1) ? len : (TERM_LINE_MAX - 1);
    memcpy(line->text, text, copy_len);
    line->text[copy_len] = '\0';
    line->len = copy_len;
    s_term.line_count++;
}

/* Split text into lines at '\n' characters, skip '\r', and add to buffer */
static void term_add_output(const char *text, uint16_t len)
{
    uint16_t start = 0;
    for (uint16_t i = 0; i < len; i++) {
        if (text[i] == '\r') {
            /* Flush text before \r as a line */
            if (i > start) {
                term_add_line(&text[start], i - start);
            }
            start = i + 1;
        } else if (text[i] == '\n') {
            if (i > start) {
                term_add_line(&text[start], i - start);
            }
            start = i + 1;
        }
    }
    /* Remaining text after last newline */
    if (start < len) {
        term_add_line(&text[start], len - start);
    }

    /* Auto-scroll to bottom if enabled */
    if (s_term.auto_scroll) {
        term_scroll_to_bottom();
    }
}

/*=============================================================================
 *  History Management
 *=============================================================================*/

static void term_history_push(const char *cmd)
{
    if (cmd[0] == '\0') return;

    /* Skip if same as last history entry */
    if (s_term.history_count > 0) {
        uint16_t last = (s_term.history_count - 1) % TERM_HISTORY_MAX;
        if (strcmp(s_term.history[last], cmd) == 0) return;
    }

    uint16_t idx = s_term.history_count % TERM_HISTORY_MAX;
    strncpy(s_term.history[idx], cmd, TERM_INPUT_MAX - 1);
    s_term.history[idx][TERM_INPUT_MAX - 1] = '\0';
    s_term.history_count++;
}

/*=============================================================================
 *  Protocol Callbacks
 *=============================================================================*/

static void on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    (void)truncated;
    term_add_output(output, len);

    if (is_last) {
        s_term.cli_busy = false;
    }

    term_invalidate_content();
    term_invalidate_status();
}

static void on_cwd_notify(const char *path)
{
    strncpy(s_term.cwd, path, sizeof(s_term.cwd) - 1);
    s_term.cwd[sizeof(s_term.cwd) - 1] = '\0';
    term_invalidate_status();
}

static uart_app_callbacks_t s_terminal_callbacks = {
    .on_file_list = NULL,
    .on_cli_response = on_cli_response,
    .on_cwd_notify = on_cwd_notify,
};

/*=============================================================================
 *  Command Execution
 *=============================================================================*/

static void term_execute(void)
{
    if (s_term.input_len == 0) {
        term_add_line("> ", 2);
        term_invalidate_content();
        return;
    }

    /* Add prompt + command to output */
    char prompt_line[TERM_INPUT_MAX + TERM_PROMPT_LEN];
    prompt_line[0] = '>';
    prompt_line[1] = ' ';
    memcpy(&prompt_line[2], s_term.input, s_term.input_len);
    term_add_line(prompt_line, s_term.input_len + TERM_PROMPT_LEN);

    /* Push to history */
    term_history_push(s_term.input);

    /* Send to Core via CLI passthrough */
    s_term.cli_busy = true;
    UART_SetAppCallbacks(&s_terminal_callbacks);
    UART_SendCLI(s_term.input);

    /* Clear input */
    s_term.input[0] = '\0';
    s_term.input_len = 0;
    s_term.cursor_pos = 0;
    s_term.history_idx = -1;

    term_invalidate_content();
    term_invalidate_status();
}

/*=============================================================================
 *  Input Handling
 *=============================================================================*/

static void term_handle_key_event(ui_event_t *e)
{
    bool ctrl = (e->key_modifiers & (UI_MOD_LCTRL | UI_MOD_RCTRL)) != 0;

    /* Arrow keys for history / cursor */
    switch (e->type) {
    case UI_EVENT_KEY_UP_ARROW:
        if (s_term.history_count > 0) {
            if (s_term.history_idx < 0)
                s_term.history_idx = (int16_t)s_term.history_count - 1;
            else if (s_term.history_idx > 0)
                s_term.history_idx--;
            uint16_t idx = s_term.history_idx % TERM_HISTORY_MAX;
            strncpy(s_term.input, s_term.history[idx], TERM_INPUT_MAX - 1);
            s_term.input[TERM_INPUT_MAX - 1] = '\0';
            s_term.input_len = (uint16_t)strlen(s_term.input);
            s_term.cursor_pos = s_term.input_len;
            term_invalidate_input_line();
        }
        return;
    case UI_EVENT_KEY_DOWN_ARROW:
        if (s_term.history_idx >= 0) {
            s_term.history_idx++;
            if (s_term.history_idx >= (int16_t)s_term.history_count) {
                s_term.history_idx = -1;
                s_term.input[0] = '\0';
                s_term.input_len = 0;
                s_term.cursor_pos = 0;
            } else {
                uint16_t idx = s_term.history_idx % TERM_HISTORY_MAX;
                strncpy(s_term.input, s_term.history[idx], TERM_INPUT_MAX - 1);
                s_term.input[TERM_INPUT_MAX - 1] = '\0';
                s_term.input_len = (uint16_t)strlen(s_term.input);
                s_term.cursor_pos = s_term.input_len;
            }
            term_invalidate_input_line();
        }
        return;
    case UI_EVENT_KEY_LEFT_ARROW:
        if (s_term.cursor_pos > 0) {
            s_term.cursor_pos--;
            s_term.cursor_visible = true;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
        return;
    case UI_EVENT_KEY_RIGHT_ARROW:
        if (s_term.cursor_pos < s_term.input_len) {
            s_term.cursor_pos++;
            s_term.cursor_visible = true;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
        return;
    case UI_EVENT_KEY_OK:
        term_execute();
        return;
    case UI_EVENT_KEY_BACK:
        return;  /* Let page system handle back navigation */
    default:
        break;
    }

    if (e->type != UI_EVENT_KEY_DOWN && e->type != UI_EVENT_KEY_LONG_REPEAT)
        return;

    /* Ctrl+C: cancel / clear input */
    if (ctrl && e->char_code == 'c') {
        s_term.input[0] = '\0';
        s_term.input_len = 0;
        s_term.cursor_pos = 0;
        term_invalidate_input_line();
        return;
    }

    /* Ctrl+U: clear input line */
    if (ctrl && e->char_code == 'u') {
        s_term.input[0] = '\0';
        s_term.input_len = 0;
        s_term.cursor_pos = 0;
        term_invalidate_input_line();
        return;
    }

    /* Ctrl+A: move cursor to start */
    if (ctrl && e->char_code == 'a') {
        s_term.cursor_pos = 0;
        s_term.cursor_visible = true;
        s_term.cursor_blink_ms = 0;
        term_invalidate_input_line();
        return;
    }

    /* Ctrl+E: move cursor to end */
    if (ctrl && e->char_code == 'e') {
        s_term.cursor_pos = s_term.input_len;
        s_term.cursor_visible = true;
        s_term.cursor_blink_ms = 0;
        term_invalidate_input_line();
        return;
    }

    /* Backspace */
    if (e->char_code == 0x08) {
        if (s_term.cursor_pos > 0) {
            memmove(&s_term.input[s_term.cursor_pos - 1],
                    &s_term.input[s_term.cursor_pos],
                    s_term.input_len - s_term.cursor_pos + 1);
            s_term.cursor_pos--;
            s_term.input_len--;
            s_term.cursor_visible = true;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
        return;
    }

    /* Delete */
    if (e->char_code == 0x7F) {
        if (s_term.cursor_pos < s_term.input_len) {
            memmove(&s_term.input[s_term.cursor_pos],
                    &s_term.input[s_term.cursor_pos + 1],
                    s_term.input_len - s_term.cursor_pos);
            s_term.input_len--;
            s_term.cursor_visible = true;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
        return;
    }

    /* Printable characters */
    if (e->char_code >= 0x20 && e->char_code <= 0x7E) {
        if (s_term.input_len < TERM_INPUT_MAX - 1) {
            memmove(&s_term.input[s_term.cursor_pos + 1],
                    &s_term.input[s_term.cursor_pos],
                    s_term.input_len - s_term.cursor_pos + 1);
            s_term.input[s_term.cursor_pos] = (char)e->char_code;
            s_term.cursor_pos++;
            s_term.input_len++;
            s_term.cursor_visible = true;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
        return;
    }
}

/* Touch/mouse event handler for scrolling */
static void term_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    /* Mouse wheel: pixel-level scrolling */
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        s_term.scroll_y -= e->scroll_delta * TERM_WHEEL_PX;
        term_clamp_scroll();
        term_invalidate_content();
        return;
    }

    /* Swipe: scroll by lines */
    if (e->type == UI_EVENT_SWIPE_UP) {
        s_term.scroll_y += TERM_SWIPE_LINES * TERM_LINE_H;
        term_clamp_scroll();
        term_invalidate_content();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_term.scroll_y -= TERM_SWIPE_LINES * TERM_LINE_H;
        term_clamp_scroll();
        term_invalidate_content();
        return;
    }

    /* Double click: scroll to bottom */
    if (e->type == UI_EVENT_DOUBLE_CLICK) {
        term_scroll_to_bottom();
        term_invalidate_content();
        return;
    }
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void term_draw_output(const ui_rect_t *dirty)
{
    /* Fill content background (only within dirty region) */
    ui_draw_fill_rect(dirty, TERM_BG);

    /* Calculate which lines are visible based on pixel scroll */
    int16_t max_scroll = term_max_scroll();
    if (s_term.scroll_y > max_scroll) s_term.scroll_y = max_scroll;
    if (s_term.scroll_y < 0) s_term.scroll_y = 0;

    /* First line that intersects the viewport */
    int16_t first_line = s_term.scroll_y / TERM_LINE_H;
    /* Pixel offset of first_line within the viewport (negative = clipped) */
    int16_t first_line_offset = -(s_term.scroll_y % TERM_LINE_H);

    /* Number of lines that could be visible (including partially) */
    int16_t visible_lines = (TERM_CONTENT_H - first_line_offset + TERM_LINE_H - 1) / TERM_LINE_H + 1;

    /* Only draw lines that intersect the dirty region */
    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;

    for (int16_t i = 0; i < visible_lines; i++) {
        int16_t line_idx = first_line + i;
        int16_t y = TERM_TEXT_Y + first_line_offset + i * TERM_LINE_H;

        /* Skip lines outside dirty region */
        if (y + TERM_LINE_H <= dirty_top || y >= dirty_bot)
            continue;

        /* Skip lines outside content viewport */
        if (y + TERM_LINE_H <= TERM_TEXT_Y || y >= TERM_TEXT_Y + TERM_CONTENT_H)
            continue;

        if (line_idx < (int16_t)s_term.line_count) {
            /* Output line */
            term_line_t *line = term_get_line((uint16_t)line_idx);
            if (line && line->len > 0) {
                ui_color_t color = TERM_TEXT_COLOR;
                const char *text = line->text;
                uint16_t len = line->len;

                if (len >= 2 && text[0] == '>' && text[1] == ' ') {
                    /* Prompt line: draw "> " in prompt color, rest in input color */
                    ui_draw_text(TERM_MARGIN_X, y, "> ", &font_montserrat_12, TERM_PROMPT_COLOR);
                    if (len > 2) {
                        char cmd[TERM_LINE_MAX];
                        uint16_t cmd_len = len - 2;
                        if (cmd_len > sizeof(cmd) - 1) cmd_len = sizeof(cmd) - 1;
                        memcpy(cmd, &text[2], cmd_len);
                        cmd[cmd_len] = '\0';
                        ui_draw_text(TERM_MARGIN_X + ui_text_width("> ", &font_montserrat_12), y,
                                     cmd, &font_montserrat_12, TERM_INPUT_COLOR);
                    }
                } else {
                    /* Check if it looks like an error */
                    if (strstr(text, "error") || strstr(text, "Error") ||
                        strstr(text, "ERROR") || strstr(text, "cannot") ||
                        strstr(text, "failed") || strstr(text, "busy")) {
                        color = TERM_ERROR_COLOR;
                    }
                    ui_draw_text(TERM_MARGIN_X, y, text, &font_montserrat_12, color);
                }
            }
        } else if (line_idx == (int16_t)s_term.line_count) {
            /* Input line: draw prompt + input text + cursor */
            ui_draw_text(TERM_MARGIN_X, y, "> ", &font_montserrat_12, TERM_PROMPT_COLOR);
            int16_t prompt_w = ui_text_width("> ", &font_montserrat_12);
            int16_t text_x = TERM_MARGIN_X + prompt_w;

            if (s_term.input_len > 0) {
                /* Draw text before cursor */
                if (s_term.cursor_pos > 0) {
                    char before[TERM_INPUT_MAX];
                    uint16_t blen = s_term.cursor_pos;
                    if (blen > sizeof(before) - 1) blen = sizeof(before) - 1;
                    memcpy(before, s_term.input, blen);
                    before[blen] = '\0';
                    ui_draw_text(text_x, y, before, &font_montserrat_12, TERM_INPUT_COLOR);
                    text_x += ui_text_width(before, &font_montserrat_12);
                }

                /* Draw cursor */
                if (s_term.cursor_visible) {
                    ui_draw_vline(text_x, y + 1, TERM_LINE_H - 2, TERM_CURSOR_COLOR);
                }

                /* Draw text after cursor */
                if (s_term.cursor_pos < s_term.input_len) {
                    char after[TERM_INPUT_MAX];
                    uint16_t alen = s_term.input_len - s_term.cursor_pos;
                    if (alen > sizeof(after) - 1) alen = sizeof(after) - 1;
                    memcpy(after, &s_term.input[s_term.cursor_pos], alen);
                    after[alen] = '\0';
                    int16_t cursor_w = 2;
                    ui_draw_text(text_x + cursor_w, y, after, &font_montserrat_12, TERM_INPUT_COLOR);
                }
            } else {
                /* Empty input: just draw cursor */
                if (s_term.cursor_visible) {
                    ui_draw_vline(text_x, y + 1, TERM_LINE_H - 2, TERM_CURSOR_COLOR);
                }
            }
        }
        /* Lines beyond line_count+1 are empty — already covered by bg fill */
    }
}

static void term_draw_status(void)
{
    int16_t y = UI_SCREEN_HEIGHT - TERM_STATUS_H;
    ui_rect_t sbg = {0, y, UI_SCREEN_WIDTH, TERM_STATUS_H};
    ui_draw_fill_rect(&sbg, TERM_STATUS_BG);
    ui_draw_hline(0, y, UI_SCREEN_WIDTH, TERM_BORDER);

    char buf[80];
    int pos = 0;
    buf[pos++] = ' ';
    buf[pos++] = '[';

    /* CWD */
    if (s_term.cwd[0]) {
        int cwd_len = (int)strlen(s_term.cwd);
        int max_cwd = 40;
        if (cwd_len > max_cwd) {
            buf[pos++] = '.';
            buf[pos++] = '.';
            memcpy(&buf[pos], &s_term.cwd[cwd_len - max_cwd + 2], max_cwd - 2);
            pos += max_cwd - 2;
        } else {
            memcpy(&buf[pos], s_term.cwd, cwd_len);
            pos += cwd_len;
        }
    } else {
        buf[pos++] = '/';
    }
    buf[pos++] = ']';
    buf[pos++] = ' ';

    if (s_term.cli_busy) {
        const char *busy = " ...";
        int blen = (int)strlen(busy);
        memcpy(&buf[pos], busy, blen);
        pos += blen;
    }

    buf[pos] = '\0';
    ui_draw_text(4, y + 3, buf, &font_montserrat_12, TERM_STATUS_TEXT);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void term_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_terminal_callbacks);
    term_invalidate_all();
}

static void term_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Only draw content/status if dirty region intersects */
    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;
    int16_t content_top = TERM_TEXT_Y;
    int16_t status_top = UI_SCREEN_HEIGHT - TERM_STATUS_H;

    if (dirty_bot > content_top && dirty_top < status_top) {
        term_draw_output(dirty);
    }

    if (dirty_bot > status_top) {
        term_draw_status();
    }
}

static bool term_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    /* Only handle keyboard and core-key events */
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    /* Reset cursor blink on any key activity */
    s_term.cursor_visible = true;
    s_term.cursor_blink_ms = 0;

    term_handle_key_event(e);
    return true;  /* Consume all key events */
}

static void term_page_update(ui_page_t *page)
{
    (void)page;

    /* Cursor blink */
    s_term.cursor_blink_ms += 16;
    if (s_term.cursor_visible) {
        if (s_term.cursor_blink_ms >= 530) {
            s_term.cursor_visible = false;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
    } else {
        if (s_term.cursor_blink_ms >= 470) {
            s_term.cursor_visible = true;
            s_term.cursor_blink_ms = 0;
            term_invalidate_input_line();
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void app_terminal_init(void)
{
    ui_app_page_init(&s_app_terminal, "Terminal", 0x10F);
    memset(&s_term, 0, sizeof(s_term));
    s_term.history_idx = -1;
    s_term.cursor_visible = true;
    s_term.auto_scroll = true;
    strcpy(s_term.cwd, "/");

    /* Welcome message */
    term_add_line("Terminal v2.0", 13);
    term_add_line("Connected to Core CLI via UART", 30);
    term_add_line("Type 'help' for available commands.", 35);
    term_add_line("", 0);

    /* Set initial scroll to bottom */
    term_scroll_to_bottom();

    /* Touch area for scrolling (content area) */
    ui_rect_t touch_rect = {0, TERM_TEXT_Y, UI_SCREEN_WIDTH, TERM_CONTENT_H};
    ui_widget_init(&s_term_touch, &touch_rect);
    s_term_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_term_touch.event_cb = term_touch_event;

    s_terminal_widgets[0] = (ui_widget_t *)&s_app_terminal.btn_back;
    s_terminal_widgets[1] = (ui_widget_t *)&s_app_terminal.lbl_title;
    s_terminal_widgets[2] = &s_term_touch;

    ui_page_set_widgets(&s_app_terminal.page, s_terminal_widgets, 3);
    ui_page_set_callbacks(&s_app_terminal.page, term_page_enter, NULL, term_page_draw, NULL);
    ui_page_set_update_cb(&s_app_terminal.page, term_page_update);
    ui_page_set_event_cb(&s_app_terminal.page, term_page_event);
    ui_page_register(&s_app_terminal.page);
}

ui_page_t *app_terminal_get_page(void)
{
    return &s_app_terminal.page;
}
