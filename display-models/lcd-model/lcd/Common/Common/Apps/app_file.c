#include "app_file.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_file;

void app_file_init(void)
{
    ui_app_page_init(&s_app_file, "Files", 0x101);
    ui_page_register(&s_app_file.page);
}

ui_page_t *app_file_get_page(void)
{
    return &s_app_file.page;
}
