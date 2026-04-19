/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_models.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Models page header.
*                      Tab view with module status cards.
********************************************************************************/
#ifndef __UI_MODELS_H
#define __UI_MODELS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_models_init(void);
void ui_models_enter(ui_page_t *page);
void ui_models_draw(ui_page_t *page, ui_rect_t *dirty);

#ifdef __cplusplus
}
#endif

#endif /* __UI_MODELS_H */
