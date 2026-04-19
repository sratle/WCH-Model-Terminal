/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_main.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
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
#define SIDEBAR_ITEM_COUNT  4

/*=============================================================================
 *  Menu Items
 *=============================================================================*/

typedef enum {
    MENU_HOME = 0,
    MENU_SOFTWARE,
    MENU_MODELS,
    MENU_SETTINGS,
} menu_item_t;

/*=============================================================================
 *  Main UI API
 *=============================================================================*/

/* Initialize main UI framework */
void ui_main_init(void);

/* Set active menu item */
void ui_main_set_menu(menu_item_t item);

/* Get currently selected menu item */
menu_item_t ui_main_get_menu(void);

/* Draw sidebar */
void ui_main_draw_sidebar(ui_rect_t *dirty);

/* Handle sidebar input events */
void ui_main_handle_event(ui_event_t *e);

/*=============================================================================
 *  External Page References
 *=============================================================================*/

extern ui_page_t page_home;
extern ui_page_t page_software;
extern ui_page_t page_models;
extern ui_page_t page_settings;

#ifdef __cplusplus
}
#endif

#endif /* __UI_MAIN_H */
