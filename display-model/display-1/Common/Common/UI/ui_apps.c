/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_apps.c
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/25
* Description        : Apps page implementation.
*                      16 apps in a single 4x4 grid (no pagination needed).
*                      V4.0: Keyboard navigation support (arrow keys + Enter).
********************************************************************************/
#include "ui_apps.h"
#include "ui_main.h"
#include "../Apps/apps.h"

/*=============================================================================
 *  App Grid Configuration
 *=============================================================================*/

#define APPS_GRID_COLS       4
#define APPS_GRID_ROWS       4
#define APPS_PER_PAGE        (APPS_GRID_COLS * APPS_GRID_ROWS)
#define APP_BTN_W            128
#define APP_BTN_H            90
#define APP_BTN_GAP_X        14
#define APP_BTN_GAP_Y        12
#define APPS_GRID_TOP        70
#define APPS_TOTAL           16
#define APPS_PAGE_COUNT      ((APPS_TOTAL + APPS_PER_PAGE - 1) / APPS_PER_PAGE)

/*=============================================================================
 *  App Entry Data
 *=============================================================================*/

typedef struct {
    const char *name;
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
    ui_page_t *(*get_page)(void);
} app_entry_t;

static const app_entry_t s_apps[APPS_TOTAL] = {
    {"Music",       icon_audio_16_bitmap,       ICON_AUDIO_16_WIDTH,       ICON_AUDIO_16_HEIGHT,       app_music_get_page},
    {"Files",       icon_directory_16_bitmap,    ICON_DIRECTORY_16_WIDTH,   ICON_DIRECTORY_16_HEIGHT,   app_file_get_page},
    {"Editor",      icon_edit_16_bitmap,        ICON_EDIT_16_WIDTH,        ICON_EDIT_16_HEIGHT,        app_editor_get_page},
    {"Images",      icon_image_16_bitmap,       ICON_IMAGE_16_WIDTH,       ICON_IMAGE_16_HEIGHT,       app_images_get_page},
    {"USB",         icon_usb_16_bitmap,         ICON_USB_16_WIDTH,         ICON_USB_16_HEIGHT,         app_usb_get_page},
    {"Power",       icon_power_16_bitmap,       ICON_POWER_16_WIDTH,       ICON_POWER_16_HEIGHT,       app_power_get_page},
    {"BT",          icon_bluetooth_16_bitmap,   ICON_BLUETOOTH_16_WIDTH,   ICON_BLUETOOTH_16_HEIGHT,   app_bt_get_page},
    {"NFC",         icon_wifi_16_bitmap,        ICON_WIFI_16_WIDTH,        ICON_WIFI_16_HEIGHT,        app_nfc_get_page},
    {"Finger",      icon_uf15b_16_bitmap,       ICON_UF15B_16_WIDTH,       ICON_UF15B_16_HEIGHT,       app_fingerprint_get_page},
    {"Health",      icon_charge_16_bitmap,      ICON_CHARGE_16_WIDTH,      ICON_CHARGE_16_HEIGHT,      app_health_get_page},
    {"SubDisp",     icon_bars_16_bitmap,        ICON_BARS_16_WIDTH,        ICON_BARS_16_HEIGHT,        app_subdisplay_get_page},
    {"Lights",      icon_tint_16_bitmap,        ICON_TINT_16_WIDTH,        ICON_TINT_16_HEIGHT,        app_lights_get_page},
    {"IRRange",     icon_gps_16_bitmap,         ICON_GPS_16_WIDTH,         ICON_GPS_16_HEIGHT,         app_irrange_get_page},
    {"EBook",       icon_list_16_bitmap,        ICON_LIST_16_WIDTH,        ICON_LIST_16_HEIGHT,        app_ebook_get_page},
    {"EMusic",      icon_volume_max_16_bitmap,  ICON_VOLUME_MAX_16_WIDTH,  ICON_VOLUME_MAX_16_HEIGHT,  app_emusic_get_page},
    {"Terminal",    icon_keyboard_16_bitmap,    ICON_KEYBOARD_16_WIDTH,    ICON_KEYBOARD_16_HEIGHT,    app_terminal_get_page},
};

/*=============================================================================
 *  Apps Page State
 *=============================================================================*/

static uint8_t s_current_page = 0;
static int8_t  s_focus_idx = -1;   /* Keyboard focus index within current page grid (-1 = none) */

static ui_label_t lbl_title;
static ui_label_t lbl_page;
static ui_button_t btn_prev;
static ui_button_t btn_next;
static ui_icon_button_t btn_apps[APPS_PER_PAGE];
static ui_widget_t *s_apps_widgets[4 + APPS_PER_PAGE];

/*=============================================================================
 *  Focus Management
 *=============================================================================*/

/* Get the number of visible apps on the current page */
static int apps_on_page(void)
{
    int page_offset = s_current_page * APPS_PER_PAGE;
    int remaining = APPS_TOTAL - page_offset;
    return (remaining < APPS_PER_PAGE) ? remaining : APPS_PER_PAGE;
}

static void apps_clear_focus(void)
{
    if (s_focus_idx >= 0 && s_focus_idx < APPS_PER_PAGE) {
        btn_apps[s_focus_idx].base.flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate((ui_widget_t *)&btn_apps[s_focus_idx]);
    }
    s_focus_idx = -1;
}

static void apps_set_focus(int8_t idx)
{
    apps_clear_focus();
    if (idx >= 0 && idx < apps_on_page()) {
        s_focus_idx = idx;
        btn_apps[s_focus_idx].base.flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate((ui_widget_t *)&btn_apps[s_focus_idx]);
    }
}

/*=============================================================================
 *  Page Navigation
 *=============================================================================*/

static char s_page_text[8];

static void apps_update_grid(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;
    int16_t grid_x_start = cx + 10;
    int16_t grid_y_start = APPS_GRID_TOP;
    int page_offset = s_current_page * APPS_PER_PAGE;

    for (int i = 0; i < APPS_PER_PAGE; i++) {
        int app_idx = page_offset + i;

        if (app_idx < APPS_TOTAL) {
            int16_t col = i % APPS_GRID_COLS;
            int16_t row = i / APPS_GRID_COLS;

            ui_rect_t btn_rect = {
                grid_x_start + col * (APP_BTN_W + APP_BTN_GAP_X),
                grid_y_start + row * (APP_BTN_H + APP_BTN_GAP_Y),
                APP_BTN_W,
                APP_BTN_H
            };

            btn_apps[i].base.rect = btn_rect;
            btn_apps[i].icon_bitmap = s_apps[app_idx].icon;
            btn_apps[i].icon_w = s_apps[app_idx].icon_w;
            btn_apps[i].icon_h = s_apps[app_idx].icon_h;
            btn_apps[i].text = s_apps[app_idx].name;
            btn_apps[i].base.user_data = (void *)(intptr_t)app_idx;
            ui_widget_set_visible((ui_widget_t *)&btn_apps[i], true);
        } else {
            ui_widget_set_visible((ui_widget_t *)&btn_apps[i], false);
        }
    }

    s_page_text[0] = '1' + s_current_page;
    s_page_text[1] = '/';
    s_page_text[2] = '0' + APPS_PAGE_COUNT;
    s_page_text[3] = '\0';
    ui_label_set_text(&lbl_page, s_page_text);

    ui_widget_set_visible((ui_widget_t *)&btn_prev, s_current_page > 0);
    ui_widget_set_visible((ui_widget_t *)&btn_next, s_current_page < APPS_PAGE_COUNT - 1);

    /* Reset focus */
    apps_clear_focus();
}

static void prev_page_click(ui_widget_t *w)
{
    (void)w;
    if (s_current_page > 0) {
        s_current_page--;
        apps_update_grid();
        ui_page_invalidate_all();
    }
}

static void next_page_click(ui_widget_t *w)
{
    (void)w;
    if (s_current_page < APPS_PAGE_COUNT - 1) {
        s_current_page++;
        apps_update_grid();
        ui_page_invalidate_all();
    }
}

/*=============================================================================
 *  App Button Callback
 *=============================================================================*/

static void app_button_click(ui_widget_t *w)
{
    int app_idx = (int)(intptr_t)w->user_data;
    if (app_idx >= 0 && app_idx < APPS_TOTAL) {
        ui_page_t *target = s_apps[app_idx].get_page();
        if (target) {
            ui_page_push(target);
        }
    }
}

/*=============================================================================
 *  Keyboard Event Handler (page-level)
 *=============================================================================*/

static bool apps_handle_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        int count = apps_on_page();
        if (s_focus_idx < 0) {
            apps_set_focus(0);
        } else if (s_focus_idx + APPS_GRID_COLS < count) {
            apps_set_focus(s_focus_idx + APPS_GRID_COLS);
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_focus_idx < 0) {
            apps_set_focus(0);
        } else if (s_focus_idx >= APPS_GRID_COLS) {
            apps_set_focus(s_focus_idx - APPS_GRID_COLS);
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_RIGHT_ARROW) {
        int count = apps_on_page();
        if (s_focus_idx < 0) {
            apps_set_focus(0);
        } else if (s_focus_idx + 1 < count) {
            /* Check we stay in the same row */
            int cur_row = s_focus_idx / APPS_GRID_COLS;
            int new_row = (s_focus_idx + 1) / APPS_GRID_COLS;
            if (new_row == cur_row) {
                apps_set_focus(s_focus_idx + 1);
            }
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_LEFT_ARROW) {
        if (s_focus_idx < 0) {
            apps_set_focus(0);
        } else if (s_focus_idx > 0) {
            /* Check we stay in the same row */
            int cur_row = s_focus_idx / APPS_GRID_COLS;
            int new_row = (s_focus_idx - 1) / APPS_GRID_COLS;
            if (new_row == cur_row) {
                apps_set_focus(s_focus_idx - 1);
            }
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_OK) {
        if (s_focus_idx >= 0 && s_focus_idx < apps_on_page()) {
            app_button_click((ui_widget_t *)&btn_apps[s_focus_idx]);
            return true;
        }
        /* No focus: set focus to first item */
        apps_set_focus(0);
        return true;
    }

    return false;
}

/*=============================================================================
 *  Apps Page Initialization
 *=============================================================================*/

void ui_apps_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t title_rect = {cx, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Apps", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    ui_rect_t page_rect = {cx + 400, 24, 60, 20};
    ui_label_init(&lbl_page, &page_rect, "1/2", &font_montserrat_12);
    ui_label_set_color(&lbl_page, UI_COLOR_TEXT_SECONDARY);

    ui_rect_t prev_rect = {cx + 350, 20, 40, 28};
    ui_button_init(&btn_prev, &prev_rect, "<", &font_montserrat_12);
    ui_button_set_callback(&btn_prev, prev_page_click);
    ui_button_set_colors(&btn_prev, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
    btn_prev.radius = 14;

    ui_rect_t next_rect = {cx + 470, 20, 40, 28};
    ui_button_init(&btn_next, &next_rect, ">", &font_montserrat_12);
    ui_button_set_callback(&btn_next, next_page_click);
    ui_button_set_colors(&btn_next, UI_COLOR_BG_CARD, UI_COLOR_SECONDARY, UI_COLOR_TEXT_PRIMARY);
    btn_next.radius = 14;

    s_apps_widgets[0] = (ui_widget_t *)&lbl_title;
    s_apps_widgets[1] = (ui_widget_t *)&lbl_page;
    s_apps_widgets[2] = (ui_widget_t *)&btn_prev;
    s_apps_widgets[3] = (ui_widget_t *)&btn_next;

    int16_t grid_x_start = cx + 10;
    int16_t grid_y_start = APPS_GRID_TOP;

    for (int i = 0; i < APPS_PER_PAGE; i++) {
        int16_t col = i % APPS_GRID_COLS;
        int16_t row = i / APPS_GRID_COLS;

        ui_rect_t btn_rect = {
            grid_x_start + col * (APP_BTN_W + APP_BTN_GAP_X),
            grid_y_start + row * (APP_BTN_H + APP_BTN_GAP_Y),
            APP_BTN_W,
            APP_BTN_H
        };

        ui_icon_button_init(&btn_apps[i], &btn_rect,
                            NULL, 0, 0, "", &font_montserrat_12);
        ui_icon_button_set_callback(&btn_apps[i], app_button_click);

        s_apps_widgets[4 + i] = (ui_widget_t *)&btn_apps[i];
    }

    apps_update_grid();

    ui_page_set_widgets(&page_apps, s_apps_widgets, 4 + APPS_PER_PAGE);
    ui_page_set_callbacks(&page_apps, ui_apps_enter, NULL, NULL, NULL);
    ui_page_set_event_cb(&page_apps, apps_handle_event);
}

void ui_apps_enter(ui_page_t *page)
{
    (void)page;
    s_current_page = 0;
    apps_update_grid();
}
