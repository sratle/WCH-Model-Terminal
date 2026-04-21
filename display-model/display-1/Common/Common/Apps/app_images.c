#include "app_images.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_images;

void app_images_init(void)
{
    ui_app_page_init(&s_app_images, "Images", 0x103);
    ui_page_register(&s_app_images.page);
}

ui_page_t *app_images_get_page(void)
{
    return &s_app_images.page;
}
