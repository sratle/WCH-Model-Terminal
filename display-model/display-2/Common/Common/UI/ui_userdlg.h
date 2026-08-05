/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_userdlg.h
* Description        : User dialog page — PIN login (guest) or user info +
*                      logout (logged in). Pushed from sidebar user area.
*                      Adapted from Display-1 for E-ink display.
********************************************************************************/
#ifndef __UI_USERDLG_H
#define __UI_USERDLG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui_page.h"

void ui_userdlg_init(void);
ui_page_t *ui_userdlg_get_page(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_USERDLG_H */
