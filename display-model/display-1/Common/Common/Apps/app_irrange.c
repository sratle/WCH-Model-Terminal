#include "app_irrange.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_irrange;

void app_irrange_init(void)
{
    ui_app_page_init(&s_app_irrange, "IRRange", 0x10C);
    ui_page_register(&s_app_irrange.page);
}

ui_page_t *app_irrange_get_page(void)
{
    return &s_app_irrange.page;
}
