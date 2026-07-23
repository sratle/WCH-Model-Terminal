/********************************** (C) COPYRIGHT *******************************
* File Name          : app_power.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/07/22
* Description        : Power app — shows whether the Power module is connected
*                      and its current battery level. Display-only.
*                      Reactive: Core pushes module online + battery into
*                      g_disp_state; the page tick repaints only the region
*                      that changed (MiniUI partial refresh).
********************************************************************************/
#include "app_power.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <stdio.h>

/*=============================================================================
 *  Layout (800x480)
 *=============================================================================*/

#define PWR_ROW_X       48
#define PWR_ROW_W       (UI_SCREEN_WIDTH - 2 * PWR_ROW_X)

#define PWR_HDR_Y       (APP_TITLE_BAR_H + 8)
#define PWR_STATUS_Y    (APP_TITLE_BAR_H + 40)
#define PWR_STATUS_H    72
#define PWR_BATT_Y      (PWR_STATUS_Y + PWR_STATUS_H + 24)
#define PWR_BATT_H      180

#define PWR_CARD_BORDER UI_HEX(0xE0E0E0)

/* Battery glyph geometry (inside the battery card) */
#define BATT_W          280
#define BATT_H          110
#define BATT_X          (PWR_ROW_X + (PWR_ROW_W - BATT_W) / 2)
#define BATT_TERM_W     14
#define BATT_TERM_H     44

static ui_app_page_t s_app_power;

/* Change-detection cache */
static uint8_t s_prev_online = 0xFF;
static uint8_t s_prev_batt   = 0xFF;
static uint8_t s_prev_charge = 0xFF;

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static ui_color_t batt_color(uint8_t pct)
{
    if (pct <= 15) return UI_COLOR_DANGER;
    if (pct <= 30) return UI_COLOR_WARNING;
    return UI_COLOR_STATUS_ON;
}

static void pwr_draw_status(void)
{
    uint8_t online   = g_disp_state.power_online;
    uint8_t charging = (g_disp_state.charge_state & 0x01) ? 1 : 0;
    ui_rect_t card = {PWR_ROW_X, PWR_STATUS_Y, PWR_ROW_W, PWR_STATUS_H};
    ui_draw_fill_round_rect(&card, 10, UI_COLOR_BG_CARD);
    ui_draw_round_rect_border(&card, 10, PWR_CARD_BORDER, 1);

    ui_color_t dot = online ? UI_COLOR_STATUS_ON : UI_COLOR_STATUS_OFF;
    ui_draw_fill_circle(PWR_ROW_X + 32, PWR_STATUS_Y + PWR_STATUS_H / 2, 13, dot);

    ui_draw_text(PWR_ROW_X + 62, PWR_STATUS_Y + 14, "Power Module",
                 &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);
    ui_draw_text(PWR_ROW_X + 62, PWR_STATUS_Y + 42,
                 online ? "Connected" : "Not connected",
                 &font_montserrat_12,
                 online ? UI_COLOR_TEXT_PRIMARY : UI_COLOR_TEXT_SECONDARY);

    /* 右侧充电状态徐章 */
    if (online) {
        const char *txt = charging ? "Charging" : "Discharging";
        ui_color_t  col = charging ? UI_COLOR_STATUS_ON : UI_COLOR_TEXT_SECONDARY;
        int16_t tw = ui_text_width(txt, &font_montserrat_16);
        int16_t pill_w = tw + 40;
        int16_t pill_x = PWR_ROW_X + PWR_ROW_W - pill_w - 16;
        int16_t pill_y = PWR_STATUS_Y + (PWR_STATUS_H - 34) / 2;
        ui_rect_t pill = {pill_x, pill_y, pill_w, 34};
        ui_draw_round_rect_border(&pill, 17, col, 2);
        ui_draw_fill_circle(pill_x + 16, pill_y + 17, 5, col);
        ui_draw_text(pill_x + 28, pill_y + 9, txt, &font_montserrat_16, col);
    }
}

static void pwr_draw_battery(void)
{
    uint8_t online   = g_disp_state.power_online;
    uint8_t pct      = g_disp_state.battery_pct;
    uint8_t charging = (g_disp_state.charge_state & 0x01) ? 1 : 0;

    ui_rect_t card = {PWR_ROW_X, PWR_BATT_Y, PWR_ROW_W, PWR_BATT_H};
    ui_draw_fill_round_rect(&card, 10, UI_COLOR_BG_CARD);
    ui_draw_round_rect_border(&card, 10, PWR_CARD_BORDER, 1);

    if (!online) {
        ui_draw_text(PWR_ROW_X + 24, PWR_BATT_Y + PWR_BATT_H / 2 - 8,
                     "-- no power module --",
                     &font_montserrat_16, UI_COLOR_TEXT_SECONDARY);
        return;
    }

    if (pct > 100) pct = 100;

    /* Battery body outline */
    int16_t bx = BATT_X;
    int16_t by = PWR_BATT_Y + 22;
    ui_rect_t body = {bx, by, BATT_W, BATT_H};
    ui_draw_round_rect_border(&body, 8, UI_COLOR_TEXT_PRIMARY, 3);
    /* Positive terminal */
    ui_rect_t term = {bx + BATT_W + 3, by + (BATT_H - BATT_TERM_H) / 2,
                      BATT_TERM_W, BATT_TERM_H};
    ui_draw_fill_round_rect(&term, 3, UI_COLOR_TEXT_PRIMARY);

    /* Fill proportional to percentage */
    int16_t inner_x = bx + 8;
    int16_t inner_y = by + 8;
    int16_t inner_w = BATT_W - 16;
    int16_t inner_h = BATT_H - 16;
    int16_t fill_w  = (int16_t)((int32_t)inner_w * pct / 100);
    if (fill_w > 0) {
        ui_rect_t fill = {inner_x, inner_y, fill_w, inner_h};
        ui_draw_fill_round_rect(&fill, 4, batt_color(pct));
    }

    /* Percentage text centered in the battery */
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%u%%", pct);
    int16_t tw = ui_text_width(pct_str, &font_montserrat_16);
    ui_draw_text(bx + (BATT_W - tw) / 2, by + BATT_H / 2 - 8,
                 pct_str, &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);

    /* 充电状态 */
    ui_draw_text(bx, by + BATT_H + 14,
                 charging ? "Charging" : "On battery (discharging)",
                 &font_montserrat_16,
                 charging ? UI_COLOR_STATUS_ON : UI_COLOR_TEXT_SECONDARY);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void pwr_page_enter(ui_page_t *page)
{
    (void)page;
    s_prev_online = 0xFF;
    s_prev_batt   = 0xFF;
    s_prev_charge = 0xFF;
    UART_SendGetSysStatus();
    ui_page_invalidate_all();
}

static void pwr_page_update(ui_page_t *page)
{
    (void)page;
    uint8_t online = g_disp_state.power_online;
    uint8_t batt   = g_disp_state.battery_pct;
    uint8_t charge = (g_disp_state.charge_state & 0x01);

    if (online != s_prev_online) {
        s_prev_online = online;
        ui_rect_t r1 = {PWR_ROW_X, PWR_STATUS_Y, PWR_ROW_W, PWR_STATUS_H};
        ui_rect_t r2 = {PWR_ROW_X, PWR_BATT_Y, PWR_ROW_W, PWR_BATT_H};
        ui_page_invalidate(&r1);
        ui_page_invalidate(&r2);
    }
    if (batt != s_prev_batt || charge != s_prev_charge) {
        uint8_t charge_changed = (charge != s_prev_charge);
        s_prev_batt   = batt;
        s_prev_charge = charge;
        ui_rect_t rb = {PWR_ROW_X, PWR_BATT_Y, PWR_ROW_W, PWR_BATT_H};
        ui_page_invalidate(&rb);
        if (charge_changed) {   /* 充电徐章在状态卡，需一同刷新 */
            ui_rect_t rs = {PWR_ROW_X, PWR_STATUS_Y, PWR_ROW_W, PWR_STATUS_H};
            ui_page_invalidate(&rs);
        }
    }
}

static void pwr_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    int16_t dtop = dirty->y;
    int16_t dbot = dirty->y + dirty->h;

    if (dtop < PWR_STATUS_Y) {
        ui_draw_text(PWR_ROW_X, PWR_HDR_Y, "Power",
                     &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
    }
    if (dbot > PWR_STATUS_Y && dtop < PWR_STATUS_Y + PWR_STATUS_H)
        pwr_draw_status();
    if (dbot > PWR_BATT_Y && dtop < PWR_BATT_Y + PWR_BATT_H)
        pwr_draw_battery();
}

/*=============================================================================
 *  Init
 *=============================================================================*/

void app_power_init(void)
{
    ui_app_page_init(&s_app_power, "Power", 0x105);
    ui_page_set_callbacks(&s_app_power.page, pwr_page_enter, NULL, pwr_page_draw, NULL);
    ui_page_set_update_cb(&s_app_power.page, pwr_page_update);
    ui_page_register(&s_app_power.page);
}

ui_page_t *app_power_get_page(void)
{
    return &s_app_power.page;
}
