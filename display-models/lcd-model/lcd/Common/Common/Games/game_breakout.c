#include "game_breakout.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_game_breakout;

void game_breakout_init(void)
{
    ui_app_page_init(&s_game_breakout, "Breakout", 0x203);
    ui_page_register(&s_game_breakout.page);
}

ui_page_t *game_breakout_get_page(void)
{
    return &s_game_breakout.page;
}
