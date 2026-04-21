#include "game_tetris.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_game_tetris;

void game_tetris_init(void)
{
    ui_app_page_init(&s_game_tetris, "Tetris", 0x200);
    ui_page_register(&s_game_tetris.page);
}

ui_page_t *game_tetris_get_page(void)
{
    return &s_game_tetris.page;
}
