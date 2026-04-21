#include "app_music.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_music;

void app_music_init(void)
{
    ui_app_page_init(&s_app_music, "Music", 0x100);
    ui_page_register(&s_app_music.page);
}

ui_page_t *app_music_get_page(void)
{
    return &s_app_music.page;
}
