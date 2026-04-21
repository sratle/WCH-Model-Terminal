/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_games.h
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : Games page header.
*                      4 games in a single page grid.
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
