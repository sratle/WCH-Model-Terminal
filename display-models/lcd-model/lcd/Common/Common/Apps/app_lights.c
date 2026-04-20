#include "app_lights.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_lights;

void app_lights_init(void)
{
    ui_app_page_init(&s_app_lights, "Lights", 0x10B);
    ui_page_register(&s_app_lights.page);
}

ui_page_t *app_lights_get_page(void)
{
    return &s_app_lights.page;
}
