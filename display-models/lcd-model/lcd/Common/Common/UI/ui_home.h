/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_home.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : Home page header.
*                      Date/time display and module status cards.
********************************************************************************/
#ifndef __UI_HOME_H
#define __UI_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

/*=============================================================================
 *  Home Page API
 *=============================================================================*/

/* Initialize home page */
void ui_home_init(void);

/* Enter home page callback */
void ui_home_enter(ui_page_t *page);

/* Draw home page callback */
void ui_home_draw(ui_page_t *page, ui_rect_t *dirty);

/* Handle home page events */
void ui_home_event(ui_widget_t *w, ui_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* __UI_HOME_H */
