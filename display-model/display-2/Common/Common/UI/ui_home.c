/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_home.c
* Author             : E-ink Model Team
* Version            : V3.0.0
* Date               : 2026/07/19
* Description        : Home page implementation.
*                      Live date/time (from Core, locally advanced each
*                      second), module status cards (MCU/BT/Batt/Touch).
*                      Values are polled from g_disp_state once per second
*                      so Core-pushed status updates appear without hooks.
********************************************************************************/
#include "ui_home.h"
#include "ui_main.h"
#include "../UART/uart_module.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <stdio.h>
#include <string.h>

/*=============================================================================
 *  Home Page Widgets
 *=============================================================================*/

static ui_label_t lbl_date;
static ui_label_t lbl_time;
static ui_card_t card_core;
static ui_card_t card_bt;
static ui_card_t card_batt;
static ui_card_t card_touch;

static ui_label_t lbl_core_name;
static ui_label_t lbl_bt_name;
static ui_label_t lbl_batt_name;
static ui_label_t lbl_touch_name;
static ui_label_t lbl_batt_pct;

static ui_status_dot_t dot_core;
static ui_status_dot_t dot_bt;
static ui_status_dot_t dot_batt;
static ui_status_dot_t dot_touch;

static ui_widget_t *s_home_widgets[11];

/*=============================================================================
 *  Live Status State
 *=============================================================================*/

static char s_time_text[12];   /* "HH:MM:SS" */
static char s_date_text[14];   /* "YYYY/MM/DD" */
static char s_batt_text[8];    /* "85%" */
static uint32_t s_last_sec_ms = 0;
static uint8_t s_prev_battery = 0xFF;
static uint8_t s_prev_bt      = 0xFF;
static uint8_t s_prev_charge  = 0xFF;

/*=============================================================================
 *  Time Helpers
 *=============================================================================*/

/* days since 1970-01-01 -> (year, month, day), Howard Hinnant's algorithm */
static void civil_from_days(uint32_t z, int *y, unsigned *m, unsigned *d)
{
    z += 719468;
    uint32_t era = z / 146097;
    uint32_t doe = z - era * 146097;
    uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int yy = (int)yoe + (int)era * 400;
    uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint32_t mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = yy + (*m <= 2);
}

static void home_format_time(void)
{
    if (!(g_disp_state.status_valid & STATUS_BIT_TIME)) {
        strcpy(s_time_text, "--:--:--");
        strcpy(s_date_text, "----/--/--");
        return;
    }
    uint32_t t = g_disp_state.unix_time;
    unsigned hh = (t / 3600) % 24;
    unsigned mm = (t / 60) % 60;
    unsigned ss = t % 60;
    snprintf(s_time_text, sizeof(s_time_text), "%02u:%02u:%02u", hh, mm, ss);

    int y;
    unsigned m, d;
    civil_from_days(t / 86400, &y, &m, &d);
    snprintf(s_date_text, sizeof(s_date_text), "%04d/%02u/%02u", y, m, d);
}

static void home_format_battery(void)
{
    if (!(g_disp_state.status_valid & STATUS_BIT_BATTERY)) {
        strcpy(s_batt_text, "--");
        return;
    }
    if (g_disp_state.charge_state & 0x01) {
        snprintf(s_batt_text, sizeof(s_batt_text), "%d%%+", g_disp_state.battery_pct);
    } else {
        snprintf(s_batt_text, sizeof(s_batt_text), "%d%%", g_disp_state.battery_pct);
    }
}

/*=============================================================================
 *  Home Page Initialization
 *=============================================================================*/

void ui_home_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t date_rect = {cx, 20, 200, 24};
    ui_label_init(&lbl_date, &date_rect, "----/--/--", &font_montserrat_12);
    ui_label_set_color(&lbl_date, UI_COLOR_TEXT_SECONDARY);

    ui_rect_t time_rect = {cx, 48, 300, 36};
    ui_label_init(&lbl_time, &time_rect, "--:--:--", &font_montserrat_16);
    ui_label_set_color(&lbl_time, UI_COLOR_PRIMARY);

    int16_t card_w = 90;
    int16_t card_h = 90;
    int16_t card_gap = 12;
    int16_t card_y = 110;

    ui_rect_t card_rect = {cx, card_y, card_w, card_h};
    ui_card_init(&card_core, &card_rect);
    card_core.base.bg_color = UI_COLOR_BG_CARD;
    card_core.radius = 10;
    card_core.border_width = 1;

    card_rect.x = cx + card_w + card_gap;
    ui_card_init(&card_bt, &card_rect);
    card_bt.base.bg_color = UI_COLOR_BG_CARD;
    card_bt.radius = 10;
    card_bt.border_width = 1;

    card_rect.x = cx + 2 * (card_w + card_gap);
    ui_card_init(&card_batt, &card_rect);
    card_batt.base.bg_color = UI_COLOR_BG_CARD;
    card_batt.radius = 10;
    card_batt.border_width = 1;

    card_rect.x = cx + 3 * (card_w + card_gap);
    ui_card_init(&card_touch, &card_rect);
    card_touch.base.bg_color = UI_COLOR_BG_CARD;
    card_touch.radius = 10;
    card_touch.border_width = 1;

    ui_rect_t name_rect = {0, 0, card_w - 30, 20};
    name_rect.x = cx + 10;
    name_rect.y = card_y + 10;
    ui_label_init(&lbl_core_name, &name_rect, "MCU", &font_montserrat_12);
    lbl_core_name.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_core_name, UI_COLOR_TEXT_PRIMARY);

    name_rect.x = cx + card_w + card_gap + 10;
    ui_label_init(&lbl_bt_name, &name_rect, "BT", &font_montserrat_12);
    lbl_bt_name.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_bt_name, UI_COLOR_TEXT_PRIMARY);

    name_rect.x = cx + 2 * (card_w + card_gap) + 10;
    ui_label_init(&lbl_batt_name, &name_rect, "Batt", &font_montserrat_12);
    lbl_batt_name.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_batt_name, UI_COLOR_TEXT_PRIMARY);

    name_rect.x = cx + 3 * (card_w + card_gap) + 10;
    ui_label_init(&lbl_touch_name, &name_rect, "Touch", &font_montserrat_12);
    lbl_touch_name.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_touch_name, UI_COLOR_TEXT_PRIMARY);

    /* Battery percentage text inside the Batt card */
    ui_rect_t pct_rect = {cx + 2 * (card_w + card_gap) + 10, card_y + 40, card_w - 20, 24};
    ui_label_init(&lbl_batt_pct, &pct_rect, "--", &font_montserrat_16);
    lbl_batt_pct.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_batt_pct, UI_COLOR_TEXT_PRIMARY);

    ui_rect_t dot_rect = {0, 0, 12, 12};
    dot_rect.x = cx + card_w - 24;
    dot_rect.y = card_y + 14;
    ui_status_dot_init(&dot_core, &dot_rect, true);

    dot_rect.x = cx + card_w + card_gap + card_w - 24;
    ui_status_dot_init(&dot_bt, &dot_rect, false);

    dot_rect.x = cx + 2 * (card_w + card_gap) + card_w - 24;
    ui_status_dot_init(&dot_batt, &dot_rect, false);

    dot_rect.x = cx + 3 * (card_w + card_gap) + card_w - 24;
    ui_status_dot_init(&dot_touch, &dot_rect, true);

    ui_card_add_child(&card_core, (ui_widget_t *)&lbl_core_name);
    ui_card_add_child(&card_core, (ui_widget_t *)&dot_core);

    ui_card_add_child(&card_bt, (ui_widget_t *)&lbl_bt_name);
    ui_card_add_child(&card_bt, (ui_widget_t *)&dot_bt);

    ui_card_add_child(&card_batt, (ui_widget_t *)&lbl_batt_name);
    ui_card_add_child(&card_batt, (ui_widget_t *)&dot_batt);
    ui_card_add_child(&card_batt, (ui_widget_t *)&lbl_batt_pct);

    ui_card_add_child(&card_touch, (ui_widget_t *)&lbl_touch_name);
    ui_card_add_child(&card_touch, (ui_widget_t *)&dot_touch);

    s_home_widgets[0] = (ui_widget_t *)&lbl_date;
    s_home_widgets[1] = (ui_widget_t *)&lbl_time;
    s_home_widgets[2] = (ui_widget_t *)&card_core;
    s_home_widgets[3] = (ui_widget_t *)&card_bt;
    s_home_widgets[4] = (ui_widget_t *)&card_batt;
    s_home_widgets[5] = (ui_widget_t *)&card_touch;

    ui_page_set_widgets(&page_home, s_home_widgets, 6);
    ui_page_set_callbacks(&page_home, ui_home_enter, NULL, NULL, NULL);
    ui_page_set_update_cb(&page_home, ui_home_update);
}

void ui_home_enter(ui_page_t *page)
{
    (void)page;
    home_format_time();
    home_format_battery();
    ui_label_set_text(&lbl_date, s_date_text);
    ui_label_set_text(&lbl_time, s_time_text);
    ui_label_set_text(&lbl_batt_pct, s_batt_text);

    s_prev_battery = g_disp_state.battery_pct;
    s_prev_bt = g_disp_state.bt_connected;
    s_prev_charge = g_disp_state.charge_state;
    ui_status_dot_set_state(&dot_bt, g_disp_state.bt_connected != 0);
    ui_status_dot_set_state(&dot_batt,
                            (g_disp_state.status_valid & STATUS_BIT_BATTERY) &&
                            (g_disp_state.battery_pct > 20 || (g_disp_state.charge_state & 0x01)));
    s_last_sec_ms = ui_get_real_ms();
}

/* Poll Core-pushed status once per second (cheap, hook-free) */
void ui_home_update(ui_page_t *page, uint32_t dt_ms)
{
    (void)page;
    (void)dt_ms;

    uint32_t now = ui_get_real_ms();
    if ((uint32_t)(now - s_last_sec_ms) < 1000) return;
    uint32_t elapsed = now - s_last_sec_ms;
    s_last_sec_ms = now;

    bool changed = false;

    /* Advance the clock locally (Core re-syncs via UPDATE_STATUS) */
    if (g_disp_state.status_valid & STATUS_BIT_TIME) {
        g_disp_state.unix_time += elapsed / 1000;
    }
    home_format_time();
    if (strcmp(s_time_text, lbl_time.text) != 0) {
        ui_label_set_text(&lbl_time, s_time_text);
        ui_label_set_text(&lbl_date, s_date_text);
        changed = true;
    }

    if (g_disp_state.battery_pct != s_prev_battery ||
        g_disp_state.charge_state != s_prev_charge) {
        s_prev_battery = g_disp_state.battery_pct;
        s_prev_charge = g_disp_state.charge_state;
        home_format_battery();
        ui_label_set_text(&lbl_batt_pct, s_batt_text);
        ui_status_dot_set_state(&dot_batt,
                                (g_disp_state.status_valid & STATUS_BIT_BATTERY) &&
                                (g_disp_state.battery_pct > 20 || (g_disp_state.charge_state & 0x01)));
        changed = true;
    }

    if (g_disp_state.bt_connected != s_prev_bt) {
        s_prev_bt = g_disp_state.bt_connected;
        ui_status_dot_set_state(&dot_bt, g_disp_state.bt_connected != 0);
        changed = true;
    }

    (void)changed;   /* widget setters already invalidate their rects */
}
