/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_home.h
* Author             : LCD Model Team
* Version            : V2.0.0
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

void ui_home_init(void);
void ui_home_enter(ui_page_t *page);

#ifdef __cplusplus
}
#endif

#endif /* __UI_HOME_H */
