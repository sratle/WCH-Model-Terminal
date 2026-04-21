#include "game_snake.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_game_snake;

void game_snake_init(void)
{
    ui_app_page_init(&s_game_snake, "Snake", 0x202);
    ui_page_register(&s_game_snake.page);
}

ui_page_t *game_snake_get_page(void)
{
    return &s_game_snake.page;
}
