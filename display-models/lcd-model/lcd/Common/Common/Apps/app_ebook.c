#include "app_ebook.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_ebook;

void app_ebook_init(void)
{
    ui_app_page_init(&s_app_ebook, "EBook", 0x10D);
    ui_page_register(&s_app_ebook.page);
}

ui_page_t *app_ebook_get_page(void)
{
    return &s_app_ebook.page;
}
