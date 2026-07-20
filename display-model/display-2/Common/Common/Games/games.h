/********************************** (C) COPYRIGHT *******************************
* File Name          : games.h
* Author             : E-ink Model Team
* Version            : V2.0.0
* Date               : 2026/07/19
* Description        : Game module header.
*                      Trimmed for E-ink resource budget: only 2048 and
*                      Minesweeper are kept (turn-based, partial-refresh
*                      friendly).  Action games were removed to free FLASH.
********************************************************************************/
#ifndef __GAMES_H
#define __GAMES_H

#include "../MiniUI/miniui.h"

void games_init_all(void);

ui_page_t *game_2048_get_page(void);
ui_page_t *game_minesweeper_get_page(void);

#endif
