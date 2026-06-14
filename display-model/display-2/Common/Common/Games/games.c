/********************************** (C) COPYRIGHT *******************************
* File Name          : games.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : Game module initialization.
*                      Adapted from Display-1 for E-ink display.
********************************************************************************/
#include "games.h"
#include "game_tetris.h"
#include "game_2048.h"
#include "game_snake.h"
#include "game_breakout.h"
#include "game_airplane.h"
#include "game_touchball.h"
#include "game_minesweeper.h"
#include "game_contra.h"

void games_init_all(void)
{
    game_tetris_init();
    game_2048_init();
    game_snake_init();
    game_breakout_init();
    game_airplane_init();
    game_touchball_init();
    game_minesweeper_init();
    game_contra_init();
}
