/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_main.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : Main UI framework implementation.
*                      Sidebar navigation and page container.
********************************************************************************/
#include "ui_main.h"
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
    ui_rect_t sidebar = {0, 0, SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};

    ui_draw_fill_rect(&sidebar, UI_COLOR_BG_SIDEBAR);

    ui_rect_t title_rect = {0, 20, SIDEBAR_WIDTH, 40};
    ui_draw_text_in_rect(&title_rect, "ELAB", &font_montserrat_16, UI_COLOR_PRIMARY, 1);

    for (int i = 0; i < SIDEBAR_ITEM_COUNT; i++) {
        ui_rect_t item_rect = {
            0,
            80 + i * SIDEBAR_ITEM_HEIGHT,
            SIDEBAR_WIDTH,
            SIDEBAR_ITEM_HEIGHT
        };

        if (i == (int)s_active_menu) {
            ui_draw_fill_rect(&item_rect, UI_COLOR_SECONDARY);

            ui_rect_t indicator = {0, item_rect.y, 4, SIDEBAR_ITEM_HEIGHT};
            ui_draw_fill_rect(&indicator, UI_COLOR_PRIMARY);

            ui_draw_text_in_rect(&item_rect, s_menu_labels[i], &font_montserrat_12, UI_COLOR_TEXT_PRIMARY, 1);
        } else {
            ui_draw_text_in_rect(&item_rect, s_menu_labels[i], &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);
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

    s_active_menu = MENU_HOME;
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

    if (e->type == UI_EVENT_PRESS || e->type == UI_EVENT_RELEASE) {
        if (e->pos.x < SIDEBAR_WIDTH) {
            int item_idx = (e->pos.y - 80) / SIDEBAR_ITEM_HEIGHT;
            if (item_idx >= 0 && item_idx < SIDEBAR_ITEM_COUNT) {
                if (e->type == UI_EVENT_RELEASE) {
                    ui_main_set_menu((menu_item_t)item_idx);
                }
            }
            return true;
        }
    }

    return false;
}
