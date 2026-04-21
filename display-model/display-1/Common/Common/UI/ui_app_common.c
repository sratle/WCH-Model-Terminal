/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_app_common.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : Common fullscreen app/game page template implementation.
********************************************************************************/
#include "ui_app_common.h"
#include "../MiniUI/miniui_page.h"
#include <string.h>

/*=============================================================================
 *  Back Button Callback
 *=============================================================================*/

static void app_back_click(ui_widget_t *w)
{
    (void)w;
    if (ui_page_can_go_back()) {
        ui_page_pop();
    }
}

/*=============================================================================
 *  Common Fullscreen Page Draw
 *=============================================================================*/

void ui_app_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&full, UI_COLOR_BG_MAIN);

    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);
}

/*=============================================================================
 *  Common Fullscreen Page Init
 *=============================================================================*/

void ui_app_page_init(ui_app_page_t *app, const char *name, uint32_t id)
{
    memset(app, 0, sizeof(ui_app_page_t));

    ui_page_struct_init_fullscreen(&app->page, name, id);

    ui_rect_t back_rect = {8, 6, 50, 28};
    ui_button_init(&app->btn_back, &back_rect, "<", &font_montserrat_16);
    ui_button_set_callback(&app->btn_back, app_back_click);
    ui_button_set_colors(&app->btn_back, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    app->btn_back.radius = 14;

    ui_rect_t title_rect = {68, 8, 300, 24};
    ui_label_init(&app->lbl_title, &title_rect, name, &font_montserrat_16);
    ui_label_set_color(&app->lbl_title, UI_COLOR_WHITE);
    app->lbl_title.base.bg_color = UI_COLOR_TRANSPARENT;

    app->widgets[0] = (ui_widget_t *)&app->btn_back;
    app->widgets[1] = (ui_widget_t *)&app->lbl_title;

    ui_page_set_widgets(&app->page, app->widgets, 2);
    ui_page_set_callbacks(&app->page, NULL, NULL, ui_app_page_draw, NULL);
}
