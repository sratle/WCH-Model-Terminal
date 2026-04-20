#include "game_2048.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_game_2048;

void game_2048_init(void)
{
    ui_app_page_init(&s_game_2048, "2048", 0x201);
    ui_page_register(&s_game_2048.page);
}

ui_page_t *game_2048_get_page(void)
{
    return &s_game_2048.page;
}
