/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_models.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : Models page implementation.
*                      Tab view with module status cards.
********************************************************************************/
#include "ui_models.h"
#include "ui_main.h"

/*=============================================================================
 *  Module Status Data
*=============================================================================*/

typedef struct {
    const char *name;
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
    bool online;
    const char *version;
} module_info_t;

static const module_info_t s_comm_modules[] = {
    {"Bluetooth", icon_bluetooth_16_bitmap, ICON_BLUETOOTH_16_WIDTH, ICON_BLUETOOTH_16_HEIGHT, true,  "v2.1"},
    {"WiFi",      icon_wifi_16_bitmap,      ICON_WIFI_16_WIDTH,      ICON_WIFI_16_HEIGHT,      false, "v1.3"},
    {"USB",       icon_usb_16_bitmap,       ICON_USB_16_WIDTH,       ICON_USB_16_HEIGHT,       true,  "v3.0"},
    {"GPS",       icon_gps_16_bitmap,       ICON_GPS_16_WIDTH,       ICON_GPS_16_HEIGHT,       true,  "v1.0"},
};

static const module_info_t s_display_modules[] = {
    {"LCD",       icon_image_16_bitmap,     ICON_IMAGE_16_WIDTH,     ICON_IMAGE_16_HEIGHT,     true,  "v1.0"},
    {"Touch",     icon_bars_16_bitmap,      ICON_BARS_16_WIDTH,      ICON_BARS_16_HEIGHT,      true,  "v2.0"},
    {"Audio",     icon_audio_16_bitmap,     ICON_AUDIO_16_WIDTH,     ICON_AUDIO_16_HEIGHT,     true,  "v1.2"},
};

static const module_info_t s_storage_modules[] = {
    {"SD Card",   icon_sd_card_16_bitmap,   ICON_SD_CARD_16_WIDTH,   ICON_SD_CARD_16_HEIGHT,   true,  "v1.0"},
    {"Flash",     icon_drive_16_bitmap,     ICON_DRIVE_16_WIDTH,     ICON_DRIVE_16_HEIGHT,     true,  "v2.1"},
    {"Download",  icon_download_16_bitmap,  ICON_DOWNLOAD_16_WIDTH,  ICON_DOWNLOAD_16_HEIGHT,  false, "v1.0"},
};

/*=============================================================================
 *  Models Page Widgets
*=============================================================================*/

static ui_label_t lbl_title;
static ui_tabview_t tabview;

static ui_card_t card_comm[4];
static ui_card_t card_disp[3];
static ui_card_t card_stor[3];

static ui_label_t lbl_comm_name[4];
static ui_label_t lbl_disp_name[3];
static ui_label_t lbl_stor_name[3];

static ui_status_dot_t dot_comm[4];
static ui_status_dot_t dot_disp[3];
static ui_status_dot_t dot_stor[3];

static ui_label_t lbl_comm_ver[4];
static ui_label_t lbl_disp_ver[3];
static ui_label_t lbl_stor_ver[3];

static ui_widget_t *s_models_widgets[6];

static bool s_models_inited = false;

static void update_models_widgets(void)
{
    uint8_t tab = ui_tabview_get_active(&tabview);
    s_models_widgets[0] = (ui_widget_t *)&lbl_title;
    s_models_widgets[1] = (ui_widget_t *)&tabview;

    ui_card_t *cards = NULL;
    int count = 0;
    switch (tab) {
    case 0: cards = card_comm; count = 4; break;
    case 1: cards = card_disp; count = 3; break;
    case 2: cards = card_stor; count = 3; break;
    default: break;
    }

    for (int i = 0; i < count; i++) {
        s_models_widgets[2 + i] = (ui_widget_t *)&cards[i];
    }
    ui_page_set_widgets(&page_models, s_models_widgets, 2 + count);
}

/*=============================================================================
 *  Tab Change Handler
*=============================================================================*/

static void models_tab_change(ui_widget_t *w, uint8_t tab)
{
    (void)w;
    (void)tab;
    update_models_widgets();
    ui_page_invalidate_all();
}

/*=============================================================================
 *  Internal: Initialize card widgets for a tab
*=============================================================================*/

static void init_tab_cards(const module_info_t *mods, ui_card_t *cards,
                           ui_label_t *names, ui_status_dot_t *dots,
                           ui_label_t *vers, int count,
                           int16_t cx, int16_t cy_start)
{
    int16_t card_w = 160;
    int16_t card_h = 70;
    int16_t card_gap = 12;
    int16_t col = 0;
    int16_t cy = cy_start;

    for (int i = 0; i < count; i++) {
        ui_rect_t card_rect = {cx + col * (card_w + card_gap), cy, card_w, card_h};
        ui_card_init(&cards[i], &card_rect);
        cards[i].base.bg_color = UI_COLOR_BG_CARD;
        cards[i].radius = 8;

        ui_rect_t name_r = {card_rect.x + 10, card_rect.y + 8, 100, 18};
        ui_label_init(&names[i], &name_r, mods[i].name, &font_montserrat_12);
        names[i].base.bg_color = UI_COLOR_TRANSPARENT;
        ui_label_set_color(&names[i], UI_COLOR_TEXT_PRIMARY);

        ui_rect_t dot_r = {card_rect.x + card_w - 22, card_rect.y + 10, 12, 12};
        ui_status_dot_init(&dots[i], &dot_r, mods[i].online);

        ui_rect_t ver_r = {card_rect.x + 10, card_rect.y + 34, 80, 16};
        ui_label_init(&vers[i], &ver_r, mods[i].version, &font_montserrat_12);
        vers[i].base.bg_color = UI_COLOR_TRANSPARENT;
        ui_label_set_color(&vers[i], UI_COLOR_TEXT_SECONDARY);

        ui_card_add_child(&cards[i], (ui_widget_t *)&names[i]);
        ui_card_add_child(&cards[i], (ui_widget_t *)&dots[i]);
        ui_card_add_child(&cards[i], (ui_widget_t *)&vers[i]);

        col++;
        if (col >= 3) { col = 0; cy += card_h + card_gap; }
    }
}

/*=============================================================================
 *  Models Page Initialization
*=============================================================================*/

void ui_models_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t title_rect = {cx, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Models", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    ui_rect_t tv_rect = {SIDEBAR_WIDTH, 50, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT - 50};
    ui_tabview_init(&tabview, &tv_rect, 3);
    ui_tabview_set_label(&tabview, 0, "Comm");
    ui_tabview_set_label(&tabview, 1, "Display");
    ui_tabview_set_label(&tabview, 2, "Storage");
    tabview.on_tab_change = models_tab_change;

    s_models_widgets[0] = (ui_widget_t *)&lbl_title;
    s_models_widgets[1] = (ui_widget_t *)&tabview;
    ui_page_set_widgets(&page_models, s_models_widgets, 2);
    ui_page_set_callbacks(&page_models, ui_models_enter, NULL, NULL, NULL);

    s_models_inited = false;
}

void ui_models_enter(ui_page_t *page)
{
    (void)page;

    if (!s_models_inited) {
        int16_t cx = SIDEBAR_WIDTH + 20;
        int16_t cy = tabview.content_rect.y + 10;

        init_tab_cards(s_comm_modules, card_comm, lbl_comm_name, dot_comm, lbl_comm_ver, 4, cx, cy);
        init_tab_cards(s_display_modules, card_disp, lbl_disp_name, dot_disp, lbl_disp_ver, 3, cx, cy);
        init_tab_cards(s_storage_modules, card_stor, lbl_stor_name, dot_stor, lbl_stor_ver, 3, cx, cy);

        update_models_widgets();
        s_models_inited = true;
    }
}
