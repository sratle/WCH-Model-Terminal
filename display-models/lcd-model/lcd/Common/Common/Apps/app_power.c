#include "app_power.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_power;

void app_power_init(void)
{
    ui_app_page_init(&s_app_power, "Power", 0x105);
    ui_page_register(&s_app_power.page);
}

ui_page_t *app_power_get_page(void)
{
    return &s_app_power.page;
}
