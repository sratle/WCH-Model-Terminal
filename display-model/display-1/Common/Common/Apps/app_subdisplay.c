#include "app_subdisplay.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_subdisplay;

void app_subdisplay_init(void)
{
    ui_app_page_init(&s_app_subdisplay, "SubDisp", 0x10A);
    ui_page_register(&s_app_subdisplay.page);
}

ui_page_t *app_subdisplay_get_page(void)
{
    return &s_app_subdisplay.page;
}
