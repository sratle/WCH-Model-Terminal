#include "games.h"
#include "game_tetris.h"
#include "game_2048.h"
#include "game_snake.h"
#include "game_airplane.h"
#include "game_minesweeper.h"
#include "../UART/uart_module.h"

void games_init_all(void)
{
    game_tetris_init();
    game_2048_init();
    game_snake_init();
    game_airplane_init();
    game_minesweeper_init();
}

void games_rgb_wave(uint8_t direction, uint8_t speed)
{
    UART_SendRgbWave(direction, speed);
}

void games_rgb_ripple(uint8_t speed)
{
    UART_SendRgbRipple(speed);
}

void games_rgb_wave_dir(games_dir_t dir, uint8_t speed)
{
    /* 逻辑方向 → 波浪传播方向：向左输入 = 波浪从右向左传播 */
    static const uint8_t dir_map[4] = {
        RGB_WAVE_DIR_R2L,   /* GAMES_DIR_LEFT  */
        RGB_WAVE_DIR_L2R,   /* GAMES_DIR_RIGHT */
        RGB_WAVE_DIR_B2T,   /* GAMES_DIR_UP    */
        RGB_WAVE_DIR_T2B,   /* GAMES_DIR_DOWN  */
    };
    if (dir > GAMES_DIR_DOWN)
        return;
    UART_SendRgbWave(dir_map[dir], speed);
}
