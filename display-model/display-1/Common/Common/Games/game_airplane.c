#include "game_airplane.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_game_airplane;

void game_airplane_init(void)
{
    ui_app_page_init(&s_game_airplane, "Airplane", 0x201);
    ui_page_register(&s_game_airplane.page);
}

ui_page_t *game_airplane_get_page(void)
{
    return &s_game_airplane.page;
}
