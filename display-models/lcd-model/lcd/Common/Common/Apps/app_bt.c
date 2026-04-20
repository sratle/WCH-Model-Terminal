#include "app_bt.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_bt;

void app_bt_init(void)
{
    ui_app_page_init(&s_app_bt, "BT", 0x106);
    ui_page_register(&s_app_bt.page);
}

ui_page_t *app_bt_get_page(void)
{
    return &s_app_bt.page;
}
