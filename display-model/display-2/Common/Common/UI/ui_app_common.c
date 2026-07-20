/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_app_common.c
* Author             : E-ink Model Team
* Version            : V2.0.0
* Date               : 2026/06/12
* Description        : Common fullscreen app/game page template implementation.
*                      Provides back button, title bar, and ESC handling.
*                      Adapted from Display-1 for E-ink display.
********************************************************************************/
#include "ui_app_common.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>

/*=============================================================================
 *  Back Button Callback
 *=============================================================================*/

static void app_back_click(ui_widget_t *w)
{
    (void)w;
    ui_page_pop();
}

/*=============================================================================
 *  App Page Event Handler
 *=============================================================================*/

static bool app_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    /* ESC key (either event form) → go back */
    if (e->type == UI_EVENT_KEY_BACK) {
        ui_page_pop();
        return true;
    }
    if (e->type == UI_EVENT_KEY_CLICK && e->key.code == UI_KEY_BACK) {
        ui_page_pop();
        return true;
    }
    return false;
}

/*=============================================================================
 *  App Page Draw
 *=============================================================================*/

void ui_app_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)dirty;

    ui_app_page_t *app = (ui_app_page_t *)page;
    if (!app) return;

    /* Title bar background */
    ui_rect_t bar_rect = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar_rect, UI_COLOR_PRIMARY);

    /* Back button */
    ui_widget_draw((ui_widget_t *)&app->btn_back, dirty);

    /* Title text */
    ui_widget_draw((ui_widget_t *)&app->lbl_title, dirty);

    /* Separator line */
    ui_rect_t sep = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH, 1};
    ui_draw_fill_rect(&sep, UI_COLOR_TEXT_SECONDARY);
}

/*=============================================================================
 *  App Page Initialization
 *=============================================================================*/

void ui_app_page_init(ui_app_page_t *app, const char *name, uint32_t id)
{
    if (!app) return;

    ui_page_struct_init_fullscreen(&app->page, name, id);

    /* Back button */
    ui_rect_t back_rect = {8, 6, 60, 28};
    ui_button_init(&app->btn_back, &back_rect, "< Back", &font_montserrat_12);
    ui_button_set_callback(&app->btn_back, app_back_click);
    ui_button_set_colors(&app->btn_back, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    app->btn_back.radius = 4;

    /* Title label */
    ui_rect_t title_rect = {80, 8, UI_SCREEN_WIDTH - 90, 24};
    ui_label_init(&app->lbl_title, &title_rect, name, &font_montserrat_16);
    ui_label_set_color(&app->lbl_title, UI_COLOR_WHITE);
    app->lbl_title.base.bg_color = UI_COLOR_PRIMARY;

    /* Widget list */
    app->widgets[0] = (ui_widget_t *)&app->btn_back;
    app->widgets[1] = (ui_widget_t *)&app->lbl_title;
    ui_page_set_widgets(&app->page, app->widgets, 2);

    /* Set callbacks */
    ui_page_set_callbacks(&app->page, NULL, NULL, ui_app_page_draw, NULL);
    ui_page_set_event_cb(&app->page, app_page_event);

    /* Content rect below title bar */
    app->page.content_rect.x = 0;
    app->page.content_rect.y = APP_TITLE_BAR_H + 1;
    app->page.content_rect.w = UI_SCREEN_WIDTH;
    app->page.content_rect.h = UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - 1;
}
