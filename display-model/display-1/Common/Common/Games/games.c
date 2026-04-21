#include "games.h"
#include "game_tetris.h"
#include "game_2048.h"
#include "game_snake.h"
#include "game_breakout.h"

void games_init_all(void)
{
    game_tetris_init();
    game_2048_init();
    game_snake_init();
    game_breakout_init();
}
