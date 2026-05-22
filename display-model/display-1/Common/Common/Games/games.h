#ifndef __GAMES_H
#define __GAMES_H

#include "../MiniUI/miniui.h"

void games_init_all(void);

ui_page_t *game_tetris_get_page(void);
ui_page_t *game_2048_get_page(void);
ui_page_t *game_snake_get_page(void);
ui_page_t *game_breakout_get_page(void);
ui_page_t *game_airplane_get_page(void);
ui_page_t *game_touchball_get_page(void);
ui_page_t *game_minesweeper_get_page(void);
ui_page_t *game_contra_get_page(void);

#endif
