/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_home.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Home page implementation.
*                      Date/time display and module status cards.
********************************************************************************/
#include "ui_home.h"
#include "ui_main.h"

/*=============================================================================
 *  Home Page Widgets
 *=============================================================================*/

static ui_label_t lbl_date;
static ui_label_t lbl_time;
static ui_card_t card_core;
static ui_card_t card_bt;
static ui_card_t card_wifi;
static ui_card_t card_touch;

static ui_label_t lbl_core_name;
static ui_label_t lbl_bt_name;
static ui_label_t lbl_wifi_name;
static ui_label_t lbl_touch_name;

static ui_status_dot_t dot_core;
static ui_status_dot_t dot_bt;
static ui_status_dot_t dot_wifi;
static ui_status_dot_t dot_touch;

static ui_widget_t *s_home_widgets[10];

/*=============================================================================
 *  Home Page Drawing
 *=============================================================================*/

void ui_home_draw(ui_page_t *page, ui_rect_t *dirty)
{
    ui_rect_t content = {SIDEBAR_WIDTH, 0, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&content, UI_COLOR_BG_MAIN);
}

/*=============================================================================
 *  Home Page Initialization
 *=============================================================================*/

void ui_home_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t date_rect = {cx, 20, 200, 24};
    ui_label_init(&lbl_date, &date_rect, "2025/04/19", &font_montserrat_12);
    ui_label_set_color(&lbl_date, UI_COLOR_TEXT_SECONDARY);

    ui_rect_t time_rect = {cx, 48, 300, 36};
    ui_label_init(&lbl_time, &time_rect, "14:30:00", &font_montserrat_16);
    ui_label_set_color(&lbl_time, UI_COLOR_PRIMARY);

    int16_t card_w = 130;
    int16_t card_h = 90;
    int16_t card_gap = 12;
    int16_t card_y = 110;

    ui_rect_t card_rect = {cx, card_y, card_w, card_h};
    ui_card_init(&card_core, &card_rect);
    card_core.base.bg_color = UI_COLOR_BG_CARD;
    card_core.radius = 10;

    card_rect.x = cx + card_w + card_gap;
    ui_card_init(&card_bt, &card_rect);
    card_bt.base.bg_color = UI_COLOR_BG_CARD;
    card_bt.radius = 10;

    card_rect.x = cx + 2 * (card_w + card_gap);
    ui_card_init(&card_wifi, &card_rect);
    card_wifi.base.bg_color = UI_COLOR_BG_CARD;
    card_wifi.radius = 10;

    card_rect.x = cx + 3 * (card_w + card_gap);
    ui_card_init(&card_touch, &card_rect);
    card_touch.base.bg_color = UI_COLOR_BG_CARD;
    card_touch.radius = 10;

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
    ui_label_init(&lbl_wifi_name, &name_rect, "WiFi", &font_montserrat_12);
    lbl_wifi_name.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_wifi_name, UI_COLOR_TEXT_PRIMARY);

    name_rect.x = cx + 3 * (card_w + card_gap) + 10;
    ui_label_init(&lbl_touch_name, &name_rect, "Touch", &font_montserrat_12);
    lbl_touch_name.base.bg_color = UI_COLOR_TRANSPARENT;
    ui_label_set_color(&lbl_touch_name, UI_COLOR_TEXT_PRIMARY);

    ui_rect_t dot_rect = {0, 0, 12, 12};
    dot_rect.x = cx + card_w - 24;
    dot_rect.y = card_y + 14;
    ui_status_dot_init(&dot_core, &dot_rect, true);

    dot_rect.x = cx + card_w + card_gap + card_w - 24;
    ui_status_dot_init(&dot_bt, &dot_rect, true);

    dot_rect.x = cx + 2 * (card_w + card_gap) + card_w - 24;
    ui_status_dot_init(&dot_wifi, &dot_rect, false);

    dot_rect.x = cx + 3 * (card_w + card_gap) + card_w - 24;
    ui_status_dot_init(&dot_touch, &dot_rect, true);

    ui_card_add_child(&card_core, (ui_widget_t *)&lbl_core_name);
    ui_card_add_child(&card_core, (ui_widget_t *)&dot_core);

    ui_card_add_child(&card_bt, (ui_widget_t *)&lbl_bt_name);
    ui_card_add_child(&card_bt, (ui_widget_t *)&dot_bt);

    ui_card_add_child(&card_wifi, (ui_widget_t *)&lbl_wifi_name);
    ui_card_add_child(&card_wifi, (ui_widget_t *)&dot_wifi);

    ui_card_add_child(&card_touch, (ui_widget_t *)&lbl_touch_name);
    ui_card_add_child(&card_touch, (ui_widget_t *)&dot_touch);

    s_home_widgets[0] = (ui_widget_t *)&lbl_date;
    s_home_widgets[1] = (ui_widget_t *)&lbl_time;
    s_home_widgets[2] = (ui_widget_t *)&card_core;
    s_home_widgets[3] = (ui_widget_t *)&card_bt;
    s_home_widgets[4] = (ui_widget_t *)&card_wifi;
    s_home_widgets[5] = (ui_widget_t *)&card_touch;

    ui_page_set_widgets(&page_home, s_home_widgets, 6);
    ui_page_set_callbacks(&page_home, ui_home_enter, NULL, ui_home_draw, NULL);
}

void ui_home_enter(ui_page_t *page)
{
    ui_label_set_text(&lbl_date, "2025/04/19");
    ui_label_set_text(&lbl_time, "14:30:00");
}
