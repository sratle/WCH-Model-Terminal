#include "games.h"
#include "game_best.h"
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
    game_best_init();
    game_tetris_init();
    game_2048_init();
    game_snake_init();
    game_breakout_init();
    game_airplane_init();
    game_touchball_init();
    game_minesweeper_init();
    game_contra_init();
}
