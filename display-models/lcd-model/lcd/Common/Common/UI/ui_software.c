/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_software.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Software page implementation.
*                      App grid with icon buttons.
********************************************************************************/
#include "ui_software.h"
#include "ui_main.h"

/*=============================================================================
 *  App Grid Configuration
 *=============================================================================*/

#define APP_GRID_COLS       4
#define APP_GRID_ROWS       3
#define APP_BTN_W           120
#define APP_BTN_H           100
#define APP_BTN_GAP_X       16
#define APP_BTN_GAP_Y       16
#define APP_GRID_TOP        70

/*=============================================================================
 *  App Entry Data
 *=============================================================================*/

typedef struct {
    const char *name;
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
} app_entry_t;

static const app_entry_t s_apps[APP_GRID_COLS * APP_GRID_ROWS] = {
    {"Audio",    icon_audio_16_bitmap,    ICON_AUDIO_16_WIDTH,    ICON_AUDIO_16_HEIGHT},
    {"Video",    icon_video_16_bitmap,    ICON_VIDEO_16_WIDTH,    ICON_VIDEO_16_HEIGHT},
    {"Image",    icon_image_16_bitmap,    ICON_IMAGE_16_WIDTH,    ICON_IMAGE_16_HEIGHT},
    {"Files",    icon_directory_16_bitmap,ICON_DIRECTORY_16_WIDTH,ICON_DIRECTORY_16_HEIGHT},
    {"Settings", icon_settings_16_bitmap, ICON_SETTINGS_16_WIDTH, ICON_SETTINGS_16_HEIGHT},
    {"Download", icon_download_16_bitmap, ICON_DOWNLOAD_16_WIDTH, ICON_DOWNLOAD_16_HEIGHT},
    {"Upload",   icon_upload_16_bitmap,   ICON_UPLOAD_16_WIDTH,   ICON_UPLOAD_16_HEIGHT},
    {"Edit",     icon_edit_16_bitmap,     ICON_EDIT_16_WIDTH,     ICON_EDIT_16_HEIGHT},
    {"USB",      icon_usb_16_bitmap,      ICON_USB_16_WIDTH,      ICON_USB_16_HEIGHT},
    {"BT",       icon_bluetooth_16_bitmap,ICON_BLUETOOTH_16_WIDTH,ICON_BLUETOOTH_16_HEIGHT},
    {"WiFi",     icon_wifi_16_bitmap,     ICON_WIFI_16_WIDTH,     ICON_WIFI_16_HEIGHT},
    {"GPS",      icon_gps_16_bitmap,      ICON_GPS_16_WIDTH,      ICON_GPS_16_HEIGHT},
};

/*=============================================================================
 *  Software Page Widgets
 *=============================================================================*/

static ui_label_t lbl_title;
static ui_icon_button_t btn_apps[APP_GRID_COLS * APP_GRID_ROWS];
static ui_widget_t *s_software_widgets[1 + APP_GRID_COLS * APP_GRID_ROWS];

/*=============================================================================
 *  App Button Callback
 *=============================================================================*/

static void app_button_click(ui_widget_t *w)
{
    (void)w;
}

/*=============================================================================
 *  Software Page Drawing
 *=============================================================================*/

void ui_software_draw(ui_page_t *page, ui_rect_t *dirty)
{
    ui_rect_t content = {SIDEBAR_WIDTH, 0, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&content, UI_COLOR_BG_MAIN);
}

/*=============================================================================
 *  Software Page Initialization
 *=============================================================================*/

void ui_software_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t title_rect = {cx, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Software", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    s_software_widgets[0] = (ui_widget_t *)&lbl_title;

    int16_t grid_x_start = cx + 10;
    int16_t grid_y_start = APP_GRID_TOP;

    for (int i = 0; i < APP_GRID_COLS * APP_GRID_ROWS; i++) {
        int16_t col = i % APP_GRID_COLS;
        int16_t row = i / APP_GRID_COLS;

        ui_rect_t btn_rect = {
            grid_x_start + col * (APP_BTN_W + APP_BTN_GAP_X),
            grid_y_start + row * (APP_BTN_H + APP_BTN_GAP_Y),
            APP_BTN_W,
            APP_BTN_H
        };

        ui_icon_button_init(&btn_apps[i], &btn_rect,
                            s_apps[i].icon, s_apps[i].icon_w, s_apps[i].icon_h,
                            s_apps[i].name, &font_montserrat_12);
        ui_icon_button_set_callback(&btn_apps[i], app_button_click);

        s_software_widgets[1 + i] = (ui_widget_t *)&btn_apps[i];
    }

    ui_page_set_widgets(&page_software, s_software_widgets, 1 + APP_GRID_COLS * APP_GRID_ROWS);
    ui_page_set_callbacks(&page_software, ui_software_enter, NULL, ui_software_draw, NULL);
}

void ui_software_enter(ui_page_t *page)
{
    (void)page;
}
