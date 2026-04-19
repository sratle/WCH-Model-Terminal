/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_software.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Software page header.
*                      App grid with icon buttons.
********************************************************************************/
#ifndef __UI_SOFTWARE_H
#define __UI_SOFTWARE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_software_init(void);
void ui_software_enter(ui_page_t *page);
void ui_software_draw(ui_page_t *page, ui_rect_t *dirty);

#ifdef __cplusplus
}
#endif

#endif /* __UI_SOFTWARE_H */
