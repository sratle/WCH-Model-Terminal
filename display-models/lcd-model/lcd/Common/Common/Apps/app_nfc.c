#include "app_nfc.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_nfc;

void app_nfc_init(void)
{
    ui_app_page_init(&s_app_nfc, "NFC", 0x107);
    ui_page_register(&s_app_nfc.page);
}

ui_page_t *app_nfc_get_page(void)
{
    return &s_app_nfc.page;
}
