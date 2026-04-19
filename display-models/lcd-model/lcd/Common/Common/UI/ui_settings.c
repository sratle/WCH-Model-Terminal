/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_settings.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Settings page implementation.
*                      Settings list with slider/switch/button controls.
********************************************************************************/
#include "ui_settings.h"
#include "ui_main.h"

/*=============================================================================
 *  Settings Item Configuration
 *=============================================================================*/

#define SETTINGS_ITEM_H     50
#define SETTINGS_TOP        70
#define SETTINGS_LEFT       20

/*=============================================================================
 *  Settings Page Widgets
 *=============================================================================*/

static ui_label_t lbl_title;

static ui_list_item_t item_brightness;
static ui_list_item_t item_volume;
static ui_list_item_t item_wifi;
static ui_list_item_t item_bt;
static ui_list_item_t item_touch_beep;
static ui_list_item_t item_auto_off;
static ui_list_item_t item_about;

static ui_slider_t slider_brightness;
static ui_slider_t slider_volume;
static ui_switch_t switch_wifi;
static ui_switch_t switch_bt;
static ui_switch_t switch_beep;
static ui_slider_t slider_auto_off;
static ui_button_t btn_about;

static ui_widget_t *s_settings_widgets[8];

/*=============================================================================
 *  Settings Callbacks
 *=============================================================================*/

static void brightness_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    (void)value;
}

static void volume_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    (void)value;
}

static void wifi_toggle(ui_widget_t *w, bool state)
{
    (void)w;
    (void)state;
}

static void bt_toggle(ui_widget_t *w, bool state)
{
    (void)w;
    (void)state;
}

static void beep_toggle(ui_widget_t *w, bool state)
{
    (void)w;
    (void)state;
}

static void auto_off_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    (void)value;
}

static void about_click(ui_widget_t *w)
{
    (void)w;
}

/*=============================================================================
 *  Settings Page Drawing
 *=============================================================================*/

void ui_settings_draw(ui_page_t *page, ui_rect_t *dirty)
{
    ui_rect_t content = {SIDEBAR_WIDTH, 0, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&content, UI_COLOR_BG_MAIN);
}

/*=============================================================================
 *  Settings Page Initialization
 *=============================================================================*/

void ui_settings_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + SETTINGS_LEFT;

    ui_rect_t title_rect = {cx, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Options", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    int16_t item_y = SETTINGS_TOP;
    int16_t item_w = UI_SCREEN_WIDTH - SIDEBAR_WIDTH - 2 * SETTINGS_LEFT;

    ui_rect_t item_rect = {cx, item_y, item_w, SETTINGS_ITEM_H};
    ui_list_item_init(&item_brightness, &item_rect, "Brightness", &font_montserrat_12);
    ui_rect_t sl_rect = {cx + item_w - 160, item_y + 15, 150, 20};
    ui_slider_init(&slider_brightness, &sl_rect, 0, 100, 70);
    ui_slider_set_callback(&slider_brightness, brightness_change);
    ui_list_item_set_control(&item_brightness, (ui_widget_t *)&slider_brightness);

    item_y += SETTINGS_ITEM_H;
    item_rect.y = item_y;
    ui_list_item_init(&item_volume, &item_rect, "Volume", &font_montserrat_12);
    sl_rect.y = item_y + 15;
    ui_slider_init(&slider_volume, &sl_rect, 0, 100, 50);
    ui_slider_set_callback(&slider_volume, volume_change);
    ui_list_item_set_control(&item_volume, (ui_widget_t *)&slider_volume);

    item_y += SETTINGS_ITEM_H;
    item_rect.y = item_y;
    ui_list_item_init(&item_wifi, &item_rect, "WiFi", &font_montserrat_12);
    ui_rect_t sw_rect = {cx + item_w - 60, item_y + 10, 50, 30};
    ui_switch_init(&switch_wifi, &sw_rect, false);
    ui_switch_set_callback(&switch_wifi, wifi_toggle);
    ui_list_item_set_control(&item_wifi, (ui_widget_t *)&switch_wifi);

    item_y += SETTINGS_ITEM_H;
    item_rect.y = item_y;
    ui_list_item_init(&item_bt, &item_rect, "Bluetooth", &font_montserrat_12);
    sw_rect.y = item_y + 10;
    ui_switch_init(&switch_bt, &sw_rect, true);
    ui_switch_set_callback(&switch_bt, bt_toggle);
    ui_list_item_set_control(&item_bt, (ui_widget_t *)&switch_bt);

    item_y += SETTINGS_ITEM_H;
    item_rect.y = item_y;
    ui_list_item_init(&item_touch_beep, &item_rect, "Touch Beep", &font_montserrat_12);
    sw_rect.y = item_y + 10;
    ui_switch_init(&switch_beep, &sw_rect, true);
    ui_switch_set_callback(&switch_beep, beep_toggle);
    ui_list_item_set_control(&item_touch_beep, (ui_widget_t *)&switch_beep);

    item_y += SETTINGS_ITEM_H;
    item_rect.y = item_y;
    ui_list_item_init(&item_auto_off, &item_rect, "Auto Off (min)", &font_montserrat_12);
    sl_rect.y = item_y + 15;
    ui_slider_init(&slider_auto_off, &sl_rect, 0, 30, 5);
    ui_slider_set_callback(&slider_auto_off, auto_off_change);
    ui_list_item_set_control(&item_auto_off, (ui_widget_t *)&slider_auto_off);

    item_y += SETTINGS_ITEM_H;
    item_rect.y = item_y;
    ui_list_item_init(&item_about, &item_rect, "About", &font_montserrat_12);
    item_about.show_divider = false;
    ui_rect_t btn_rect = {cx + item_w - 90, item_y + 8, 80, 34};
    ui_button_init(&btn_about, &btn_rect, "Info", &font_montserrat_12);
    ui_button_set_callback(&btn_about, about_click);
    ui_button_set_colors(&btn_about, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    ui_list_item_set_control(&item_about, (ui_widget_t *)&btn_about);

    s_settings_widgets[0] = (ui_widget_t *)&lbl_title;
    s_settings_widgets[1] = (ui_widget_t *)&item_brightness;
    s_settings_widgets[2] = (ui_widget_t *)&item_volume;
    s_settings_widgets[3] = (ui_widget_t *)&item_wifi;
    s_settings_widgets[4] = (ui_widget_t *)&item_bt;
    s_settings_widgets[5] = (ui_widget_t *)&item_touch_beep;
    s_settings_widgets[6] = (ui_widget_t *)&item_auto_off;
    s_settings_widgets[7] = (ui_widget_t *)&item_about;

    ui_page_set_widgets(&page_settings, s_settings_widgets, 8);
    ui_page_set_callbacks(&page_settings, ui_settings_enter, NULL, ui_settings_draw, NULL);
}

void ui_settings_enter(ui_page_t *page)
{
    (void)page;
}
