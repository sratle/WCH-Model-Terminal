/********************************** (C) COPYRIGHT *******************************
* File Name          : app_bt.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/07/22
* Description        : BT app — shows whether the wireless (CH585F) chip is
*                      online, whether an app/phone is currently connected,
*                      and a bar chart of the last 10 BT traffic samples
*                      (byte counts). Display-only.
*                      Reactive: Core pushes online/connect/traffic into
*                      g_disp_state; the page tick repaints only the changed
*                      region (MiniUI partial refresh).
********************************************************************************/
#include "app_bt.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <stdio.h>

/*=============================================================================
 *  Layout (800x480)
 *=============================================================================*/

#define BT_ROW_X        40
#define BT_ROW_W        (UI_SCREEN_WIDTH - 2 * BT_ROW_X)

#define BT_HDR_Y        (APP_TITLE_BAR_H + 8)
#define BT_STATUS_Y     (APP_TITLE_BAR_H + 36)
#define BT_STATUS_H     100
#define BT_CHART_Y      (BT_STATUS_Y + BT_STATUS_H + 20)
#define BT_CHART_H      224

#define BT_CARD_BORDER  UI_HEX(0xE0E0E0)
#define BT_BAR_COLOR    UI_COLOR_PRIMARY
#define BT_GRID_COLOR   UI_HEX(0xEAEAEA)

/* Chart plot area inside the chart card */
#define BT_PLOT_PAD_L   56
#define BT_PLOT_PAD_R   16
#define BT_PLOT_PAD_T   34
#define BT_PLOT_PAD_B   22

static ui_app_page_t s_app_bt;

/* Change-detection cache */
static uint8_t  s_prev_online    = 0xFF;
static uint8_t  s_prev_connected = 0xFF;
static uint32_t s_prev_traffic_sig = 0xFFFFFFFFu;

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static uint32_t bt_traffic_signature(void)
{
    uint32_t sig = g_disp_state.bt_traffic_count;
    uint8_t i;
    for (i = 0; i < g_disp_state.bt_traffic_count && i < 10; i++)
        sig = sig * 31u + g_disp_state.bt_traffic[i];
    return sig;
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void bt_draw_status(void)
{
    uint8_t online    = g_disp_state.wireless_online;
    uint8_t connected = g_disp_state.bt_connected;

    ui_rect_t card = {BT_ROW_X, BT_STATUS_Y, BT_ROW_W, BT_STATUS_H};
    ui_draw_fill_round_rect(&card, 10, UI_COLOR_BG_CARD);
    ui_draw_round_rect_border(&card, 10, BT_CARD_BORDER, 1);

    int16_t line1 = BT_STATUS_Y + 24;
    int16_t line2 = BT_STATUS_Y + 62;

    /* Wireless chip online */
    ui_draw_fill_circle(BT_ROW_X + 30, line1 + 8, 11,
                        online ? UI_COLOR_STATUS_ON : UI_COLOR_STATUS_OFF);
    ui_draw_text(BT_ROW_X + 52, line1, "Wireless chip",
                 &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);
    ui_draw_text(BT_ROW_X + 260, line1, online ? "Online" : "Offline",
                 &font_montserrat_16,
                 online ? UI_COLOR_TEXT_PRIMARY : UI_COLOR_TEXT_SECONDARY);

    /* App/phone connection */
    ui_draw_fill_circle(BT_ROW_X + 30, line2 + 8, 11,
                        connected ? UI_COLOR_STATUS_ON : UI_COLOR_STATUS_OFF);
    ui_draw_text(BT_ROW_X + 52, line2, "App connection",
                 &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);
    ui_draw_text(BT_ROW_X + 260, line2, connected ? "Connected" : "Not connected",
                 &font_montserrat_16,
                 connected ? UI_COLOR_TEXT_PRIMARY : UI_COLOR_TEXT_SECONDARY);
}

static void bt_draw_chart(void)
{
    ui_rect_t card = {BT_ROW_X, BT_CHART_Y, BT_ROW_W, BT_CHART_H};
    ui_draw_fill_round_rect(&card, 10, UI_COLOR_BG_CARD);
    ui_draw_round_rect_border(&card, 10, BT_CARD_BORDER, 1);

    ui_draw_text(BT_ROW_X + 16, BT_CHART_Y + 10, "Recent BT traffic (bytes)",
                 &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);

    uint8_t n = g_disp_state.bt_traffic_count;
    if (n == 0) {
        ui_draw_text(BT_ROW_X + BT_ROW_W / 2 - 50, BT_CHART_Y + BT_CHART_H / 2 - 8,
                     "No traffic yet", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
        return;
    }

    /* Plot area */
    int16_t px = BT_ROW_X + BT_PLOT_PAD_L;
    int16_t py = BT_CHART_Y + BT_PLOT_PAD_T;
    int16_t pw = BT_ROW_W - BT_PLOT_PAD_L - BT_PLOT_PAD_R;
    int16_t ph = BT_CHART_H - BT_PLOT_PAD_T - BT_PLOT_PAD_B;

    /* Peak for scaling */
    uint16_t peak = 1;
    uint8_t i;
    for (i = 0; i < n; i++)
        if (g_disp_state.bt_traffic[i] > peak) peak = g_disp_state.bt_traffic[i];

    /* Axis + max label */
    ui_draw_hline(px, py + ph, pw, UI_COLOR_TEXT_SECONDARY);
    ui_draw_vline(px, py, ph, UI_COLOR_TEXT_SECONDARY);
    {
        char maxs[12];
        snprintf(maxs, sizeof(maxs), "%u", peak);
        ui_draw_text(BT_ROW_X + 8, py - 6, maxs, &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
        ui_draw_hline(px, py, pw, BT_GRID_COLOR);
    }

    /* 10 fixed slots so bars keep a stable position as history fills */
    int16_t slot_w = pw / 10;
    int16_t bar_w  = slot_w - 8;
    if (bar_w < 4) bar_w = 4;

    for (i = 0; i < n; i++) {
        uint16_t val = g_disp_state.bt_traffic[i];
        int16_t bh = (int16_t)((int32_t)ph * val / peak);
        if (bh < 1 && val > 0) bh = 1;
        int16_t bx = px + 4 + i * slot_w;
        int16_t by = py + ph - bh;

        if (bh > 0) {
            ui_rect_t bar = {bx, by, bar_w, bh};
            ui_draw_fill_round_rect(&bar, 3, BT_BAR_COLOR);
        }
        /* Value label above the bar */
        {
            char vs[12];
            snprintf(vs, sizeof(vs), "%u", val);
            int16_t tw = ui_text_width(vs, &font_montserrat_12);
            int16_t tx = bx + (bar_w - tw) / 2;
            if (tx < px) tx = px;
            int16_t ty = by - 14;
            if (ty < py) ty = py;
            ui_draw_text(tx, ty, vs, &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
        }
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void bt_page_enter(ui_page_t *page)
{
    (void)page;
    s_prev_online      = 0xFF;
    s_prev_connected   = 0xFF;
    s_prev_traffic_sig = 0xFFFFFFFFu;
    UART_SendGetSysStatus();
    ui_page_invalidate_all();
}

static void bt_page_update(ui_page_t *page)
{
    (void)page;
    uint8_t online    = g_disp_state.wireless_online;
    uint8_t connected = g_disp_state.bt_connected;
    uint32_t sig      = bt_traffic_signature();

    if (online != s_prev_online || connected != s_prev_connected) {
        s_prev_online    = online;
        s_prev_connected = connected;
        ui_rect_t r = {BT_ROW_X, BT_STATUS_Y, BT_ROW_W, BT_STATUS_H};
        ui_page_invalidate(&r);
    }
    if (sig != s_prev_traffic_sig) {
        s_prev_traffic_sig = sig;
        ui_rect_t r = {BT_ROW_X, BT_CHART_Y, BT_ROW_W, BT_CHART_H};
        ui_page_invalidate(&r);
    }
}

static void bt_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    int16_t dtop = dirty->y;
    int16_t dbot = dirty->y + dirty->h;

    if (dtop < BT_STATUS_Y) {
        ui_draw_text(BT_ROW_X, BT_HDR_Y, "Bluetooth / Wireless",
                     &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
    }
    if (dbot > BT_STATUS_Y && dtop < BT_STATUS_Y + BT_STATUS_H)
        bt_draw_status();
    if (dbot > BT_CHART_Y && dtop < BT_CHART_Y + BT_CHART_H)
        bt_draw_chart();
}

/*=============================================================================
 *  Init
 *=============================================================================*/

void app_bt_init(void)
{
    ui_app_page_init(&s_app_bt, "BT", 0x106);
    ui_page_set_callbacks(&s_app_bt.page, bt_page_enter, NULL, bt_page_draw, NULL);
    ui_page_set_update_cb(&s_app_bt.page, bt_page_update);
    ui_page_register(&s_app_bt.page);
}

ui_page_t *app_bt_get_page(void)
{
    return &s_app_bt.page;
}
