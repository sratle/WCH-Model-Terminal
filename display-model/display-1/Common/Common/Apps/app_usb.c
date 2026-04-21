#include "app_usb.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_usb;

void app_usb_init(void)
{
    ui_app_page_init(&s_app_usb, "USB", 0x104);
    ui_page_register(&s_app_usb.page);
}

ui_page_t *app_usb_get_page(void)
{
    return &s_app_usb.page;
}
