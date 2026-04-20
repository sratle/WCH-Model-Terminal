/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_app_common.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : Common fullscreen app/game page template.
*                      Provides back button, title bar, and ESC handling.
********************************************************************************/
#ifndef __UI_APP_COMMON_H
#define __UI_APP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

/*=============================================================================
 *  Fullscreen Page Configuration
 *=============================================================================*/

#define APP_TITLE_BAR_H     40

/*=============================================================================
 *  Fullscreen App Page Structure
 *=============================================================================*/

typedef struct {
    ui_page_t page;
    ui_button_t btn_back;
    ui_label_t lbl_title;
    ui_widget_t *widgets[2];
} ui_app_page_t;

/*=============================================================================
 *  Common Fullscreen Page API
 *=============================================================================*/

void ui_app_page_init(ui_app_page_t *app, const char *name, uint32_t id);
void ui_app_page_draw(ui_page_t *page, ui_rect_t *dirty);

#ifdef __cplusplus
}
#endif

#endif /* __UI_APP_COMMON_H */
