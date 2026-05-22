/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_apps.h
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : Apps page header.
*                      16 apps in a single 4x4 grid (no pagination needed).
********************************************************************************/
#ifndef __UI_APPS_H
#define __UI_APPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_apps_init(void);
void ui_apps_enter(ui_page_t *page);

#ifdef __cplusplus
}
#endif

#endif /* __UI_APPS_H */
