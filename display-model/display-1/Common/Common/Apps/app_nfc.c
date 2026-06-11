/********************************** (C) COPYRIGHT *******************************
* File Name          : app_nfc.c
* Description        : NFC card manager app.
*                      List stored NFC cards from nfc.json.
*                      Add/edit names via keyboard input.
*                      Display real-time card detection events from Core.
********************************************************************************/
#include "app_nfc.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Layout (800x480) */
#define NFC_LEFT_W      400
#define NFC_LIST_X      8
#define NFC_LIST_Y      (APP_TITLE_BAR_H + 56)  /* below detect area */
#define NFC_LIST_W      (NFC_LEFT_W - 16)
#define NFC_LIST_H      (UI_SCREEN_HEIGHT - NFC_LIST_Y - 40)
#define NFC_ITEM_H      36
#define NFC_HDR_H       30
#define NFC_VISIBLE     ((NFC_LIST_H - NFC_HDR_H) / NFC_ITEM_H)

#define NFC_RIGHT_X     NFC_LEFT_W
#define NFC_RIGHT_W     (UI_SCREEN_WIDTH - NFC_LEFT_W)
#define NFC_BTN_W       120
#define NFC_BTN_H       34
#define NFC_BTN_GAP     10

/* Detect display area (top of left panel) */
#define NFC_DETECT_Y    (APP_TITLE_BAR_H + 4)
#define NFC_DETECT_H    48

/* Colors */
#define NFC_BG          UI_COLOR_BG_MAIN
#define NFC_LIST_BG     UI_COLOR_BG_CARD
#define NFC_SEL         UI_HEX(0xE3F2FD)
#define NFC_ALT         UI_HEX(0xFAFAFA)
#define NFC_BORDER      UI_HEX(0xE0E0E0)
#define NFC_CTRL_BG     UI_HEX(0xF0F0F0)
#define NFC_STATUS_BG   UI_HEX(0xE8E8E8)

/* State */
#define NFC_MAX_ENTRIES 32
#define NFC_CLI_BUF_SIZE 2048
#define NFC_NAME_MAX    20
#define NFC_INPUT_MAX   24
#define NFC_HEX_ID_LEN  10  /* 5 bytes = 10 hex chars */

typedef struct {
    char hex_id[NFC_HEX_ID_LEN + 1];
    char name[NFC_NAME_MAX + 1];
} nfc_entry_t;

static ui_app_page_t s_app_nfc;

/* Card list */
static nfc_entry_t s_nfc_list[NFC_MAX_ENTRIES];
static uint8_t     s_nfc_count;
static int16_t     s_nfc_scroll;
static int16_t     s_nfc_selected;

/* CLI response */
static char     s_cli_buf[NFC_CLI_BUF_SIZE];
static uint16_t s_cli_len;
static bool     s_cli_assembling;

/* Editing state */
static bool     s_editing;
static bool     s_adding;
static char     s_input[NFC_INPUT_MAX + 1];
static uint8_t  s_input_len;
static char     s_edit_hex_id[NFC_HEX_ID_LEN + 1]; /* hex ID being edited */

/* Status */
static char s_status[80];

/* Detection result */
static uint8_t s_detect_card_id;
static char    s_detect_hex[NFC_HEX_ID_LEN + 1];
static uint8_t s_detect_active;  /* 1 = card detected, 0 = no card */
static uint32_t s_detect_tick;

/* Widgets */
static ui_widget_t  s_list_touch;
static ui_button_t  btn_refresh, btn_add, btn_edit, btn_save, btn_cancel;
static ui_widget_t *s_widgets[2 + 1 + 5]; /* back + title + touch + 5 buttons */

/* Forward declarations */
static void nfc_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last);
static void nfc_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                   const uint8_t *evt_data, uint8_t evt_len);
static void nfc_set_status(const char *msg);
static void nfc_invalidate_list(void);
static void nfc_invalidate_status(void);
static void nfc_invalidate_detect(void);
static void nfc_invalidate_input(void);

static uart_app_callbacks_t s_nfc_cb = {
    .on_cli_response = nfc_on_cli_response,
    .on_file_list = NULL,
    .on_cwd_notify = NULL,
};
static uart_submodel_cb_t s_nfc_sub_cb = {
    .on_submodel_event = nfc_on_submodel_event,
};

/* ---- Helpers ---- */

static void bytes_to_hex(const uint8_t *bytes, uint8_t len, char *out)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    for (uint8_t i = 0; i < len; i++) {
        out[i * 2]     = hex_chars[(bytes[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[bytes[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static void nfc_set_status(const char *msg)
{
    strncpy(s_status, msg, sizeof(s_status) - 1);
    s_status[sizeof(s_status) - 1] = '\0';
    nfc_invalidate_status();
}

static void nfc_invalidate_list(void)
{
    ui_rect_t r = {NFC_LIST_X, NFC_LIST_Y, NFC_LIST_W, NFC_LIST_H};
    ui_page_invalidate(&r);
}

static void nfc_invalidate_status(void)
{
    ui_rect_t r = {0, UI_SCREEN_HEIGHT - 34, UI_SCREEN_WIDTH, 34};
    ui_page_invalidate(&r);
}

static void nfc_invalidate_detect(void)
{
    ui_rect_t r = {NFC_LIST_X, NFC_DETECT_Y, NFC_LIST_W, NFC_DETECT_H};
    ui_page_invalidate(&r);
}

static void nfc_invalidate_input(void)
{
    ui_rect_t r = {NFC_RIGHT_X + 12, UI_SCREEN_HEIGHT - 140, NFC_RIGHT_W - 24, 100};
    ui_page_invalidate(&r);
}

static void nfc_start_edit(const char *hex_id, const char *existing_name)
{
    s_editing = true;
    s_adding = false;
    strncpy(s_edit_hex_id, hex_id, NFC_HEX_ID_LEN);
    s_edit_hex_id[NFC_HEX_ID_LEN] = '\0';
    if (existing_name) {
        strncpy(s_input, existing_name, NFC_INPUT_MAX);
        s_input[NFC_INPUT_MAX] = '\0';
    } else {
        s_input[0] = '\0';
    }
    s_input_len = (uint8_t)strlen(s_input);
    nfc_invalidate_input();
    nfc_set_status("Type name, Enter to save, Esc to cancel");
}

static void nfc_start_add(void)
{
    s_editing = true;
    s_adding = true;
    s_edit_hex_id[0] = '\0';
    s_input[0] = '\0';
    s_input_len = 0;
    nfc_invalidate_input();
    nfc_set_status("Enter hex ID (10 chars) then name, Tab to switch, Enter to save");
}

static void nfc_cancel(void)
{
    s_editing = false;
    s_adding = false;
    nfc_invalidate_input();
    nfc_set_status("Cancelled");
}

static void nfc_save(void)
{
    if (s_input_len == 0) {
        nfc_set_status("Name cannot be empty");
        return;
    }

    char cmd[80];
    if (s_adding) {
        /* For add mode: user enters hex_id in s_edit_hex_id */
        if (strlen(s_edit_hex_id) != NFC_HEX_ID_LEN) {
            nfc_set_status("Hex ID must be 10 characters");
            return;
        }
        snprintf(cmd, sizeof(cmd), "nfc set %s %s", s_edit_hex_id, s_input);
    } else {
        snprintf(cmd, sizeof(cmd), "nfc set %s %s", s_edit_hex_id, s_input);
    }
    UART_SendCLI(cmd);

    s_editing = false;
    s_adding = false;
    nfc_invalidate_input();
    nfc_set_status("Saved, refreshing...");
    UART_SendCLI("nfc ls");
}

static void nfc_refresh(void)
{
    s_nfc_count = 0;
    s_nfc_scroll = 0;
    s_nfc_selected = -1;
    nfc_set_status("Loading NFC cards...");
    UART_SendCLI("nfc ls");
}

/* ---- Submodel event handler ---- */

static void nfc_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                   const uint8_t *evt_data, uint8_t evt_len)
{
    if (sub_type != SUBMODEL_NFC) return;

    if (sub_cmd == NFC_EVT_CARD_DETECT && evt_data && evt_len >= 6) {
        s_detect_card_id = evt_data[0];
        bytes_to_hex(&evt_data[1], 5, s_detect_hex);
        s_detect_active = 1;
        s_detect_tick = 0;

        /* Look up name */
        char *name = NULL;
        for (uint8_t i = 0; i < s_nfc_count; i++) {
            if (strcmp(s_nfc_list[i].hex_id, s_detect_hex) == 0) {
                name = s_nfc_list[i].name;
                break;
            }
        }

        char msg[80];
        if (name) {
            snprintf(msg, sizeof(msg), "Card #%d: %s (%s)", s_detect_card_id, name, s_detect_hex);
        } else {
            snprintf(msg, sizeof(msg), "Card #%d: %s (unknown)", s_detect_card_id, s_detect_hex);
        }
        nfc_set_status(msg);
        nfc_invalidate_detect();
    }
}

/* ---- CLI response parsing ---- */

static void nfc_parse_ls(const char *text, uint16_t len)
{
    /* "nfc ls" output: "0A1A3BAC20: Sratle\n..." */
    const char *p = text;
    const char *end = text + len;
    s_nfc_count = 0;

    while (p < end && s_nfc_count < NFC_MAX_ENTRIES) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n') eol++;

        uint16_t line_len = (uint16_t)(eol - p);
        if (line_len > 0) {
            char line[128];
            uint16_t cpy = (line_len < 127) ? line_len : 127;
            memcpy(line, p, cpy);
            line[cpy] = '\0';

            const char *colon = strchr(line, ':');
            if (colon) {
                uint16_t hex_len = (uint16_t)(colon - line);
                if (hex_len == NFC_HEX_ID_LEN) {
                    memcpy(s_nfc_list[s_nfc_count].hex_id, line, NFC_HEX_ID_LEN);
                    s_nfc_list[s_nfc_count].hex_id[NFC_HEX_ID_LEN] = '\0';

                    const char *name = colon + 1;
                    while (*name == ' ') name++;
                    strncpy(s_nfc_list[s_nfc_count].name, name, NFC_NAME_MAX);
                    s_nfc_list[s_nfc_count].name[NFC_NAME_MAX] = '\0';
                    /* Trim trailing whitespace */
                    int nlen = (int)strlen(s_nfc_list[s_nfc_count].name) - 1;
                    while (nlen >= 0 && (s_nfc_list[s_nfc_count].name[nlen] == ' ' ||
                           s_nfc_list[s_nfc_count].name[nlen] == '\r' ||
                           s_nfc_list[s_nfc_count].name[nlen] == '\n'))
                        s_nfc_list[s_nfc_count].name[nlen--] = '\0';

                    s_nfc_count++;
                }
            }
        }

        p = eol;
        while (p < end && (*p == '\r' || *p == '\n')) p++;
    }

    char msg[48];
    snprintf(msg, sizeof(msg), "%d NFC card(s)", s_nfc_count);
    nfc_set_status(msg);
    nfc_invalidate_list();
}

static void nfc_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    if (!s_cli_assembling) {
        s_cli_len = 0;
        s_cli_assembling = true;
    }

    uint16_t space = NFC_CLI_BUF_SIZE - 1 - s_cli_len;
    uint16_t copy_len = (len < space) ? len : space;
    if (copy_len > 0) {
        memcpy(&s_cli_buf[s_cli_len], output, copy_len);
        s_cli_len += copy_len;
    }
    s_cli_buf[s_cli_len] = '\0';

    if (!is_last) return;
    s_cli_assembling = false;

    /* Parse "nfc ls" response or show ad-hoc response */
    if (strstr(s_cli_buf, ":") != NULL && s_cli_len > 12) {
        /* Likely a listing with hex_id: name format */
        nfc_parse_ls(s_cli_buf, s_cli_len);
    } else {
        /* Show as status */
        char msg[78];
        uint16_t slen = s_cli_len;
        if (slen > 77) slen = 77;
        memcpy(msg, s_cli_buf, slen);
        msg[slen] = '\0';
        for (uint16_t i = 0; i < slen; i++) {
            if (msg[i] == '\r' || msg[i] == '\n') msg[i] = ' ';
        }
        nfc_set_status(msg);
    }
}

/* ---- List touch ---- */

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int16_t rel_y = e->pos.y - (NFC_LIST_Y + NFC_HDR_H) + s_nfc_scroll * NFC_ITEM_H;
        int16_t idx = rel_y / NFC_ITEM_H;
        if (idx >= 0 && idx < s_nfc_count) {
            s_nfc_selected = idx;
            nfc_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_nfc_scroll + NFC_VISIBLE < s_nfc_count) {
            s_nfc_scroll++;
            nfc_invalidate_list();
        }
    }
    else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_nfc_scroll > 0) {
            s_nfc_scroll--;
            nfc_invalidate_list();
        }
    }
}

/* ---- Button callbacks ---- */

static void btn_refresh_click(ui_widget_t *w) { (void)w; nfc_refresh(); }
static void btn_add_click(ui_widget_t *w) { (void)w; nfc_start_add(); }

static void btn_edit_click(ui_widget_t *w)
{
    (void)w;
    if (s_nfc_selected < 0 || s_nfc_selected >= s_nfc_count) {
        nfc_set_status("Select a card first");
        return;
    }
    nfc_start_edit(s_nfc_list[s_nfc_selected].hex_id,
                   s_nfc_list[s_nfc_selected].name);
}

static void btn_save_click(ui_widget_t *w) { (void)w; nfc_save(); }
static void btn_cancel_click(ui_widget_t *w) { (void)w; nfc_cancel(); }

/* ---- Page callbacks ---- */

static void nfc_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_nfc_cb);
    UART_SetSubmodelCallbacks(&s_nfc_sub_cb);
    s_detect_active = 0;
    s_editing = false;
    nfc_refresh();
}

static void nfc_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearAppCallbacks();
    UART_ClearSubmodelCallbacks();
}

static void nfc_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    int16_t dt = dirty->y, db = dirty->y + dirty->h;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Detection area */
    if (dt < NFC_DETECT_Y + NFC_DETECT_H && db > NFC_DETECT_Y) {
        ui_rect_t dr = {NFC_LIST_X, NFC_DETECT_Y, NFC_LIST_W, NFC_DETECT_H};
        if (s_detect_active) {
            ui_draw_fill_rect(&dr, UI_HEX(0xE3F2FD));
            ui_draw_rect_border(&dr, UI_HEX(0x2196F3), 2);

            char line1[48];
            snprintf(line1, sizeof(line1), "Card #%d detected", s_detect_card_id);
            ui_draw_text(NFC_LIST_X + 12, NFC_DETECT_Y + 6, line1,
                         &font_montserrat_16, UI_HEX(0x1565C0));

            /* Look up name */
            char *name = NULL;
            for (uint8_t i = 0; i < s_nfc_count; i++) {
                if (strcmp(s_nfc_list[i].hex_id, s_detect_hex) == 0) {
                    name = s_nfc_list[i].name;
                    break;
                }
            }

            char line2[60];
            if (name)
                snprintf(line2, sizeof(line2), "%s  (%s)", name, s_detect_hex);
            else
                snprintf(line2, sizeof(line2), "%s", s_detect_hex);
            ui_draw_text(NFC_LIST_X + 12, NFC_DETECT_Y + 26, line2,
                         &font_montserrat_12, UI_HEX(0x1976D2));
        } else {
            ui_draw_fill_rect(&dr, NFC_LIST_BG);
            ui_draw_rect_border(&dr, NFC_BORDER, 1);
            ui_draw_text(NFC_LIST_X + 12, NFC_DETECT_Y + 16,
                         "Waiting for NFC card...",
                         &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
        }
    }

    /* Card list */
    if (dt < NFC_LIST_Y + NFC_LIST_H && db > NFC_LIST_Y) {
        ui_rect_t bg = {NFC_LIST_X, NFC_LIST_Y, NFC_LIST_W, NFC_LIST_H};
        ui_draw_fill_rect(&bg, NFC_LIST_BG);
        ui_draw_rect_border(&bg, NFC_BORDER, 1);

        /* Header */
        ui_rect_t hdr = {NFC_LIST_X, NFC_LIST_Y, NFC_LIST_W, NFC_HDR_H};
        ui_draw_fill_rect(&hdr, UI_HEX(0xE3F2FD));
        char hdr_text[32];
        snprintf(hdr_text, sizeof(hdr_text), "NFC Cards (%d)", s_nfc_count);
        ui_draw_text(NFC_LIST_X + 10, NFC_LIST_Y + 8, hdr_text,
                     &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

        /* Items */
        for (int16_t vis = 0; vis < NFC_VISIBLE; vis++) {
            int16_t idx = s_nfc_scroll + vis;
            if (idx >= s_nfc_count) break;

            int16_t iy = NFC_LIST_Y + NFC_HDR_H + vis * NFC_ITEM_H;
            if (iy + NFC_ITEM_H > NFC_LIST_Y + NFC_LIST_H) break;

            ui_rect_t ir = {NFC_LIST_X + 1, iy, NFC_LIST_W - 2, NFC_ITEM_H};
            if (idx == s_nfc_selected)
                ui_draw_fill_rect(&ir, NFC_SEL);
            else if (idx & 1)
                ui_draw_fill_rect(&ir, NFC_ALT);

            /* Hex ID badge */
            ui_draw_fill_round_rect(
                &(ui_rect_t){NFC_LIST_X + 6, iy + 7, 90, 22}, 4, UI_HEX(0x90CAF9));
            ui_draw_text(NFC_LIST_X + 10, iy + 11, s_nfc_list[idx].hex_id,
                         &font_montserrat_12, UI_COLOR_WHITE);

            /* Name */
            ui_draw_text(NFC_LIST_X + 104, iy + 11, s_nfc_list[idx].name,
                         &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
        }

        /* Scroll indicator */
        if (s_nfc_count > NFC_VISIBLE) {
            int16_t sb_h = NFC_LIST_H - NFC_HDR_H;
            int16_t th_h = sb_h * NFC_VISIBLE / (s_nfc_count + 1);
            if (th_h < 20) th_h = 20;
            int16_t th_y = NFC_LIST_Y + NFC_HDR_H +
                (sb_h - th_h) * s_nfc_scroll / (s_nfc_count - NFC_VISIBLE + 1);
            ui_rect_t thumb = {NFC_LIST_X + NFC_LIST_W - 6, th_y, 4, th_h};
            ui_draw_fill_round_rect(&thumb, 2, UI_HEX(0xBDBDBD));
        }
    }

    /* Right panel */
    if (dt < UI_SCREEN_HEIGHT && db > APP_TITLE_BAR_H &&
        dirty->x + dirty->w > NFC_RIGHT_X) {
        ui_rect_t ctrl = {NFC_RIGHT_X, APP_TITLE_BAR_H, NFC_RIGHT_W,
                          UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - 34};
        ui_draw_fill_rect(&ctrl, NFC_CTRL_BG);

        /* Input area (when editing) */
        if (s_editing) {
            int16_t input_y = UI_SCREEN_HEIGHT - 180;
            ui_rect_t input_bg = {NFC_RIGHT_X + 12, input_y, NFC_RIGHT_W - 24, 140};
            ui_draw_fill_rect(&input_bg, UI_COLOR_BG_CARD);
            ui_draw_rect_border(&input_bg, UI_COLOR_PRIMARY, 2);

            ui_draw_text(NFC_RIGHT_X + 20, input_y + 8,
                         s_adding ? "Add New NFC Card" : "Edit Card Name",
                         &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

            if (s_adding) {
                /* Hex ID input */
                ui_draw_text(NFC_RIGHT_X + 20, input_y + 28, "Hex ID (10 chars):",
                             &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
                ui_rect_t field1 = {NFC_RIGHT_X + 20, input_y + 44, NFC_RIGHT_W - 40, 26};
                ui_draw_fill_rect(&field1, UI_COLOR_WHITE);
                ui_draw_rect_border(&field1, NFC_BORDER, 1);
                char hex_disp[NFC_HEX_ID_LEN + 2];
                memcpy(hex_disp, s_edit_hex_id, strlen(s_edit_hex_id));
                hex_disp[strlen(s_edit_hex_id)] = '|';
                hex_disp[strlen(s_edit_hex_id) + 1] = '\0';
                ui_draw_text(NFC_RIGHT_X + 26, input_y + 49, hex_disp,
                             &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

                /* Name input */
                ui_draw_text(NFC_RIGHT_X + 20, input_y + 78, "Name:",
                             &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
                ui_rect_t field2 = {NFC_RIGHT_X + 20, input_y + 94, NFC_RIGHT_W - 40, 26};
                ui_draw_fill_rect(&field2, UI_COLOR_WHITE);
                ui_draw_rect_border(&field2, NFC_BORDER, 1);
                char name_disp[NFC_INPUT_MAX + 2];
                memcpy(name_disp, s_input, s_input_len);
                name_disp[s_input_len] = '|';
                name_disp[s_input_len + 1] = '\0';
                ui_draw_text(NFC_RIGHT_X + 26, input_y + 99, name_disp,
                             &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
            } else {
                /* Name input only */
                ui_draw_text(NFC_RIGHT_X + 20, input_y + 28, "Name:",
                             &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
                ui_rect_t field = {NFC_RIGHT_X + 20, input_y + 44, NFC_RIGHT_W - 40, 26};
                ui_draw_fill_rect(&field, UI_COLOR_WHITE);
                ui_draw_rect_border(&field, NFC_BORDER, 1);
                char disp[NFC_INPUT_MAX + 2];
                memcpy(disp, s_input, s_input_len);
                disp[s_input_len] = '|';
                disp[s_input_len + 1] = '\0';
                ui_draw_text(NFC_RIGHT_X + 26, input_y + 49, disp,
                             &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
            }

            ui_draw_text(NFC_RIGHT_X + 20, input_y + 124,
                         "Enter=Save  Esc=Cancel",
                         &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
        }
    }

    /* Status bar */
    if (db > UI_SCREEN_HEIGHT - 34) {
        ui_rect_t sb = {0, UI_SCREEN_HEIGHT - 34, UI_SCREEN_WIDTH, 34};
        ui_draw_fill_rect(&sb, NFC_STATUS_BG);
        ui_draw_text(12, UI_SCREEN_HEIGHT - 24, s_status,
                     &font_montserrat_12, UI_HEX(0x555555));
    }
}

static bool nfc_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    if (s_editing) {
        if (e->type == UI_EVENT_KEY_CLICK && e->char_code >= 0x20 && e->char_code <= 0x7E) {
            if (s_adding && strlen(s_edit_hex_id) < NFC_HEX_ID_LEN) {
                /* Still entering hex ID */
                char c = (char)e->char_code;
                /* Validate hex character */
                if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
                    uint8_t hlen = (uint8_t)strlen(s_edit_hex_id);
                    /* Auto-uppercase */
                    if (c >= 'a' && c <= 'f') c -= 32;
                    s_edit_hex_id[hlen] = c;
                    s_edit_hex_id[hlen + 1] = '\0';
                    nfc_invalidate_input();
                }
            } else if (s_input_len < NFC_INPUT_MAX) {
                s_input[s_input_len++] = (char)e->char_code;
                s_input[s_input_len] = '\0';
                nfc_invalidate_input();
            }
            return true;
        }
        if (e->type == UI_EVENT_KEY_CLICK && e->char_code == 0x08) {
            /* Backspace */
            if (s_input_len > 0) {
                s_input[--s_input_len] = '\0';
            } else if (s_adding && strlen(s_edit_hex_id) > 0) {
                s_edit_hex_id[strlen(s_edit_hex_id) - 1] = '\0';
            }
            nfc_invalidate_input();
            return true;
        }
        if (e->type == UI_EVENT_KEY_CLICK && e->key_code == 0x05) {
            nfc_save();
            return true;
        }
        if (e->type == UI_EVENT_KEY_CLICK && e->key_code == 0x06) {
            nfc_cancel();
            return true;
        }
        return true;
    }

    /* Navigation */
    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_nfc_selected < s_nfc_count - 1) {
            s_nfc_selected++;
            if (s_nfc_selected >= s_nfc_scroll + NFC_VISIBLE) s_nfc_scroll++;
            nfc_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_nfc_selected > 0) {
            s_nfc_selected--;
            if (s_nfc_selected < s_nfc_scroll) s_nfc_scroll--;
            nfc_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_OK) {
        if (s_nfc_selected >= 0) {
            nfc_start_edit(s_nfc_list[s_nfc_selected].hex_id,
                           s_nfc_list[s_nfc_selected].name);
        }
        return true;
    }
    return false;
}

/* ---- Init ---- */

void app_nfc_init(void)
{
    ui_app_page_init(&s_app_nfc, "NFC", 0x107);

    s_nfc_count = 0;
    s_nfc_scroll = 0;
    s_nfc_selected = -1;
    s_cli_len = 0;
    s_cli_assembling = false;
    s_editing = false;
    s_adding = false;
    s_input_len = 0;
    s_detect_active = 0;
    s_detect_card_id = 0;
    s_detect_hex[0] = '\0';
    strcpy(s_status, "Ready");

    int wi = 0;
    s_widgets[wi++] = (ui_widget_t *)&s_app_nfc.btn_back;
    s_widgets[wi++] = (ui_widget_t *)&s_app_nfc.lbl_title;

    /* List touch */
    {
        ui_rect_t r = {NFC_LIST_X, NFC_LIST_Y + NFC_HDR_H, NFC_LIST_W, NFC_LIST_H - NFC_HDR_H};
        ui_widget_init(&s_list_touch, &r);
        s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_list_touch.event_cb = list_touch_event;
    }
    s_widgets[wi++] = &s_list_touch;

    /* Right panel buttons */
    int16_t bx = NFC_RIGHT_X + 16;
    int16_t by = APP_TITLE_BAR_H + 16;

    /* Refresh */
    {
        ui_rect_t r = {bx, by, NFC_BTN_W, NFC_BTN_H};
        ui_button_init(&btn_refresh, &r, "Refresh", &font_montserrat_12);
        ui_button_set_callback(&btn_refresh, btn_refresh_click);
        ui_button_set_colors(&btn_refresh, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
        btn_refresh.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_refresh;

    by += NFC_BTN_H + NFC_BTN_GAP + 12;
    /* Add */
    {
        ui_rect_t r = {bx, by, NFC_BTN_W, NFC_BTN_H};
        ui_button_init(&btn_add, &r, "Add Card", &font_montserrat_12);
        ui_button_set_callback(&btn_add, btn_add_click);
        ui_button_set_colors(&btn_add, UI_COLOR_BG_CARD, UI_HEX(0x4CAF50), UI_COLOR_TEXT_PRIMARY);
        btn_add.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_add;

    /* Edit */
    by += NFC_BTN_H + NFC_BTN_GAP;
    {
        ui_rect_t r = {bx, by, NFC_BTN_W, NFC_BTN_H};
        ui_button_init(&btn_edit, &r, "Edit Name", &font_montserrat_12);
        ui_button_set_callback(&btn_edit, btn_edit_click);
        ui_button_set_colors(&btn_edit, UI_COLOR_BG_CARD, UI_HEX(0xFF9800), UI_COLOR_TEXT_PRIMARY);
        btn_edit.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_edit;

    /* Save */
    by += NFC_BTN_H + NFC_BTN_GAP;
    {
        ui_rect_t r = {bx, by, NFC_BTN_W, NFC_BTN_H};
        ui_button_init(&btn_save, &r, "Save", &font_montserrat_12);
        ui_button_set_callback(&btn_save, btn_save_click);
        ui_button_set_colors(&btn_save, UI_COLOR_BG_CARD, UI_COLOR_PRIMARY, UI_COLOR_TEXT_PRIMARY);
        btn_save.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_save;

    /* Cancel */
    by += NFC_BTN_H + NFC_BTN_GAP;
    {
        ui_rect_t r = {bx, by, NFC_BTN_W, NFC_BTN_H};
        ui_button_init(&btn_cancel, &r, "Cancel", &font_montserrat_12);
        ui_button_set_callback(&btn_cancel, btn_cancel_click);
        ui_button_set_colors(&btn_cancel, UI_COLOR_BG_CARD, UI_HEX(0x9E9E9E), UI_COLOR_TEXT_PRIMARY);
        btn_cancel.radius = 8;
    }
    s_widgets[wi++] = (ui_widget_t *)&btn_cancel;

    ui_page_set_widgets(&s_app_nfc.page, s_widgets, (uint16_t)wi);
    ui_page_set_callbacks(&s_app_nfc.page, nfc_page_enter, nfc_page_exit, nfc_page_draw, NULL);
    ui_page_set_event_cb(&s_app_nfc.page, nfc_page_event);
    ui_page_register(&s_app_nfc.page);
}

ui_page_t *app_nfc_get_page(void)
{
    return &s_app_nfc.page;
}
