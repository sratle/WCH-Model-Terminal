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
#include "../SSD1963/ssd1963.h"
#include "../settings.h"
#include "../UART/uart_module.h"

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
static ui_list_item_t item_auto_off;
static ui_list_item_t item_about;

static ui_slider_t slider_brightness;
static ui_slider_t slider_volume;
static ui_slider_t slider_auto_off;
static ui_button_t btn_about;

static ui_widget_t *s_settings_widgets[5];

/*=============================================================================
 *  Settings Callbacks
 *=============================================================================*/

static void brightness_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    g_settings.backlight = (uint8_t)value;
    SSD1963_SetBacklight(g_settings.backlight);
}

static void volume_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    g_settings.volume = (uint8_t)value;
    /* 发送音量设置命令给 Core (VOLUME_OP_SET=0x00) */
    UART_SendVolumeControl(0x00, (uint8_t)value);
}

static void auto_off_change(ui_widget_t *w, int16_t value)
{
    (void)w;
    (void)value;
}

static ui_dialog_t dlg_about;

static void about_click(ui_widget_t *w)
{
    (void)w;
    ui_dialog_show(&dlg_about, "About",
                   "WCH Model Terminal\n2026@ELAB\nShen Xingyi\nZhang ZhiCheng\nHuang Yuchun",
                   NULL, NULL);
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
    ui_slider_init(&slider_brightness, &sl_rect, 50, 150, (int16_t)g_settings.backlight);
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

    ui_dialog_init(&dlg_about);

    s_settings_widgets[0] = (ui_widget_t *)&lbl_title;
    s_settings_widgets[1] = (ui_widget_t *)&item_brightness;
    s_settings_widgets[2] = (ui_widget_t *)&item_volume;
    s_settings_widgets[3] = (ui_widget_t *)&item_auto_off;
    s_settings_widgets[4] = (ui_widget_t *)&item_about;

    ui_page_set_widgets(&page_settings, s_settings_widgets, 5);
    ui_page_set_callbacks(&page_settings, ui_settings_enter, NULL, NULL, NULL);
}

void ui_settings_enter(ui_page_t *page)
{
    (void)page;
}
