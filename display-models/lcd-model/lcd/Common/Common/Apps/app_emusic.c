#include "app_emusic.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_emusic;

void app_emusic_init(void)
{
    ui_app_page_init(&s_app_emusic, "EMusic", 0x10E);
    ui_page_register(&s_app_emusic.page);
}

ui_page_t *app_emusic_get_page(void)
{
    return &s_app_emusic.page;
}
