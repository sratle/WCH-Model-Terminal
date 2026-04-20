#include "app_fingerprint.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_fingerprint;

void app_fingerprint_init(void)
{
    ui_app_page_init(&s_app_fingerprint, "Finger", 0x108);
    ui_page_register(&s_app_fingerprint.page);
}

ui_page_t *app_fingerprint_get_page(void)
{
    return &s_app_fingerprint.page;
}
