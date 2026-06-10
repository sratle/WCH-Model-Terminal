/********************************** (C) COPYRIGHT *******************************
* File Name          : app_fingerprint.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : Fingerprint manager app (V2.0).
*                      Query/register/delete fingerprints via CLI passthrough.
*                      LED control (func/color/speed), security level.
*                      Scrollable list, touch + keyboard navigation.
********************************************************************************/
#include "app_fingerprint.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration (800x480)
 *=============================================================================*/

#define FP_LEFT_W           370
#define FP_LIST_X           8
#define FP_LIST_Y           (FP_RESULT_Y + FP_RESULT_H + 4)
#define FP_LIST_W           (FP_LEFT_W - 16)
#define FP_LIST_H           (UI_SCREEN_HEIGHT - FP_LIST_Y - 36)
#define FP_ITEM_H           34
#define FP_VISIBLE_ITEMS    (FP_LIST_H / FP_ITEM_H)

#define FP_CTRL_X           FP_LEFT_W
#define FP_CTRL_W           (UI_SCREEN_WIDTH - FP_LEFT_W)

/* Action buttons row */
#define FP_ACT_Y            (APP_TITLE_BAR_H + 12)
#define FP_ACT_BTN_W        116
#define FP_ACT_BTN_H        34
#define FP_ACT_GAP          8

/* LED section */
#define FP_LED_Y            (FP_ACT_Y + FP_ACT_BTN_H + 20)
#define FP_LED_LABEL_H      18
#define FP_FUNC_BTN_W       88
#define FP_FUNC_BTN_H       28
#define FP_FUNC_GAP         6
#define FP_COLOR_BTN_SIZE   30
#define FP_COLOR_GAP        6
#define FP_COLOR_Y          (FP_LED_Y + FP_LED_LABEL_H + FP_FUNC_BTN_H + 10)
#define FP_SPEED_Y          (FP_COLOR_Y + FP_COLOR_BTN_SIZE + 14)
#define FP_SPEED_W          (FP_CTRL_W - 40)
#define FP_SPEED_H          18

/* Security section */
#define FP_SEC_Y            (FP_SPEED_Y + FP_SPEED_H + 24)
#define FP_SEC_BTN_W        56
#define FP_SEC_BTN_H        30
#define FP_SEC_GAP          8

/* Status bar */
#define FP_STATUS_Y         (UI_SCREEN_HEIGHT - 30)
#define FP_STATUS_H         30

/* Identification result display area (top of left panel) */
#define FP_RESULT_Y         (APP_TITLE_BAR_H + 4)
#define FP_RESULT_H         60

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define FP_BG               UI_COLOR_BG_MAIN
#define FP_LIST_BG          UI_COLOR_BG_CARD
#define FP_LIST_SEL         UI_HEX(0xE0F2F1)
#define FP_LIST_ALT         UI_HEX(0xFAFAFA)
#define FP_ITEM_TEXT        UI_COLOR_TEXT_PRIMARY
#define FP_ITEM_ID          UI_COLOR_TEXT_SECONDARY
#define FP_CTRL_BG          UI_HEX(0xF0F0F0)
#define FP_STATUS_BG        UI_HEX(0xE8E8E8)
#define FP_STATUS_TEXT      UI_HEX(0x555555)
#define FP_SECTION_LABEL    UI_COLOR_TEXT_SECONDARY
#define FP_BORDER           UI_HEX(0xE0E0E0)

/* LED color swatches (matching protocol colors) */
static const ui_color_t s_led_colors[] = {
    UI_HEX(0x404040),   /* 0=off   */
    UI_HEX(0x2196F3),   /* 1=blue  */
    UI_HEX(0x4CAF50),   /* 2=green */
    UI_HEX(0x00BCD4),   /* 3=cyan  */
    UI_HEX(0xF44336),   /* 4=red   */
    UI_HEX(0xE91E63),   /* 5=magenta */
    UI_HEX(0xFFEB3B),   /* 6=yellow */
    UI_HEX(0xFFFFFF),   /* 7=white */
};
#define FP_NUM_COLORS   (sizeof(s_led_colors) / sizeof(s_led_colors[0]))

static const char *s_led_func_names[] = {
    "Breath", "Flicker", "On", "Off", "Grad On", "Grad Off", "Horse"
};
#define FP_NUM_FUNCS    7

/*=============================================================================
 *  State
 *=============================================================================*/

#define FP_MAX_ENTRIES      32
#define FP_CLI_BUF_SIZE     2048

typedef struct {
    uint8_t id;
    char    name[20];
} fp_entry_t;

static ui_app_page_t s_app_fp;

/* Fingerprint list */
static fp_entry_t s_fp_list[FP_MAX_ENTRIES];
static uint8_t    s_fp_count;
static int16_t    s_fp_scroll;
static int16_t    s_fp_selected;      /* -1 = none */

/* CLI response assembly buffer */
static char    s_cli_buf[FP_CLI_BUF_SIZE];
static uint16_t s_cli_len;
static bool    s_cli_assembling;      /* multi-frame in progress */

/* LED config */
static uint8_t s_led_func;            /* 1-7 */
static uint8_t s_led_color;           /* 0-7 */
static uint8_t s_led_speed;           /* 0-255 */

/* Security level */
static uint8_t s_sec_level;           /* 1-3 */

/* Status message */
static char s_status[80];

/* Identification result state */
typedef struct {
    uint8_t result;      /* 0=none, 1=success, 2=fail */
    uint8_t fp_id;       /* Fingerprint ID on success */
    uint32_t timestamp;  /* Tick when result arrived (for auto-clear) */
} fp_identify_result_t;

static fp_identify_result_t s_id_result;

/* Widgets */
static ui_widget_t  s_list_touch;
static ui_button_t  btn_register, btn_delete, btn_refresh;
static ui_button_t  btn_func[FP_NUM_FUNCS];
static ui_widget_t  s_color_btns[FP_NUM_COLORS];
static ui_slider_t  s_speed_slider;
static ui_button_t  btn_sec[3];
static ui_widget_t *s_fp_widgets[2 + 3 + FP_NUM_FUNCS + FP_NUM_COLORS + 1 + 3];
/* = 2 + 3 + 7 + 8 + 1 + 3 = 24 (back_btn + title + app widgets) */

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void fp_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last);
static void fp_parse_list(const char *text, uint16_t len);
static void fp_set_status(const char *msg);
static void fp_invalidate_list(void);
static void fp_invalidate_status(void);
static void fp_refresh_list(void);

static uart_app_callbacks_t s_fp_callbacks = {
    .on_cli_response = fp_on_cli_response,
    .on_file_list    = NULL,
    .on_cwd_notify   = NULL,
};

static void fp_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                  const uint8_t *evt_data, uint8_t evt_len);

static uart_submodel_cb_t s_fp_submodel_cb = {
    .on_submodel_event = fp_on_submodel_event,
};

/*=============================================================================
 *  Helpers
 *=============================================================================*/

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
    ui_rect_t r = {0, FP_STATUS_Y, UI_SCREEN_WIDTH, FP_STATUS_H};
    ui_page_invalidate(&r);
}

static void fp_invalidate_result(void)
{
    ui_rect_t r = {0, FP_RESULT_Y, FP_LEFT_W, FP_RESULT_H};
    ui_page_invalidate(&r);
}

static void fp_invalidate_func_btns(void)
{
    for (int i = 0; i < FP_NUM_FUNCS; i++)
        ui_widget_invalidate((ui_widget_t *)&btn_func[i]);
}

static void fp_invalidate_color_btns(void)
{
    for (int i = 0; i < (int)FP_NUM_COLORS; i++)
        ui_widget_invalidate(&s_color_btns[i]);
}

static void fp_invalidate_sec_btns(void)
{
    for (int i = 0; i < 3; i++)
        ui_widget_invalidate((ui_widget_t *)&btn_sec[i]);
}

static void fp_clear_list(void)
{
    s_fp_count = 0;
    s_fp_scroll = 0;
    s_fp_selected = -1;
    fp_invalidate_list();
}

static void fp_refresh_list(void)
{
    fp_clear_list();
    fp_set_status("Querying fingerprint list...");
    UART_SendCLI("fp ls");
}

/*=============================================================================
 *  Submodel Event Handler (fingerprint identify results)
 *=============================================================================*/

static void fp_on_submodel_event(uint8_t sub_type, uint8_t sub_cmd,
                                  const uint8_t *evt_data, uint8_t evt_len)
{
    if (sub_type != SUBMODEL_FINGERPRINT)
        return;

    switch (sub_cmd) {
    case FP_EVT_IDENTIFY_OK:
    {
        /* Core sends evt_data = [FP_SUB_IDENTIFY_OK, fp_id], so fp_id is at [1] */
        uint8_t fp_id = (evt_data && evt_len >= 2) ? evt_data[1] : 0;
        s_id_result.result = 1;  /* success */
        s_id_result.fp_id = fp_id;
        s_id_result.timestamp = 0;  /* reserved for future auto-clear */
        fp_invalidate_result();
        char msg[48];
        snprintf(msg, sizeof(msg), "Identify OK: ID #%d", fp_id);
        fp_set_status(msg);
        break;
    }
    case FP_EVT_IDENTIFY_FAIL:
        s_id_result.result = 2;  /* fail */
        s_id_result.fp_id = 0;
        s_id_result.timestamp = 0;
        fp_invalidate_result();
        fp_set_status("Identify FAIL: no match");
        break;
    default:
        break;
    }
}

/*=============================================================================
 *  CLI Response Parsing
 *=============================================================================*/

static void fp_on_cli_response(const char *output, uint16_t len, bool truncated, bool is_last)
{
    /* Accumulate multi-frame responses */
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

    if (!is_last)
        return;  /* More frames expected */

    s_cli_assembling = false;

    /* Determine what command this response is for based on content */
    if (strstr(s_cli_buf, "fp:") != NULL || strstr(s_cli_buf, "Fingerprint") != NULL ||
        strstr(s_cli_buf, "ID") != NULL || strstr(s_cli_buf, "enroll") != NULL ||
        strstr(s_cli_buf, "delete") != NULL || strstr(s_cli_buf, "query") != NULL ||
        strstr(s_cli_buf, "count") != NULL || strstr(s_cli_buf, "security") != NULL ||
        strstr(s_cli_buf, "LED") != NULL || strstr(s_cli_buf, "not found") != NULL) {
        /* This is a fingerprint-related response */
        if (strstr(s_cli_buf, "query list") != NULL || strstr(s_cli_buf, "ID") != NULL) {
            fp_parse_list(s_cli_buf, s_cli_len);
        } else {
            /* Show the response as status text (trim trailing newlines) */
            char status_msg[78];
            uint16_t slen = s_cli_len;
            if (slen > 77) slen = 77;
            memcpy(status_msg, s_cli_buf, slen);
            status_msg[slen] = '\0';
            /* Replace newlines with spaces for single-line display */
            for (uint16_t i = 0; i < slen; i++) {
                if (status_msg[i] == '\r' || status_msg[i] == '\n')
                    status_msg[i] = ' ';
            }
            fp_set_status(status_msg);
        }
    }
    /* Ignore non-fingerprint CLI responses */
}

static void fp_parse_list(const char *text, uint16_t len)
{
    fp_clear_list();

    /* Parse CLI output line by line.
     * Expected formats:
     *   "fp: query list sent" — just acknowledgment
     *   "fp: N fingerprints" or lines with ID info
     *   "ID: X  Name: YYYYY" or similar structured output
     */
    const char *p = text;
    const char *end = text + len;

    while (p < end && s_fp_count < FP_MAX_ENTRIES) {
        /* Find end of line */
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n')
            eol++;

        uint16_t line_len = (uint16_t)(eol - p);

        /* Try to parse "ID: X" or "id=X" patterns */
        {
            char line[128];
            uint16_t cpy = (line_len < 127) ? line_len : 127;
            memcpy(line, p, cpy);
            line[cpy] = '\0';

            /* Look for numeric ID pattern */
            const char *id_pos = strstr(line, "ID:");
            if (!id_pos) id_pos = strstr(line, "id:");
            if (!id_pos) id_pos = strstr(line, "ID=");
            if (!id_pos) id_pos = strstr(line, "id=");

            if (id_pos) {
                int id_val = atoi(id_pos + 3);
                if (id_val >= 0 && id_val <= 255) {
                    s_fp_list[s_fp_count].id = (uint8_t)id_val;

                    /* Look for name */
                    const char *name_pos = strstr(line, "Name:");
                    if (!name_pos) name_pos = strstr(line, "name:");
                    if (!name_pos) name_pos = strstr(line, "Name=");
                    if (!name_pos) name_pos = strstr(line, "name=");

                    if (name_pos) {
                        name_pos += 5;
                        while (*name_pos == ' ') name_pos++;
                        strncpy(s_fp_list[s_fp_count].name, name_pos, 19);
                        s_fp_list[s_fp_count].name[19] = '\0';
                        /* Trim trailing whitespace */
                        int nlen = (int)strlen(s_fp_list[s_fp_count].name) - 1;
                        while (nlen >= 0 && (s_fp_list[s_fp_count].name[nlen] == ' ' ||
                               s_fp_list[s_fp_count].name[nlen] == '\r' ||
                               s_fp_list[s_fp_count].name[nlen] == '\n'))
                            s_fp_list[s_fp_count].name[nlen--] = '\0';
                    } else {
                        snprintf(s_fp_list[s_fp_count].name, 20, "Finger_%d", id_val);
                    }
                    s_fp_count++;
                }
            }
            /* Also try "fp: N fingerprints" pattern for count display */
            else if (strstr(line, "fingerprint") != NULL || strstr(line, "count") != NULL) {
                /* Extract number if present */
                for (const char *c = line; *c; c++) {
                    if (*c >= '0' && *c <= '9') {
                        int cnt = atoi(c);
                        if (cnt > 0 && cnt <= 255) {
                            char buf[40];
                            snprintf(buf, sizeof(buf), "%d fingerprint(s)", cnt);
                            fp_set_status(buf);
                            break;
                        }
                    }
                }
            }
        }

        /* Advance past line ending */
        p = eol;
        while (p < end && (*p == '\r' || *p == '\n'))
            p++;
    }

    if (s_fp_count > 0) {
        char buf[40];
        snprintf(buf, sizeof(buf), "Found %d fingerprint(s)", s_fp_count);
        fp_set_status(buf);
    } else {
        fp_set_status("Query complete");
    }
    fp_invalidate_list();
}

/*=============================================================================
 *  List Touch Handler
 *=============================================================================*/

static void list_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int16_t rel_y = e->pos.y - FP_LIST_Y + s_fp_scroll * FP_ITEM_H;
        int16_t idx = rel_y / FP_ITEM_H;
        if (idx >= 0 && idx < s_fp_count) {
            int16_t old_sel = s_fp_selected;
            s_fp_selected = idx;
            /* Invalidate old and new selection */
            if (old_sel >= 0) {
                int16_t vis_old = old_sel - s_fp_scroll;
                if (vis_old >= 0 && vis_old < FP_VISIBLE_ITEMS) {
                    ui_rect_t r = {FP_LIST_X, FP_LIST_Y + vis_old * FP_ITEM_H, FP_LIST_W, FP_ITEM_H};
                    ui_page_invalidate(&r);
                }
            }
            {
                int16_t vis_new = idx - s_fp_scroll;
                if (vis_new >= 0 && vis_new < FP_VISIBLE_ITEMS) {
                    ui_rect_t r = {FP_LIST_X, FP_LIST_Y + vis_new * FP_ITEM_H, FP_LIST_W, FP_ITEM_H};
                    ui_page_invalidate(&r);
                }
            }
        }
    }
    else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_fp_scroll + FP_VISIBLE_ITEMS < s_fp_count) {
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

/*=============================================================================
 *  Button Callbacks
 *=============================================================================*/

static void btn_register_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("fp register");
    fp_set_status("Enrollment started, place finger on sensor...");
}

static void btn_delete_click(ui_widget_t *w)
{
    (void)w;
    if (s_fp_selected < 0 || s_fp_selected >= s_fp_count) {
        fp_set_status("Select a fingerprint to delete");
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
    fp_refresh_list();
}

/*=============================================================================
 *  LED Function Buttons
 *=============================================================================*/

static void func_btn_click(ui_widget_t *w)
{
    int idx = (int)(intptr_t)w->user_data;
    s_led_func = (uint8_t)(idx + 1);  /* 1-7 */

    fp_invalidate_func_btns();

    char cmd[48];
    snprintf(cmd, sizeof(cmd), "fp config led %d %d %d",
             s_led_func, s_led_color, s_led_speed);
    UART_SendCLI(cmd);

    char msg[60];
    snprintf(msg, sizeof(msg), "LED: %s (color=%d speed=%d)",
             s_led_func_names[idx], s_led_color, s_led_speed);
    fp_set_status(msg);
}

/*=============================================================================
 *  Color Touch Handler
 *=============================================================================*/

static void color_touch_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_DOWN) {
        int idx = (int)(intptr_t)w->user_data;
        s_led_color = (uint8_t)idx;

        fp_invalidate_color_btns();

        char cmd[48];
        snprintf(cmd, sizeof(cmd), "fp config led %d %d %d",
                 s_led_func, s_led_color, s_led_speed);
        UART_SendCLI(cmd);

        char msg[40];
        snprintf(msg, sizeof(msg), "LED color: %d", s_led_color);
        fp_set_status(msg);
    }
}

/*=============================================================================
 *  Speed Slider
 *=============================================================================*/

static void speed_slider_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    s_led_speed = (uint8_t)value;

    char cmd[48];
    snprintf(cmd, sizeof(cmd), "fp config led %d %d %d",
             s_led_func, s_led_color, s_led_speed);
    UART_SendCLI(cmd);
}

/*=============================================================================
 *  Security Level Buttons
 *=============================================================================*/

static void sec_btn_click(ui_widget_t *w)
{
    int idx = (int)(intptr_t)w->user_data;
    s_sec_level = (uint8_t)(idx + 1);  /* 1-3 */

    fp_invalidate_sec_btns();

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "fp config sec %d", s_sec_level);
    UART_SendCLI(cmd);

    char msg[40];
    snprintf(msg, sizeof(msg), "Security level: %d", s_sec_level);
    fp_set_status(msg);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void fp_page_enter(ui_page_t *page)
{
    (void)page;
    UART_SetAppCallbacks(&s_fp_callbacks);
    UART_SetSubmodelCallbacks(&s_fp_submodel_cb);
    s_id_result.result = 0;
    fp_refresh_list();
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

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    int16_t dirty_top = dirty->y;
    int16_t dirty_bot = dirty->y + dirty->h;

    /* Identification result display */
    if (dirty_top < FP_RESULT_Y + FP_RESULT_H && dirty_bot > FP_RESULT_Y) {
        ui_rect_t result_bg = {0, FP_RESULT_Y, FP_LEFT_W, FP_RESULT_H};
        if (s_id_result.result == 1) {
            /* Success */
            ui_draw_fill_rect(&result_bg, UI_HEX(0xE8F5E9));
            ui_draw_rect_border(&result_bg, UI_HEX(0x4CAF50), 2);
            /* Icon area */
            ui_draw_fill_circle(30, FP_RESULT_Y + FP_RESULT_H / 2, 16, UI_HEX(0x4CAF50));
            ui_draw_text(22, FP_RESULT_Y + FP_RESULT_H / 2 - 6, "OK",
                         &font_montserrat_12, UI_COLOR_WHITE);
            /* Text */
            ui_draw_text(54, FP_RESULT_Y + 8, "Identify Success",
                         &font_montserrat_16, UI_HEX(0x2E7D32));
            char id_str[24];
            snprintf(id_str, sizeof(id_str), "Fingerprint ID: #%d", s_id_result.fp_id);
            ui_draw_text(54, FP_RESULT_Y + 30, id_str,
                         &font_montserrat_12, UI_HEX(0x388E3C));
        } else if (s_id_result.result == 2) {
            /* Fail */
            ui_draw_fill_rect(&result_bg, UI_HEX(0xFBE9E7));
            ui_draw_rect_border(&result_bg, UI_HEX(0xF44336), 2);
            /* Icon area */
            ui_draw_fill_circle(30, FP_RESULT_Y + FP_RESULT_H / 2, 16, UI_HEX(0xF44336));
            ui_draw_text(23, FP_RESULT_Y + FP_RESULT_H / 2 - 6, "X",
                         &font_montserrat_12, UI_COLOR_WHITE);
            /* Text */
            ui_draw_text(54, FP_RESULT_Y + 8, "Identify Failed",
                         &font_montserrat_16, UI_HEX(0xC62828));
            ui_draw_text(54, FP_RESULT_Y + 30, "No matching fingerprint",
                         &font_montserrat_12, UI_HEX(0xD32F2F));
        } else {
            /* No result yet */
            ui_draw_fill_rect(&result_bg, FP_LIST_BG);
            ui_draw_rect_border(&result_bg, FP_BORDER, 1);
            ui_draw_text(12, FP_RESULT_Y + FP_RESULT_H / 2 - 6,
                         "Waiting for identification...",
                         &font_montserrat_12, FP_ITEM_ID);
        }
    }

    /* Left panel: fingerprint list */
    if (dirty_top < FP_LIST_Y + FP_LIST_H && dirty_bot > FP_LIST_Y) {
        /* List background */
        ui_rect_t list_bg = {FP_LIST_X, FP_LIST_Y, FP_LIST_W, FP_LIST_H};
        ui_draw_fill_rect(&list_bg, FP_LIST_BG);
        ui_draw_rect_border(&list_bg, FP_BORDER, 1);

        /* List header */
        ui_rect_t hdr = {FP_LIST_X, FP_LIST_Y, FP_LIST_W, FP_ITEM_H};
        ui_draw_fill_rect(&hdr, UI_HEX(0xE8F5E9));
        char hdr_text[32];
        snprintf(hdr_text, sizeof(hdr_text), "Fingerprints (%d)", s_fp_count);
        ui_draw_text(FP_LIST_X + 10, FP_LIST_Y + 10, hdr_text,
                     &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

        /* List items */
        for (int16_t vis = 0; vis < FP_VISIBLE_ITEMS; vis++) {
            int16_t idx = s_fp_scroll + vis;
            if (idx >= s_fp_count) break;

            int16_t item_y = FP_LIST_Y + (vis + 1) * FP_ITEM_H;
            /* Skip if outside list area */
            if (item_y + FP_ITEM_H > FP_LIST_Y + FP_LIST_H) break;

            ui_rect_t item_rect = {FP_LIST_X + 1, item_y, FP_LIST_W - 2, FP_ITEM_H};

            /* Selection / alternating background */
            if (idx == s_fp_selected) {
                ui_draw_fill_rect(&item_rect, FP_LIST_SEL);
            } else if (idx & 1) {
                ui_draw_fill_rect(&item_rect, FP_LIST_ALT);
            }

            /* ID badge */
            char id_text[8];
            snprintf(id_text, sizeof(id_text), "#%d", s_fp_list[idx].id);
            ui_draw_fill_round_rect(
                &(ui_rect_t){FP_LIST_X + 8, item_y + 6, 36, 22},
                4, UI_HEX(0x7EC8C8));
            ui_draw_text(FP_LIST_X + 12, item_y + 10, id_text,
                         &font_montserrat_12, UI_COLOR_WHITE);

            /* Name */
            ui_draw_text(FP_LIST_X + 52, item_y + 10, s_fp_list[idx].name,
                         &font_montserrat_12, FP_ITEM_TEXT);
        }

        /* Scroll indicator */
        if (s_fp_count > FP_VISIBLE_ITEMS) {
            int16_t sb_h = FP_LIST_H - FP_ITEM_H;
            int16_t thumb_h = sb_h * FP_VISIBLE_ITEMS / (s_fp_count + 1);
            if (thumb_h < 20) thumb_h = 20;
            int16_t thumb_y = FP_LIST_Y + FP_ITEM_H +
                              (sb_h - thumb_h) * s_fp_scroll /
                              (s_fp_count - FP_VISIBLE_ITEMS + 1);
            ui_rect_t thumb = {FP_LIST_X + FP_LIST_W - 6, thumb_y, 4, thumb_h};
            ui_draw_fill_round_rect(&thumb, 2, UI_HEX(0xBDBDBD));
        }
    }

    /* Right panel: controls */
    if (dirty_top < UI_SCREEN_HEIGHT && dirty_bot > APP_TITLE_BAR_H &&
        dirty->x + dirty->w > FP_CTRL_X) {

        /* Panel background */
        ui_rect_t ctrl_bg = {FP_CTRL_X, APP_TITLE_BAR_H, FP_CTRL_W, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - FP_STATUS_H};
        ui_draw_fill_rect(&ctrl_bg, FP_CTRL_BG);

        /* Section: Actions label */
        ui_draw_text(FP_CTRL_X + 12, FP_ACT_Y - 14, "Actions",
                     &font_montserrat_12, FP_SECTION_LABEL);

        /* Section: LED Control label */
        ui_draw_text(FP_CTRL_X + 12, FP_LED_Y - 14, "LED Control",
                     &font_montserrat_12, FP_SECTION_LABEL);

        /* Function button highlights */
        for (int i = 0; i < FP_NUM_FUNCS; i++) {
            if (i + 1 == s_led_func) {
                ui_rect_t r = btn_func[i].base.rect;
                ui_draw_rect_border(&r, UI_COLOR_PRIMARY, 2);
            }
        }

        /* Color button selection indicators */
        for (int i = 0; i < (int)FP_NUM_COLORS; i++) {
            if (i == s_led_color) {
                ui_rect_t r = s_color_btns[i].rect;
                ui_rect_t border = {r.x - 2, r.y - 2, r.w + 4, r.h + 4};
                ui_draw_rect_border(&border, UI_COLOR_TEXT_PRIMARY, 2);
            }
        }

        /* Section: Security label */
        ui_draw_text(FP_CTRL_X + 12, FP_SEC_Y - 14, "Security Level",
                     &font_montserrat_12, FP_SECTION_LABEL);

        /* Security button highlights */
        for (int i = 0; i < 3; i++) {
            if (i + 1 == s_sec_level) {
                ui_rect_t r = btn_sec[i].base.rect;
                ui_draw_rect_border(&r, UI_COLOR_PRIMARY, 2);
            }
        }
    }

    /* Status bar */
    if (dirty_bot > FP_STATUS_Y) {
        ui_rect_t status_bg = {0, FP_STATUS_Y, UI_SCREEN_WIDTH, FP_STATUS_H};
        ui_draw_fill_rect(&status_bg, FP_STATUS_BG);
        ui_draw_text(12, FP_STATUS_Y + 8, s_status,
                     &font_montserrat_12, FP_STATUS_TEXT);
    }
}

static bool fp_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_fp_selected < s_fp_count - 1) {
            int16_t old = s_fp_selected;
            s_fp_selected++;
            if (s_fp_selected >= s_fp_scroll + FP_VISIBLE_ITEMS)
                s_fp_scroll++;
            fp_invalidate_list();
            (void)old;
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_fp_selected > 0) {
            s_fp_selected--;
            if (s_fp_selected < s_fp_scroll)
                s_fp_scroll--;
            fp_invalidate_list();
        }
        return true;
    }
    if (e->type == UI_EVENT_KEY_OK) {
        /* Refresh list on Enter if nothing selected */
        if (s_fp_selected < 0) {
            fp_refresh_list();
        }
        return true;
    }

    return false;
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void app_fingerprint_init(void)
{
    ui_app_page_init(&s_app_fp, "Finger", 0x108);

    /* Initialize state */
    s_fp_count = 0;
    s_fp_scroll = 0;
    s_fp_selected = -1;
    s_cli_len = 0;
    s_cli_assembling = false;
    s_led_func = 1;     /* Breath */
    s_led_color = 1;    /* Blue */
    s_led_speed = 128;
    s_sec_level = 2;    /* Medium */
    strcpy(s_status, "Ready");

    /* --- Widgets --- */
    int widx = 0;

    /* Back button and title (from ui_app_page_t) */
    s_fp_widgets[widx++] = (ui_widget_t *)&s_app_fp.btn_back;
    s_fp_widgets[widx++] = (ui_widget_t *)&s_app_fp.lbl_title;

    /* List touch area */
    {
        ui_rect_t r = {FP_LIST_X, FP_LIST_Y + FP_ITEM_H, FP_LIST_W, FP_LIST_H - FP_ITEM_H};
        ui_widget_init(&s_list_touch, &r);
        s_list_touch.bg_color = UI_COLOR_TRANSPARENT;
        s_list_touch.event_cb = list_touch_event;
    }
    s_fp_widgets[widx++] = &s_list_touch;

    /* Action buttons */
    {
        int16_t bx = FP_CTRL_X + 12;
        ui_rect_t r1 = {bx, FP_ACT_Y, FP_ACT_BTN_W, FP_ACT_BTN_H};
        ui_button_init(&btn_register, &r1, "Register", &font_montserrat_12);
        ui_button_set_callback(&btn_register, btn_register_click);
        ui_button_set_colors(&btn_register, UI_COLOR_BG_CARD, UI_COLOR_PRIMARY, UI_COLOR_TEXT_PRIMARY);
        btn_register.radius = 8;
        s_fp_widgets[widx++] = (ui_widget_t *)&btn_register;

        bx += FP_ACT_BTN_W + FP_ACT_GAP;
        ui_rect_t r2 = {bx, FP_ACT_Y, FP_ACT_BTN_W, FP_ACT_BTN_H};
        ui_button_init(&btn_delete, &r2, "Delete", &font_montserrat_12);
        ui_button_set_callback(&btn_delete, btn_delete_click);
        ui_button_set_colors(&btn_delete, UI_COLOR_BG_CARD, UI_COLOR_DANGER, UI_COLOR_TEXT_PRIMARY);
        btn_delete.radius = 8;
        s_fp_widgets[widx++] = (ui_widget_t *)&btn_delete;

        bx += FP_ACT_BTN_W + FP_ACT_GAP;
        ui_rect_t r3 = {bx, FP_ACT_Y, FP_ACT_BTN_W, FP_ACT_BTN_H};
        ui_button_init(&btn_refresh, &r3, "Refresh", &font_montserrat_12);
        ui_button_set_callback(&btn_refresh, btn_refresh_click);
        ui_button_set_colors(&btn_refresh, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
        btn_refresh.radius = 8;
        s_fp_widgets[widx++] = (ui_widget_t *)&btn_refresh;
    }

    /* LED function buttons (7 modes) */
    {
        int16_t fx = FP_CTRL_X + 12;
        int16_t fy = FP_LED_Y + FP_LED_LABEL_H;
        int per_row = FP_CTRL_W / (FP_FUNC_BTN_W + FP_FUNC_GAP);
        if (per_row < 1) per_row = 1;

        for (int i = 0; i < FP_NUM_FUNCS; i++) {
            int16_t col = i % per_row;
            int16_t row = i / per_row;
            ui_rect_t r = {fx + col * (FP_FUNC_BTN_W + FP_FUNC_GAP),
                           fy + row * (FP_FUNC_BTN_H + FP_FUNC_GAP),
                           FP_FUNC_BTN_W, FP_FUNC_BTN_H};
            ui_button_init(&btn_func[i], &r, s_led_func_names[i], &font_montserrat_12);
            ui_button_set_callback(&btn_func[i], func_btn_click);
            ui_button_set_colors(&btn_func[i], UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
            btn_func[i].radius = 6;
            btn_func[i].base.user_data = (void *)(intptr_t)i;
            s_fp_widgets[widx++] = (ui_widget_t *)&btn_func[i];
        }
    }

    /* Color buttons (8 colors) */
    {
        int16_t cx = FP_CTRL_X + 12;
        for (int i = 0; i < (int)FP_NUM_COLORS; i++) {
            ui_rect_t r = {cx + i * (FP_COLOR_BTN_SIZE + FP_COLOR_GAP),
                           FP_COLOR_Y, FP_COLOR_BTN_SIZE, FP_COLOR_BTN_SIZE};
            ui_widget_init(&s_color_btns[i], &r);
            s_color_btns[i].bg_color = s_led_colors[i];
            s_color_btns[i].event_cb = color_touch_event;
            s_color_btns[i].user_data = (void *)(intptr_t)i;
            s_color_btns[i].flags |= UI_WIDGET_FLAG_OPAQUE;
            s_fp_widgets[widx++] = &s_color_btns[i];
        }
    }

    /* Speed slider */
    {
        ui_rect_t r = {FP_CTRL_X + 20, FP_SPEED_Y, FP_SPEED_W, FP_SPEED_H};
        ui_slider_init(&s_speed_slider, &r, 0, 255, s_led_speed);
        ui_slider_set_callback(&s_speed_slider, speed_slider_change);
        s_fp_widgets[widx++] = (ui_widget_t *)&s_speed_slider;
    }

    /* Security level buttons (1/2/3) */
    {
        int16_t sx = FP_CTRL_X + 12;
        for (int i = 0; i < 3; i++) {
            char label[4];
            snprintf(label, sizeof(label), "L%d", i + 1);
            ui_rect_t r = {sx + i * (FP_SEC_BTN_W + FP_SEC_GAP),
                           FP_SEC_Y + FP_LED_LABEL_H, FP_SEC_BTN_W, FP_SEC_BTN_H};
            ui_button_init(&btn_sec[i], &r, (i == 0) ? "Low" : (i == 1) ? "Med" : "High",
                           &font_montserrat_12);
            ui_button_set_callback(&btn_sec[i], sec_btn_click);
            ui_button_set_colors(&btn_sec[i], UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
            btn_sec[i].radius = 6;
            btn_sec[i].base.user_data = (void *)(intptr_t)i;
            s_fp_widgets[widx++] = (ui_widget_t *)&btn_sec[i];
        }
    }

    ui_page_set_widgets(&s_app_fp.page, s_fp_widgets, (uint16_t)widx);
    ui_page_set_callbacks(&s_app_fp.page, fp_page_enter, fp_page_exit, fp_page_draw, NULL);
    ui_page_set_event_cb(&s_app_fp.page, fp_page_event);
    ui_page_register(&s_app_fp.page);
}

ui_page_t *app_fingerprint_get_page(void)
{
    return &s_app_fp.page;
}
