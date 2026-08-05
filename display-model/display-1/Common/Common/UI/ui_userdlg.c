/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_userdlg.c
* Description        : User dialog page.
*                      Guest: user selector + PIN login (fp/NFC hint).
*                      Logged in: user name + bound fp/NFC credentials + Logout.
********************************************************************************/
#include "ui_userdlg.h"
#include "ui_app_common.h"
#include "../UART/uart_module.h"
#include "../MiniUI/miniui_input.h"
#include <string.h>
#include <stdio.h>

/*=============================================================================
 *  Layout
 *=============================================================================*/

#define UD_PANEL_W          640
#define UD_PANEL_H          300
#define UD_MAX_USERS        8
#define UD_PIN_MAX          8

/* 左列（选择器/PIN/消息/按钮）与右侧 PIN Pad 的分区 */
#define UD_LEFT_W           380
#define UD_PAD_X            (UD_LEFT_W + 20)
#define UD_PAD_Y            16
#define UD_PAD_W            (UD_PANEL_W - UD_PAD_X - 16)
#define UD_PAD_H            (UD_PANEL_H - 32)

/*=============================================================================
 *  State
 *=============================================================================*/

typedef struct {
    char name[17];
    char fp[40];    /* "1,2" */
    char nfc[44];   /* "0A1A3BAC20,..." (截断显示) */
} ud_user_t;

typedef struct {
    ud_user_t users[UD_MAX_USERS];
    uint8_t  user_count;
    int8_t   user_sel;
    bool     fetching;
    char     pin[UD_PIN_MAX + 1];
    uint8_t  pin_len;
    char     message[48];
    bool     info_mode;     /* true=已登录（信息+登出），false=登录 */
} ud_state_t;

static ui_app_page_t s_ud;
static ui_widget_t   s_touch;
static ui_pinpad_t   s_pinpad;
static ui_widget_t  *s_widgets[5];
static ud_state_t    s_ud_st;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void ud_on_cli_complete(const char *buf, uint16_t len, const char *tag);
static void ud_on_user_status(uint8_t logged_in, const char *name);
static void ud_on_pinpad_key(ui_widget_t *w, uint8_t key);

static uart_cli_cb_t  s_ud_cli_cb = { .on_cli_complete = ud_on_cli_complete };
static uart_user_cb_t s_ud_user_cb = { .on_user_status = ud_on_user_status };

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void ud_invalidate(void)
{
    ui_page_invalidate_all();
}

static void ud_close(void)
{
    if (ui_page_can_go_back())
        ui_page_pop();
}

/* 解析 user ls 输出：行格式 "USER <name> fp=[1,2] nfc=[AA,BB]" */
static void ud_parse_users(const char *buf, uint16_t len)
{
    const char *p = buf;
    const char *end = buf + len;

    s_ud_st.user_count = 0;
    while (p < end && s_ud_st.user_count < UD_MAX_USERS) {
        const char *eol = p;
        while (eol < end && *eol != '\r' && *eol != '\n') eol++;

        if ((eol - p) > 5 && memcmp(p, "USER ", 5) == 0) {
            ud_user_t *u = &s_ud_st.users[s_ud_st.user_count];
            const char *name = p + 5;
            const char *sp = name;
            uint16_t nlen;

            while (sp < eol && *sp != ' ') sp++;
            nlen = (uint16_t)(sp - name);
            if (nlen > 0) {
                if (nlen > 16) nlen = 16;
                memcpy(u->name, name, nlen);
                u->name[nlen] = '\0';

                /* fp=[...] */
                u->fp[0] = '\0';
                const char *fp_seg = strstr(p, "fp=[");
                if (fp_seg && fp_seg < eol) {
                    const char *q = fp_seg + 4;
                    uint16_t o = 0;
                    while (q < eol && *q != ']' && o < sizeof(u->fp) - 1)
                        u->fp[o++] = *q++;
                    u->fp[o] = '\0';
                }

                /* nfc=[...] */
                u->nfc[0] = '\0';
                const char *nfc_seg = strstr(p, "nfc=[");
                if (nfc_seg && nfc_seg < eol) {
                    const char *q = nfc_seg + 5;
                    uint16_t o = 0;
                    while (q < eol && *q != ']' && o < sizeof(u->nfc) - 1)
                        u->nfc[o++] = *q++;
                    u->nfc[o] = '\0';
                }

                s_ud_st.user_count++;
            }
        }

        p = eol;
        while (p < end && (*p == '\r' || *p == '\n')) p++;
    }

    s_ud_st.fetching = false;
    if (s_ud_st.user_count > 0) {
        s_ud_st.user_sel = 0;
        /* 登录态下默认选中当前用户 */
        if (s_ud_st.info_mode) {
            for (uint8_t i = 0; i < s_ud_st.user_count; i++) {
                if (strcmp(s_ud_st.users[i].name, g_disp_state.user_name) == 0) {
                    s_ud_st.user_sel = (int8_t)i;
                    break;
                }
            }
        }
    } else if (!s_ud_st.info_mode) {
        snprintf(s_ud_st.message, sizeof(s_ud_st.message),
                 "No users. Use CLI: user add");
    }
    ud_invalidate();
}

static void ud_login_confirm(void)
{
    char cmd[64];

    if (s_ud_st.user_sel < 0 || s_ud_st.user_sel >= s_ud_st.user_count) {
        snprintf(s_ud_st.message, sizeof(s_ud_st.message), "No user selected");
        ud_invalidate();
        return;
    }
    if (s_ud_st.pin_len == 0) {
        snprintf(s_ud_st.message, sizeof(s_ud_st.message), "Enter PIN");
        ud_invalidate();
        return;
    }
    snprintf(cmd, sizeof(cmd), "login %s %s",
             s_ud_st.users[s_ud_st.user_sel].name, s_ud_st.pin);
    UART_SetCLICallbacks(&s_ud_cli_cb);
    UART_SendCLI(cmd);
    snprintf(s_ud_st.message, sizeof(s_ud_st.message), "Verifying...");
    ud_invalidate();
}

static void ud_logout(void)
{
    UART_SetCLICallbacks(&s_ud_cli_cb);
    UART_SendCLI("logout");
    /* 登出结果由 USER_STATUS 推送驱动关闭页面 */
    snprintf(s_ud_st.message, sizeof(s_ud_st.message), "Logging out...");
    ud_invalidate();
}

/* PIN Pad 按键：数字追加 / <- 删除 / OK 登录 */
static void ud_on_pinpad_key(ui_widget_t *w, uint8_t key)
{
    (void)w;

    if (s_ud_st.info_mode)
        return;

    if (key == UI_PINPAD_KEY_BACKSPACE) {
        if (s_ud_st.pin_len > 0)
            s_ud_st.pin[--s_ud_st.pin_len] = '\0';
        ud_invalidate();
    } else if (key == UI_PINPAD_KEY_OK) {
        ud_login_confirm();
    } else {
        if (s_ud_st.pin_len < UD_PIN_MAX) {
            s_ud_st.pin[s_ud_st.pin_len++] = (char)key;
            s_ud_st.pin[s_ud_st.pin_len] = '\0';
            ud_invalidate();
        }
    }
}

/*=============================================================================
 *  Callbacks
 *=============================================================================*/

static void ud_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    if (!tag) return;

    if (s_ud_st.fetching && strcmp(tag, "user") == 0) {
        ud_parse_users(buf, len);
        return;
    }
    if (strcmp(tag, "login") == 0) {
        if (strstr(buf, "Login OK") != NULL) {
            /* 成功：等待 Core 的 USER_STATUS 推送驱动关闭（避免重复 pop） */
            snprintf(s_ud_st.message, sizeof(s_ud_st.message), "Login OK");
            ud_invalidate();
        } else {
            s_ud_st.pin_len = 0;
            s_ud_st.pin[0] = '\0';
            snprintf(s_ud_st.message, sizeof(s_ud_st.message),
                     "Wrong PIN or user");
            ud_invalidate();
        }
        return;
    }
}

static void ud_on_user_status(uint8_t logged_in, const char *name)
{
    (void)logged_in; (void)name;
    /* 登录/登出状态变化（含指纹/NFC 登录）时关闭对话框 */
    ud_close();
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void ud_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)dirty;

    /* Title bar */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Background */
    ui_rect_t bg = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                    UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bg, UI_COLOR_BG_MAIN);

    /* Panel */
    int16_t dx = (UI_SCREEN_WIDTH - UD_PANEL_W) / 2;
    int16_t dy = (UI_SCREEN_HEIGHT - UD_PANEL_H) / 2;
    ui_rect_t panel = {dx, dy, UD_PANEL_W, UD_PANEL_H};
    ui_draw_fill_round_rect(&panel, 10, UI_COLOR_BG_CARD);
    ui_draw_round_rect_border(&panel, 10, UI_COLOR_SECONDARY, 1);

    if (!s_ud_st.info_mode) {
        /* ---- 登录模式（左列 + 右侧 PIN Pad 组件） ---- */
        ui_draw_text(dx + 20, dy + 16, "User Login", UI_FONT_TITLE,
                     UI_COLOR_TEXT_PRIMARY);

        /* User selector: [<] [ Name ] [>] */
        int16_t uy = dy + 52;
        ui_rect_t prev_r = {dx + 20, uy, 40, 36};
        ui_draw_fill_round_rect(&prev_r, 6, UI_COLOR_SECONDARY);
        ui_draw_text(prev_r.x + 14, uy + 10, "<", UI_FONT_BODY, UI_COLOR_WHITE);

        ui_rect_t name_r = {dx + 68, uy, 220, 36};
        ui_draw_fill_rect(&name_r, UI_COLOR_WHITE);
        ui_draw_rect_border(&name_r, UI_COLOR_PRIMARY, 1);
        const char *uname = (s_ud_st.user_sel >= 0 &&
                             s_ud_st.user_sel < s_ud_st.user_count)
                            ? s_ud_st.users[s_ud_st.user_sel].name : "(no user)";
        ui_draw_text_in_rect(&name_r, uname, UI_FONT_TITLE,
                             UI_COLOR_TEXT_PRIMARY, 0x11);

        ui_rect_t next_r = {dx + 296, uy, 40, 36};
        ui_draw_fill_round_rect(&next_r, 6, UI_COLOR_SECONDARY);
        ui_draw_text(next_r.x + 14, uy + 10, ">", UI_FONT_BODY, UI_COLOR_WHITE);

        /* PIN field (masked) */
        char masked[UD_PIN_MAX + 1];
        uint8_t i;
        for (i = 0; i < s_ud_st.pin_len; i++) masked[i] = '*';
        masked[i] = '\0';

        ui_rect_t field = {dx + 20, dy + 100, UD_LEFT_W - 40, 40};
        ui_textfield_style_t tf_st = {
            .bg = UI_COLOR_WHITE, .border = UI_COLOR_PRIMARY,
            .text = UI_COLOR_TEXT_PRIMARY, .hint = UI_COLOR_TEXT_DISABLED,
            .cursor = UI_COLOR_PRIMARY,
            .font = UI_FONT_TITLE, .hint_font = UI_FONT_BODY,
            .radius = 0, .border_w = 1,
        };
        ui_textfield_draw(&field, masked, (uint16_t)s_ud_st.pin_len,
                          "PIN", true, &tf_st);

        /* Message / hint */
        if (s_ud_st.message[0] != '\0')
            ui_draw_text(dx + 20, dy + 148, s_ud_st.message, UI_FONT_BODY,
                         UI_COLOR_DANGER);
        else
            ui_draw_text(dx + 20, dy + 148, "Fingerprint / NFC card also works",
                         UI_FONT_BODY, UI_COLOR_TEXT_SECONDARY);

        /* Login button */
        ui_rect_t ok_r = {dx + 20, dy + UD_PANEL_H - 52, 100, 36};
        ui_draw_fill_round_rect(&ok_r, 6, UI_COLOR_PRIMARY);
        ui_draw_text(ok_r.x + 22, ok_r.y + 10, "Login", UI_FONT_BODY,
                     UI_COLOR_WHITE);
    } else {
        /* ---- 信息模式（已登录） ---- */
        char line[64];

        ui_draw_text(dx + 20, dy + 16, "Current User", UI_FONT_TITLE,
                     UI_COLOR_TEXT_PRIMARY);

        snprintf(line, sizeof(line), "Name: %s", g_disp_state.user_name);
        ui_draw_text(dx + 20, dy + 52, line, UI_FONT_TITLE, UI_COLOR_PRIMARY);

        const ud_user_t *u = NULL;
        if (s_ud_st.user_sel >= 0 && s_ud_st.user_sel < s_ud_st.user_count)
            u = &s_ud_st.users[s_ud_st.user_sel];

        snprintf(line, sizeof(line), "FP:  %s",
                 (u && u->fp[0]) ? u->fp : "(none)");
        ui_draw_text(dx + 20, dy + 96, line, UI_FONT_BODY,
                     UI_COLOR_TEXT_PRIMARY);

        snprintf(line, sizeof(line), "NFC: %s",
                 (u && u->nfc[0]) ? u->nfc : "(none)");
        ui_draw_text(dx + 20, dy + 124, line, UI_FONT_BODY,
                     UI_COLOR_TEXT_PRIMARY);

        if (s_ud_st.message[0] != '\0')
            ui_draw_text(dx + 20, dy + 156, s_ud_st.message, UI_FONT_BODY,
                         UI_COLOR_TEXT_SECONDARY);

        /* Logout button */
        ui_rect_t lo_r = {dx + UD_PANEL_W - 120, dy + UD_PANEL_H - 52, 100, 36};
        ui_draw_fill_round_rect(&lo_r, 6, UI_COLOR_DANGER);
        ui_draw_text(lo_r.x + 16, lo_r.y + 10, "Logout", UI_FONT_BODY,
                     UI_COLOR_WHITE);
    }
}

/*=============================================================================
 *  Events
 *=============================================================================*/

static void ud_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK) return;

    int16_t dx = (UI_SCREEN_WIDTH - UD_PANEL_W) / 2;
    int16_t dy = (UI_SCREEN_HEIGHT - UD_PANEL_H) / 2;

    if (!s_ud_st.info_mode) {
        int16_t uy = dy + 52;
        ui_rect_t prev_r = {dx + 20, uy, 40, 36};
        if (ui_widget_hit_test((ui_widget_t *)&prev_r, e->pos.x, e->pos.y)) {
            if (s_ud_st.user_count > 0) {
                s_ud_st.user_sel--;
                if (s_ud_st.user_sel < 0) s_ud_st.user_sel = s_ud_st.user_count - 1;
                ud_invalidate();
            }
            return;
        }
        ui_rect_t next_r = {dx + 296, uy, 40, 36};
        if (ui_widget_hit_test((ui_widget_t *)&next_r, e->pos.x, e->pos.y)) {
            if (s_ud_st.user_count > 0) {
                s_ud_st.user_sel++;
                if (s_ud_st.user_sel >= s_ud_st.user_count) s_ud_st.user_sel = 0;
                ud_invalidate();
            }
            return;
        }
        ui_rect_t ok_r = {dx + 20, dy + UD_PANEL_H - 52, 100, 36};
        if (ui_widget_hit_test((ui_widget_t *)&ok_r, e->pos.x, e->pos.y)) {
            ud_login_confirm();
            return;
        }
    } else {
        ui_rect_t lo_r = {dx + UD_PANEL_W - 120, dy + UD_PANEL_H - 52, 100, 36};
        if (ui_widget_hit_test((ui_widget_t *)&lo_r, e->pos.x, e->pos.y)) {
            ud_logout();
            return;
        }
    }
}

static bool ud_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    if (e->source != UI_INPUT_KEYBOARD && e->source != UI_INPUT_CORE_KEY)
        return false;

    /* ESC/Back: close */
    if (e->type == UI_EVENT_KEY_DOWN && e->key_code == UI_KEY_BACK) {
        ud_close();
        return true;
    }

    if (!s_ud_st.info_mode) {
        /* Enter: login */
        if (e->type == UI_EVENT_KEY_DOWN && e->key_code == UI_KEY_OK) {
            ud_login_confirm();
            return true;
        }
        /* Left/Right: switch user */
        if (e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) {
            if (e->key_code == UI_KEY_LEFT && s_ud_st.user_count > 0) {
                s_ud_st.user_sel--;
                if (s_ud_st.user_sel < 0) s_ud_st.user_sel = s_ud_st.user_count - 1;
                ud_invalidate();
                return true;
            }
            if (e->key_code == UI_KEY_RIGHT && s_ud_st.user_count > 0) {
                s_ud_st.user_sel++;
                if (s_ud_st.user_sel >= s_ud_st.user_count) s_ud_st.user_sel = 0;
                ud_invalidate();
                return true;
            }
        }
        /* PIN input */
        if ((e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) &&
            e->char_code >= 0x20 && e->char_code <= 0x7E &&
            s_ud_st.pin_len < UD_PIN_MAX) {
            s_ud_st.pin[s_ud_st.pin_len++] = (char)e->char_code;
            s_ud_st.pin[s_ud_st.pin_len] = '\0';
            ud_invalidate();
            return true;
        }
        /* Backspace */
        if ((e->type == UI_EVENT_KEY_DOWN || e->type == UI_EVENT_KEY_LONG_REPEAT) &&
            e->char_code == 0x08) {
            if (s_ud_st.pin_len > 0)
                s_ud_st.pin[--s_ud_st.pin_len] = '\0';
            ud_invalidate();
            return true;
        }
        return true;    /* 登录模式下消费全部按键 */
    }

    return true;
}

/*=============================================================================
 *  Page Callbacks / Public API
 *=============================================================================*/

static void ud_page_enter(ui_page_t *page)
{
    (void)page;

    memset(&s_ud_st, 0, sizeof(s_ud_st));
    s_ud_st.user_sel = -1;
    s_ud_st.info_mode = (g_disp_state.user_logged_in != 0);
    s_ud_st.fetching = true;

    /* PIN Pad 仅登录模式可见可用 */
    ui_widget_set_visible((ui_widget_t *)&s_pinpad, !s_ud_st.info_mode);
    ui_widget_set_enabled((ui_widget_t *)&s_pinpad, !s_ud_st.info_mode);

    UART_SetCLICallbacks(&s_ud_cli_cb);
    UART_SetUserCallbacks(&s_ud_user_cb);
    UART_SendCLI("user ls");

    ui_page_invalidate_all();
}

static void ud_page_exit(ui_page_t *page)
{
    (void)page;
    UART_ClearUserCallbacks();
}

void ui_userdlg_init(void)
{
    ui_app_page_init(&s_ud, "User", 0x201);

    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch, &touch_rect);
    s_touch.bg_color = UI_COLOR_TRANSPARENT;
    s_touch.event_cb = ud_touch_event;

    /* PIN Pad 组件：登录模式可见（面板右侧区域），信息模式隐藏 */
    {
        int16_t dx = (UI_SCREEN_WIDTH - UD_PANEL_W) / 2;
        int16_t dy = (UI_SCREEN_HEIGHT - UD_PANEL_H) / 2;
        ui_rect_t pad_r = {dx + UD_PAD_X, dy + UD_PAD_Y, UD_PAD_W, UD_PAD_H};
        ui_pinpad_init(&s_pinpad, &pad_r);
        ui_pinpad_set_callback(&s_pinpad, ud_on_pinpad_key);
        ui_widget_set_visible((ui_widget_t *)&s_pinpad, false);
        ui_widget_set_enabled((ui_widget_t *)&s_pinpad, false);
    }

    s_widgets[0] = (ui_widget_t *)&s_ud.btn_back;
    s_widgets[1] = (ui_widget_t *)&s_ud.lbl_title;
    s_widgets[2] = &s_touch;
    s_widgets[3] = (ui_widget_t *)&s_pinpad;

    ui_page_set_widgets(&s_ud.page, s_widgets, 4);
    ui_page_set_callbacks(&s_ud.page, ud_page_enter, ud_page_exit,
                          ud_page_draw, NULL);
    ui_page_set_event_cb(&s_ud.page, ud_page_event);
    ui_page_register(&s_ud.page);
}

ui_page_t *ui_userdlg_get_page(void)
{
    return &s_ud.page;
}
