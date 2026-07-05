/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_main.c
* Author             : E-ink Model Team
* Version            : V3.0.0
* Date               : 2026/06/12
* Description        : Main UI framework implementation.
*                      Sidebar navigation and page container.
*                      Adapted from Display-1 for E-ink display.
********************************************************************************/
#include "ui_main.h"
#include "ui_home.h"
#include "ui_apps.h"
#include "ui_games.h"
#include "ui_models.h"
#include "ui_settings.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>

/*=============================================================================
 *  Sidebar Data
 *=============================================================================*/

static const char *s_menu_labels[SIDEBAR_ITEM_COUNT] = {
    "Home",
    "Apps",
    "Games",
    "Models",
    "Settings"
};

static menu_item_t s_active_menu = MENU_HOME;

/*=============================================================================
 *  Page Instances
 *=============================================================================*/

ui_page_t page_home;
ui_page_t page_apps;
ui_page_t page_games;
ui_page_t page_models;
ui_page_t page_settings;

/*=============================================================================
 *  Sidebar Drawing
 *=============================================================================*/

void ui_main_draw_sidebar(ui_rect_t *dirty)
{
    (void)dirty;  /* Drawing primitives auto-clip to compositing target */

    ui_rect_t sidebar = {0, 0, SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&sidebar, UI_COLOR_BG_SIDEBAR);

    /* Divider line between sidebar and content area */
    ui_draw_vline(SIDEBAR_WIDTH - 1, 0, UI_SCREEN_HEIGHT, UI_COLOR_PRIMARY);

    /* Title block */
    ui_rect_t title_rect = {0, 20, SIDEBAR_WIDTH, 40};
    ui_draw_text_in_rect(&title_rect, "ELAB", &font_montserrat_16, UI_COLOR_PRIMARY, 1);

    /* Title underline */
    ui_draw_hline(0, 60, SIDEBAR_WIDTH, UI_COLOR_PRIMARY);

    for (int i = 0; i < SIDEBAR_ITEM_COUNT; i++) {
        ui_rect_t item_rect = {
            0,
            80 + i * SIDEBAR_ITEM_HEIGHT,
            SIDEBAR_WIDTH,
            SIDEBAR_ITEM_HEIGHT
        };

        if (i == (int)s_active_menu) {
            /* Active item: black indicator bar on left edge */
            ui_rect_t indicator = {0, item_rect.y, 4, SIDEBAR_ITEM_HEIGHT};
            ui_draw_fill_rect(&indicator, UI_COLOR_PRIMARY);

            ui_draw_text_in_rect(&item_rect, s_menu_labels[i], &font_montserrat_12, UI_COLOR_TEXT_PRIMARY, 1);
        } else {
            ui_draw_text_in_rect(&item_rect, s_menu_labels[i], &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);
        }

        /* Separator line below each item (except the last) */
        if (i < SIDEBAR_ITEM_COUNT - 1) {
            ui_draw_hline(8, item_rect.y + SIDEBAR_ITEM_HEIGHT - 1,
                          SIDEBAR_WIDTH - 16, UI_COLOR_PRIMARY);
        }
    }
}

/*=============================================================================
 *  Main UI Functions
 *=============================================================================*/

void ui_main_init(void)
{
    ui_page_struct_init(&page_home, "Home", 0);
    ui_page_struct_init(&page_apps, "Apps", 1);
    ui_page_struct_init(&page_games, "Games", 2);
    ui_page_struct_init(&page_models, "Models", 3);
    ui_page_struct_init(&page_settings, "Settings", 4);

    ui_page_register(&page_home);
    ui_page_register(&page_apps);
    ui_page_register(&page_games);
    ui_page_register(&page_models);
    ui_page_register(&page_settings);

    /* Register sidebar with page manager */
    ui_page_set_sidebar_callbacks(ui_main_draw_sidebar, ui_main_handle_event);
    ui_page_set_sidebar_width(SIDEBAR_WIDTH);

    /* Initialize sub-pages */
    ui_home_init();
    ui_apps_init();
    ui_games_init();
    ui_models_init();
    ui_settings_init();

    s_active_menu = MENU_HOME;
    ui_page_switch(&page_home);
}

void ui_main_set_menu(menu_item_t item)
{
    if (item >= SIDEBAR_ITEM_COUNT) return;
    if (item == s_active_menu) return;

    s_active_menu = item;

    switch (item) {
        case MENU_HOME:
            ui_page_switch(&page_home);
            break;
        case MENU_APPS:
            ui_page_switch(&page_apps);
            break;
        case MENU_GAMES:
            ui_page_switch(&page_games);
            break;
        case MENU_MODELS:
            ui_page_switch(&page_models);
            break;
        case MENU_SETTINGS:
            ui_page_switch(&page_settings);
            break;
    }

    ui_rect_t sidebar = {0, 0, SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_page_invalidate(&sidebar);
}

menu_item_t ui_main_get_menu(void)
{
    return s_active_menu;
}

bool ui_main_handle_event(ui_event_t *e)
{
    if (!e) return false;

    /* Helper to get pointer position from event */
    int16_t ex = 0, ey = 0;
    if (e->source == UI_INPUT_TOUCH) {
        ex = e->touch.x; ey = e->touch.y;
    } else if (e->source == UI_INPUT_MOUSE) {
        ex = e->mouse.pos.x; ey = e->mouse.pos.y;
    }

    if (e->type == UI_EVENT_CLICK || e->type == UI_EVENT_TOUCH_UP) {
        if (ex < SIDEBAR_WIDTH) {
            int item_idx = (ey - 80) / SIDEBAR_ITEM_HEIGHT;
            if (item_idx >= 0 && item_idx < SIDEBAR_ITEM_COUNT) {
                ui_main_set_menu((menu_item_t)item_idx);
            }
            return true;
        }
    }

    if (e->type == UI_EVENT_DOWN || e->type == UI_EVENT_TOUCH_DOWN) {
        if (ex < SIDEBAR_WIDTH) {
            return true;
        }
    }

    /* Swipe navigation: cycle pages */
    if (e->type == UI_EVENT_SWIPE_UP) {
        menu_item_t next = (menu_item_t)((s_active_menu + 1) % SIDEBAR_ITEM_COUNT);
        ui_main_set_menu(next);
        return true;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        menu_item_t prev = (menu_item_t)((s_active_menu + SIDEBAR_ITEM_COUNT - 1) % SIDEBAR_ITEM_COUNT);
        ui_main_set_menu(prev);
        return true;
    }

    return false;
}
