#include "app_editor.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_editor;

void app_editor_init(void)
{
    ui_app_page_init(&s_app_editor, "Editor", 0x102);
    ui_page_register(&s_app_editor.page);
}

ui_page_t *app_editor_get_page(void)
{
    return &s_app_editor.page;
}
