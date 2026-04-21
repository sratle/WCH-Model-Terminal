/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_settings.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Settings page header.
*                      Settings list with slider/switch/button controls.
********************************************************************************/
#ifndef __UI_SETTINGS_H
#define __UI_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_settings_init(void);
void ui_settings_enter(ui_page_t *page);
void ui_settings_draw(ui_page_t *page, ui_rect_t *dirty);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SETTINGS_H */
