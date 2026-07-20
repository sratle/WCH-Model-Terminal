/********************************** (C) COPYRIGHT *******************************
* File Name          : games.c
* Author             : E-ink Model Team
* Version            : V2.0.0
* Date               : 2026/07/19
* Description        : Game module initialization (2048 + Minesweeper only).
********************************************************************************/
#include "games.h"
#include "game_2048.h"
#include "game_minesweeper.h"

void games_init_all(void)
{
    game_2048_init();
    game_minesweeper_init();
}
