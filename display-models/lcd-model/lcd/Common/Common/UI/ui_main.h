/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_main.h
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : Main UI framework header.
*                      Sidebar navigation and page container.
********************************************************************************/
#ifndef __UI_MAIN_H
#define __UI_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

/*=============================================================================
 *  Sidebar Configuration
 *=============================================================================*/

#define SIDEBAR_WIDTH       200
#define SIDEBAR_ITEM_HEIGHT 60
#define SIDEBAR_ITEM_COUNT  5

/*=============================================================================
 *  Menu Items
 *=============================================================================*/

typedef enum {
    MENU_HOME = 0,
    MENU_APPS,
    MENU_GAMES,
    MENU_MODELS,
    MENU_SETTINGS,
} menu_item_t;

/*=============================================================================
 *  Main UI API
 *=============================================================================*/

void ui_main_init(void);
void ui_main_set_menu(menu_item_t item);
menu_item_t ui_main_get_menu(void);
void ui_main_draw_sidebar(ui_rect_t *dirty);
bool ui_main_handle_event(ui_event_t *e);

/*=============================================================================
 *  External Page References
 *=============================================================================*/

extern ui_page_t page_home;
extern ui_page_t page_apps;
extern ui_page_t page_games;
extern ui_page_t page_models;
extern ui_page_t page_settings;

#ifdef __cplusplus
}
#endif

#endif /* __UI_MAIN_H */
