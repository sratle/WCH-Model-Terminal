/********************************** (C) COPYRIGHT *******************************
* File Name          : app_fingerprint.c
* Description        : Fingerprint manager app.
*                      List fingerprints with names from fp.json.
*                      Add/edit names via keyboard input.
*                      Register/delete via CLI passthrough.
********************************************************************************/
#include "app_fingerprint.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Layout (800x480) */
#define FP_LEFT_W       400
#define FP_LIST_X       8
#define FP_LIST_Y       (APP_TITLE_BAR_H + 4)
#define FP_LIST_W       (FP_LEFT_W - 16)
#define FP_LIST_H       (UI_SCREEN_HEIGHT - FP_LIST_Y - 40)
#define FP_ITEM_H       36
#define FP_HDR_H        30
#define FP_VISIBLE      ((FP_LIST_H - FP_HDR_H) / FP_ITEM_H)

#define FP_RIGHT_X      FP_LEFT_W
#define FP_RIGHT_W      (UI_SCREEN_WIDTH - FP_LEFT_W)
#define FP_BTN_W        120
#define FP_BTN_H        34
#define FP_BTN_GAP      10

/* Colors */
#define FP_BG           UI_COLOR_BG_MAIN
#define FP_LIST_BG      UI_COLOR_BG_CARD
#define FP_SEL          UI_HEX(0xE0F2F1)
#define FP_ALT          UI_HEX(0xFAFAFA)
#define FP_BORDER       UI_HEX(0xE0E0E0)
#define FP_CTRL_BG      UI_HEX(0xF0F0F0)
#define FP_STATUS_BG    UI_HEX(0xE8E8E8)

/* State */
#define FP_MAX_ENTRIES  32
#define FP_CLI_BUF_SIZE 2048
#define FP_NAME_MAX     20
#define FP_INPUT_MAX    24

typedef struct {
    uint8_t id;
    char    name[FP_NAME_MAX + 1];
    uint8_t from_hw;    /* 1 = found in hardware list */
} fp_entry_t;

static ui_app_page_t s_app_fp;

static fp_entry_t s_fp_list[FP_MAX_ENTRIES];
static uint8_t    s_fp_count;
static int16_t    s_fp_scroll;
static int16_t    s_fp_selected;

/* CLI response assembly */
static char     s_cli_buf[FP_CLI_BUF_SIZE];
static uint16_t s_cli_len;
static bool     s_cli_assembling;
static uint8_t  s_cli_phase;   /* 0=idle, 1=waiting fp names, 2=waiting fp ls */

/* Editing state */
static bool     s_editing;
static bool     s_adding;      /* true=add new, false=edit existing */
static char     s_input[FP_INPUT_MAX + 1];
static uint8_t  s_input_len;
static uint8_t  s_edit_id;     /* ID being edited (for edit mode) */

/* Status */
static char     s_status[80];

/* Identify result */
static uint8_t  s_id_result;   /* 0=none, 1=ok, 2=fail */
static uint8_t  s_id_fp_id;

/* Widgets */
static ui_widget_t  s_list_touch;
static ui_button_t  btn_register, btn_delete, btn_refresh;
static ui_button_t  btn_add, btn_edit, btn_save, btn_cancel;
static ui_widget_t *s_widgets[2 + 1 + 4 + 4]; /* back + title + touch + buttons */

/* Forward declarations */
static void fp_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last);
static void fp_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                  const uint8_t *evt_data, uint8_t evt_len);
static void fp_set_status(const char *msg);
static void fp_invalidate_list(void);
static void fp_invalidate_status(void);
static void fp_invalidate_input(void);
static void fp_invalidate_all(void);

static uart_app_callbacks_t s_fp_cb = {
    .on_cli_response = fp_on_cli_response,
    .on_file_list = NULL,
    .on_cwd_notify = NULL,
};
static uart_submodel_cb_t s_fp_sub_cb = {
    .on_submodel_event = fp_on_submodel_event,
};

/* ---- Helpers ---- */

static void fp_set_status(const char *msg)
{
    strncpy(s_status, msg, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    fp_invalidate_status();
}

static void fp_invalidate_list(void)
{
    ui_rect_t r = {FP_LIST_X, FP_LIST_Y, FP_LIST_W, FP_LIST_H};
    ui_page_invalidate(&r);
}

static void fp_invalidate_status(void)
{
    ui_rect_t r = {0, UI_SCREEN_HEIGHT - 34, UI_SCREEN_WIDTH, 34};
    ui_page_invalidate(&r);
}

static void fp_invalidate_input(void)
{
    ui_rect_t r = {FP_RIGHT_X + 12, UI_SCREEN_HEIGHT - 100, FP_RIGHT_W - 24, 60};
    ui_page_invalidate(&r);
}

static void fp_invalidate_all(void)
{
    ui_page_invalidate_all();
}

static void fp_start_edit(uint8_t id, const char *existing_name)
{
    s_editing = true;
    s_adding = false;
    s_edit_id = id;
    if (existing_name) {
        strncpy(s_input, existing_name, FP_INPUT_MAX);
        s_input[FP_INPUT_MAX] = '\0';
    } else {
        s_input[0] = '\0';
    }
    s_input_len = (uint8_t)strlen(s_input);
    fp_invalidate_input();
    fp_set_status("Type name, Enter to save, Esc to cancel");
}

static void fp_start_add(void)
{
    s_editing = true;
    s_adding = true;
    s_edit_id = 0;
    s_input[0] = '\0';
    s_input_len = 0;
    fp_invalidate_input();
    fp_set_status("Type name for new fingerprint, Enter to save");
}

static void fp_cancel_edit(void)
{
    s_editing = false;
    s_adding = false;
    fp_invalidate_input();
    fp_set_status("Cancelled");
}

static void fp_save(void)
{
    if (s_input_len == 0) {
        fp_set_status("Name cannot be empty");
        return;
    }

    char cmd[64];
    if (s_adding) {
        /* Find next available ID */
        uint8_t next_id = 0;
        for (uint8_t i = 0; i < s_fp_count; i++) {
            if (s_fp_list[i].id >= next_id)
                next_id = s_fp_list[i].id + 1;
        }
        snprintf(cmd, sizeof(cmd), "fp set %d %s", next_id, s_input);
    } else {
        snprintf(cmd, sizeof(cmd), "fp set %d %s", s_edit_id, s_input);
    }
    UART_SendCLI(cmd);

    s_editing = false;
    s_adding = false;
    fp_invalidate_input();
    fp_set_status("Saved, refreshing...");

    /* Refresh after short delay (let CLI complete) */
    s_cli_phase = 1;
    UART_SendCLI("fp names");
}

static void fp_refresh(void)
{
    s_fp_count = 0;
    s_fp_scroll = 0;
    s_fp_selected = -1;
    s_cli_phase = 1;
    fp_set_status("Loading names...");
    UART_SendCLI("fp names");
}

/* ---- Submodel event handler ---- */

static void fp_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                  const uint8_t *evt_data, uint8_t evt_len)
{
    if (sub_type != SUBMODEL_FINGERPRINT) return;

    switch (sub_cmd) {
    case FP_EVT_IDENTIFY_OK:
    {
        uint8_t fp_id = (evt_data && evt_len >= 2) ? evt_data[1] : 0;
        s_id_result = 1;
        s_id_fp_id = fp_id;
        char msg[48];
        snprintf(msg, sizeof(msg), "Identify OK: ID #%d", fp_id);
        fp_set_status(msg);
        fp_invalidate_all();
        break;
    }
    case FP_EVT_IDENTIFY_FAIL:
        s_id_result = 2;
        s_id_fp_id = 0;
        fp_set_status("Identify FAIL");
        fp_invalidate_all();
        break;
    }
}

/* ---- CLI response parsing ---- */

static void fp_parse_names(const char *text, uint16_t len)
{
    /* "fp names" output: "0: Sratle\n1: John\n..." */
    const char *p = text;
    const char *end = text + len;

    while (p < end && s_fp_count < FP_MAX_ENTRIES) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n') eol++;

        uint16_t line_len = (uint16_t)(eol - p);
        if (line_len > 0) {
            char line[128];
            uint16_t cpy = (line_len < 127) ? line_len : 127;
            memcpy(line, p, cpy);
            line[cpy] = '\0';

            /* Parse "ID: Name" or "ID : Name" */
            const char *colon = strchr(line, ':');
            if (colon) {
                int id_val = atoi(line);
                if (id_val >= 0 && id_val <= 255) {
                    /* Check if already in list */
                    bool found = false;
                    for (uint8_t i = 0; i < s_fp_count; i++) {
                        if (s_fp_list[i].id == (uint8_t)id_val) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        s_fp_list[s_fp_count].id = (uint8_t)id_val;
                        s_fp_list[s_fp_count].from_hw = 0;
                        /* Skip past ": " */
                        const char *name = colon + 1;
                        while (*name == ' ') name++;
                        strncpy(s_fp_list[s_fp_count].name, name, FP_NAME_MAX);
                        s_fp_list[s_fp_count].name[FP_NAME_MAX] = '\0';
                        /* Trim trailing whitespace */
                        int nlen = (int)strlen(s_fp_list[s_fp_count].name) - 1;
                        while (nlen >= 0 && (s_fp_list[s_fp_count].name[nlen] == ' ' ||
                               s_fp_list[s_fp_count].name[nlen] == '\r' ||
                               s_fp_list[s_fp_count].name[nlen] == '\n'))
                            s_fp_list[s_fp_count].name[nlen--] = '\0';
                        s_fp_count++;
                    }
                }
            }
        }

        p = eol;
        while (p < end && (*p == '\r' || *p == '\n')) p++;
    }
}

static void fp_parse_ls(const char *text, uint16_t len)
{
    /* "fp ls" response may contain "ID: X" or "ID=X" patterns */
    const char *p = text;
    const char *end = text + len;

    while (p < end) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n') eol++;

        uint16_t line_len = (uint16_t)(eol - p);
        if (line_len > 0 && line_len < 128) {
            char line[128];
            memcpy(line, p, line_len);
            line[line_len] = '\0';

            const char *id_pos = strstr(line, "ID:");
            if (!id_pos) id_pos = strstr(line, "id:");
            if (!id_pos) id_pos = strstr(line, "ID=");
            if (!id_pos) id_pos = strstr(line, "id=");

            if (id_pos) {
                int id_val = atoi(id_pos + 3);
                if (id_val >= 0 && id_val <= 255) {
                    /* Add to list if not already present */
                    bool found = false;
                    for (uint8_t i = 0; i < s_fp_count; i++) {
                        if (s_fp_list[i].id == (uint8_t)id_val) {
                            s_fp_list[i].from_hw = 1;
                            found = true;
                            break;
                        }
                    }
                    if (!found && s_fp_count < FP_MAX_ENTRIES) {
                        s_fp_list[s_fp_count].id = (uint8_t)id_val;
                        s_fp_list[s_fp_count].from_hw = 1;
                        snprintf(s_fp_list[s_fp_count].name, FP_NAME_MAX + 1,
                                 "Finger_%d", id_val);
                        s_fp_count++;
                    }
                }
            }
        }

        p = eol;
        while (p < end && (*p == '\r' || *p == '\n')) p++;
    }
}

static void fp_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    if (!s_cli_assembling) {
        s_cli_len = 0;
        s_cli_assembling = true;
    }

    uint16_t space = FP_CLI_BUF_SIZE - 1 - s_cli_len;
    uint16_t copy_len = (len < space) ? len : space;
    if (copy_len > 0) {
        memcpy(&s_cli_buf[s_cli_len], output, copy_len);
        s_cli_len += copy_len;
    }
    s_cli_buf[s_cli_len] = '\0';

    if (!is_last) return;

    s_cli_assembling = false;

    /* Phase-based processing */
    if (s_cli_phase == 1) {
        /* Response to "fp names" */
        fp_parse_names(s_cli_buf, s_cli_len);
        /* Now request hardware list */
        s_cli_phase = 2;
        UART_SendCLI("fp ls");
        return;
    }

    if (s_cli_phase == 2) {
        /* Response to "fp ls" */
        fp_parse_ls(s_cli_buf, s_cli_len);
        s_cli_phase = 0;

        char msg[48];
        snprintf(msg, sizeof(msg), "%d fingerprint(s)", s_fp_count);
        fp_set_status(msg);
        fp_invalidate_list();
        return;
    }

    /* Phase 0: ad-hoc CLI responses (register, delete, set, etc.) */
    /* Show response as status */
    char status_msg[78];
    uint16_t slen = s_cli_len;
    if (slen > 77) slen = 77;
    memcpy(status_msg, s_cli_buf, slen);
    status_msg[slen] = '\0';
    for (uint16_t i = 0; i < slen; i++) {
        if (status_msg[i] == '\r' || status_msg[i] == '\n')
            status_msg[i] = ' ';
    }
    fp_set_status(status_msg);
}

/* ---- List touch handler ---- */

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int16_t rel_y = e->pos.y - (FP_LIST_Y + FP_HDR_H) + s_fp_scroll * FP_ITEM_H;
        int16_t idx = rel_y / FP_ITEM_H;
        if (idx >= 0 && idx < s_fp_count) {
            s_fp_selected = idx;
            fp_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_fp_scroll + FP_VISIBLE < s_fp_count) {
            s_fp_scroll++;
            fp_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_fp_scroll > 0) {
            s_fp_scroll--;
            fp_invalidate_list();
        }
    }
}

/* ---- Button callbacks ---- */

static void btn_register_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("fp register");
    fp_set_status("Enrollment started...");
}

static void btn_delete_click(ui_widget_t *w)
{
    (void)w;
    if (s_fp_selected < 0 || s_fp_selected >= s_fp_count) {
        fp_set_status("Select a fingerprint first");
        return;
    }
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "fp del %d", s_fp_list[s_fp_selected].id);
    UART_SendCLI(cmd);
    fp_set_status("Delete request sent");
}

static void btn_refresh_click(ui_widget_t *w)
{
    (void)w;
    fp_refresh();
}

static void btn_add_click(ui_widget_t *w)
{
    (void)w;
    fp_start_add();
}

static void btn_edit_click(ui_widget_t *w)
{
    (void)w;
    if (s_fp_selected < 0 || s_fp_selected >= s_fp_count) {
        fp_set_status("Select a fingerprint first");
        return;
    }
    fp_start_edit(s_fp_list[s_fp_selected].id, s_fp_list[s_fp_selected].name);
}

static void btn_save_click(ui_widget_t *w)
{
    (void)w;
    fp_save();
}

static void btn_cancel_click(ui_widget_t *w)
{
    (void)w;
    fp_cancel_edit();
}

/* ---- Page callbacks ---- */

static void fp_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_fp_cb);
    UART_SetSubmodelCallbacks(&s_fp_sub_cb);
    s_id_result = 0;
    s_editing = false;
    fp_refresh();
}

static void fp_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearAppCallbacks();
    UART_ClearSubmodelCallbacks();
}

static void fp_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    int16_t dt = dirty->y, db = dirty->y + dirty->h;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Left panel: list */
    if (dt < FP_LIST_Y + FP_LIST_H && db > FP_LIST_Y) {
        ui_rect_t bg = {FP_LIST_X, FP_LIST_Y, FP_LIST_W, FP_LIST_H};
        ui_draw_fill_rect(&bg, FP_LIST_BG);
        ui_draw_rect_border(&bg, FP_BORDER, 1);

        /* Header */
        ui_rect_t hdr = {FP_LIST_X, FP_LIST_Y, FP_LIST_W, FP_HDR_H};
        ui_draw_fill_rect(&hdr, UI_HEX(0xE8F5E9));
        char hdr_text[32];
        snprintf(hdr_text, sizeof(hdr_text), "Fingerprints (%d)", s_fp_count);
        ui_draw_text(FP_LIST_X + 10, FP_LIST_Y + 8, hdr_text,
                     &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

        /* Items */
        for (int16_t vis = 0; vis < FP_VISIBLE; vis++) {
            int16_t idx = s_fp_scroll + vis;
            if (idx >= s_fp_count) break;

            int16_t iy = FP_LIST_Y + FP_HDR_H + vis * FP_ITEM_H;
            if (iy + FP_ITEM_H > FP_LIST_Y + FP_LIST_H) break;

            ui_rect_t ir = {FP_LIST_X + 1, iy, FP_LIST_W - 2, FP_ITEM_H};

            if (idx == s_fp_selected)
                ui_draw_fill_rect(&ir, FP_SEL);
            else if (idx & 1)
                ui_draw_fill_rect(&ir, FP_ALT);

            /* ID badge */
            char id_text[8];
            snprintf(id_text, sizeof(id_text), "#%d", s_fp_list[idx].id);
            ui_draw_fill_round_rect(
                &(ui_rect_t){FP_LIST_X + 8, iy + 7, 40, 22}, 4, UI_HEX(0x7EC8C8));
            ui_draw_text(FP_LIST_X + 13, iy + 11, id_text,
                         &font_montserrat_12, UI_COLOR_WHITE);

            /* Name */
            ui_draw_text(FP_LIST_X + 56, iy + 11, s_fp_list[idx].name,
                         &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

            /* HW indicator */
            if (s_fp_list[idx].from_hw) {
                ui_draw_text(FP_LIST_X + FP_LIST_W - 40, iy + 11, "HW",
                             &font_montserrat_12, UI_HEX(0x4CAF50));
            }
        }

        /* Scroll indicator */
        if (s_fp_count > FP_VISIBLE) {
            int16_t sb_h = FP_LIST_H - FP_HDR_H;
            int16_t th_h = sb_h * FP_VISIBLE / (s_fp_count + 1);
            if (th_h < 20) th_h = 20;
            int16_t th_y = FP_LIST_Y + FP_HDR_H +
                (sb_h - th_h) * s_fp_scroll / (s_fp_count - FP_VISIBLE + 1);
            ui_rect_t thumb = {FP_LIST_X + FP_LIST_W - 6, th_y, 4, th_h};
            ui_draw_fill_round_rect(&thumb, 2, UI_HEX(0xBDBDBD));
        }
    }

    /* Right panel: controls */
    if (dt < UI_SCREEN_HEIGHT && db > APP_TITLE_BAR_H &&
        dirty->x + dirty->w > FP_RIGHT_X) {
        ui_rect_t ctrl = {FP_RIGHT_X, APP_TITLE_BAR_H, FP_RIGHT_W,
                          UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - 34};
        ui_draw_fill_rect(&ctrl, FP_CTRL_BG);

        /* Identify result area */
        int16_t ry = APP_TITLE_BAR_H + 12;
        if (s_id_result == 1) {
            ui_rect_t rr = {FP_RIGHT_X + 12, ry, FP_RIGHT_W - 24, 48};
            ui_draw_fill_rect(&rr, UI_HEX(0xE8F5E9));
            ui_draw_rect_border(&rr, UI_HEX(0x4CAF50), 2);
            char buf[40];
            snprintf(buf, sizeof(buf), "OK: ID #%d", s_id_fp_id);
            ui_draw_text(FP_RIGHT_X + 24, ry + 16, buf,
                         &font_montserrat_16, UI_HEX(0x2E7D32));
        } else if (s_id_result == 2) {
            ui_rect_t rr = {FP_RIGHT_X + 12, ry, FP_RIGHT_W - 24, 48};
            ui_draw_fill_rect(&rr, UI_HEX(0xFBE9E7));
            ui_draw_rect_border(&rr, UI_HEX(0xF44336), 2);
            ui_draw_text(FP_RIGHT_X + 24, ry + 16, "Identify Failed",
                         &font_montserrat_16, UI_HEX(0xC62828));
        }

        /* Action buttons */
        int16_t by = ry + 64;
        /* Row 1: Register, Delete, Refresh */
        /* (buttons drawn by widget system) */

        /* Row 2: Add, Edit (only when not editing) */
        if (!s_editing) {
            by += FP_BTN_H + FP_BTN_GAP + 20;
            ui_draw_text(FP_RIGHT_X + 12, by - 16, "Name Management",
                         &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
        }

        /* Input area (when editing) */
        if (s_editing) {
            int16_t input_y = UI_SCREEN_HEIGHT - 140;
            ui_rect_t input_bg = {FP_RIGHT_X + 12, input_y, FP_RIGHT_W - 24, 100};
            ui_draw_fill_rect(&input_bg, UI_COLOR_BG_CARD);
            ui_draw_rect_border(&input_bg, UI_COLOR_PRIMARY, 2);

            ui_draw_text(FP_RIGHT_X + 20, input_y + 8,
                         s_adding ? "Add New Fingerprint" : "Edit Fingerprint Name",
                         &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

            /* Input field */
            ui_rect_t field = {FP_RIGHT_X + 20, input_y + 32, FP_RIGHT_W - 40, 28};
            ui_draw_fill_rect(&field, UI_COLOR_WHITE);
            ui_draw_rect_border(&field, UI_HEX(0x90CAF9), 1);

            /* Draw input text with cursor */
            char display[FP_INPUT_MAX + 2];
            memcpy(display, s_input, s_input_len);
            display[s_input_len] = '|';
            display[s_input_len + 1] = '\0';
            ui_draw_text(FP_RIGHT_X + 26, input_y + 38, display,
                         &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);

            ui_draw_text(FP_RIGHT_X + 20, input_y + 68,
                         "Type with keyboard, Enter=Save, Esc=Cancel",
                         &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
        }
    }

    /* Status bar */
    if (db > UI_SCREEN_HEIGHT - 34) {
        ui_rect_t sb = {0, UI_SCREEN_HEIGHT - 34, UI_SCREEN_WIDTH, 34};
        ui_draw_fill_rect(&sb, FP_STATUS_BG);
        ui_draw_text(12, UI_SCREEN_HEIGHT - 24, s_status,
                     &font_montserrat_12, UI_HEX(0x555555));
    }
}

static bool fp_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    /* Editing mode: capture all keyboard input */
    if (s_editing) {
        if (e->type == UI_EVENT_KEY_CLICK && e->char_code >= 0x20 && e->char_code <= 0x7E) {
            if (s_input_len < FP_INPUT_MAX) {
                s_input[s_input_len++] = (char)e->char_code;
                s_input[s_input_len] = '\0';
                fp_invalidate_input();
            }
            return true;
        }
        if (e->type == UI_EVENT_KEY_CLICK && e->char_code == 0x08) {
            /* Backspace */
            if (s_input_len > 0) {
                s_input[--s_input_len] = '\0';
                fp_invalidate_input();
            }
            return true;
        }
        if (e->type == UI_EVENT_KEY_CLICK && e->key_code == 0x05 /* UI_KEY_OK */) {
            /* Enter = save */
            fp_save();
            return true;
        }
        if (e->type == UI_EVENT_KEY_CLICK && e->key_code == 0x06 /* UI_KEY_BACK */) {
            /* Esc/Back = cancel */
            fp_cancel_edit();
            return true;
        }
        return true; /* consume all keys while editing */
    }

    /* Navigation mode */
    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_fp_selected < s_fp_count - 1) {
            s_fp_selected++;
            if (s_fp_selected >= s_fp_scroll + FP_VISIBLE) s_fp_scroll++;
            fp_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_fp_selected > 0) {
            s_fp_selected--;
            if (s_fp_selected < s_fp_scroll) s_fp_scroll--;
            fp_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_OK) {
        if (s_fp_selected >= 0) {
            fp_start_edit(s_fp_list[s_fp_selected].id,
                          s_fp_list[s_fp_selected].name);
        }
        return true;
    }

    return false;
}

/* ---- Init ---- */

void app_fingerprint_init(void)
{
    ui_app_page_init(&s_app_fp, "Finger", 0x108);

    s_fp_count = 0;
    s_fp_scroll = 0;
    s_fp_selected = -1;
    s_cli_len = 0;
    s_cli_assembling = false;
    s_cli_phase = 0;
    s_editing = false;
    s_adding = false;
    s_input_len = 0;
    s_id_result = 0;
    strcpy(s_status, "Ready");

    int wi = 0;
    s_widgets[wi++] = (ui_widget_t *)&s_app_fp.btn_back;
    s_widgets[wi++] = (ui_widget_t *)&s_app_fp.lbl_title;

    /* List touch */
    {
        ui_rect_t r = {FP_LIST_X, FP_LIST_Y + FP_HDR_H, FP_LIST_W, FP_LIST_H - FP_HDR_H};
        ui_widget_init(&s_list_touch, &r);
        s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_list_touch.event_cb = list_touch_event;
    }
    s_widgets[wi++] = &s_list_touch;

    /* Right panel buttons */
    int16_t bx = FP_RIGHT_X + 16;
    int16_t by = APP_TITLE_BAR_H + 80;

    /* Register */
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_register, &r, "Register", &font_montserrat_12);
        ui_button_set_callback(&btn_register, btn_register_click);
        ui_button_set_colors(&btn_register, UI_COLOR_BG_CARD, UI_COLOR_PRIMARY, UI_COLOR_TEXT_PRIMARY);
        btn_register.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_register;

    /* Delete */
    by += FP_BTN_H + FP_BTN_GAP;
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_delete, &r, "Delete", &font_montserrat_12);
        ui_button_set_callback(&btn_delete, btn_delete_click);
        ui_button_set_colors(&btn_delete, UI_COLOR_BG_CARD, UI_COLOR_DANGER, UI_COLOR_TEXT_PRIMARY);
        btn_delete.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_delete;

    /* Refresh */
    by += FP_BTN_H + FP_BTN_GAP;
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_refresh, &r, "Refresh", &font_montserrat_12);
        ui_button_set_callback(&btn_refresh, btn_refresh_click);
        ui_button_set_colors(&btn_refresh, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
        btn_refresh.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_refresh;

    /* Add (name management) */
    by += FP_BTN_H + FP_BTN_GAP + 20;
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_add, &r, "Add Name", &font_montserrat_12);
        ui_button_set_callback(&btn_add, btn_add_click);
        ui_button_set_colors(&btn_add, UI_COLOR_BG_CARD, UI_HEX(0x4CAF50), UI_COLOR_TEXT_PRIMARY);
        btn_add.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_add;

    /* Edit */
    by += FP_BTN_H + FP_BTN_GAP;
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_edit, &r, "Edit Name", &font_montserrat_12);
        ui_button_set_callback(&btn_edit, btn_edit_click);
        ui_button_set_colors(&btn_edit, UI_COLOR_BG_CARD, UI_HEX(0xFF9800), UI_COLOR_TEXT_PRIMARY);
        btn_edit.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_edit;

    /* Save (visible when editing) */
    by += FP_BTN_H + FP_BTN_GAP;
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_save, &r, "Save", &font_montserrat_12);
        ui_button_set_callback(&btn_save, btn_save_click);
        ui_button_set_colors(&btn_save, UI_COLOR_BG_CARD, UI_COLOR_PRIMARY, UI_COLOR_TEXT_PRIMARY);
        btn_save.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_save;

    /* Cancel (visible when editing) */
    by += FP_BTN_H + FP_BTN_GAP;
    {
        ui_rect_t r = {bx, by, FP_BTN_W, FP_BTN_H};
        ui_button_init(&btn_cancel, &r, "Cancel", &font_montserrat_12);
        ui_button_set_callback(&btn_cancel, btn_cancel_click);
        ui_button_set_colors(&btn_cancel, UI_COLOR_BG_CARD, UI_HEX(0x9E9E9E), UI_COLOR_TEXT_PRIMARY);
        btn_cancel.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_cancel;

    ui_page_set_widgets(&s_app_fp.page, s_widgets, (uint16_t)wi);
    ui_page_set_callbacks(&s_app_fp.page, fp_page_enter, fp_page_exit, fp_page_draw, NULL);
    ui_page_set_event_cb(&s_app_fp.page, fp_page_event);
    ui_page_register(&s_app_fp.page);
}

ui_page_t *app_fingerprint_get_page(void)
{
    return &s_app_fp.page;
}
