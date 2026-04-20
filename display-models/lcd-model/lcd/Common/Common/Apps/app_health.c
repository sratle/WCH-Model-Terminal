#include "app_health.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_health;

void app_health_init(void)
{
    ui_app_page_init(&s_app_health, "Health", 0x109);
    ui_page_register(&s_app_health.page);
}

ui_page_t *app_health_get_page(void)
{
    return &s_app_health.page;
}
