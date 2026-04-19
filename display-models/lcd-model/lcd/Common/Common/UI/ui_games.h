/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_games.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Games page header.
*                      Game framework with fullscreen toggle.
********************************************************************************/
#ifndef __UI_GAMES_H
#define __UI_GAMES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_games_init(void);
void ui_games_enter(ui_page_t *page);
void ui_games_draw(ui_page_t *page, ui_rect_t *dirty);

#ifdef __cplusplus
}
#endif

#endif /* __UI_GAMES_H */
