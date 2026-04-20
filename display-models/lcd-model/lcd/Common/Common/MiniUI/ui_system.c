/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_system.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : MiniUI system initialization and main loop.
********************************************************************************/
#include "miniui.h"
#include "../UI/ui_main.h"
#include "../UI/ui_home.h"
#include "../UI/ui_apps.h"
#include "../UI/ui_models.h"
#include "../UI/ui_settings.h"
#include "../UI/ui_games.h"
#include "../Apps/apps.h"
#include "../Games/games.h"
#include "../SSD1963/ssd1963.h"
#include "../SSD1963/lcd_config.h"

/*=============================================================================
 *  UI System Initialization
 *=============================================================================*/

void UI_Init(void)
{
    LCD_Init(LCD_ORIENTATION_NORMAL);
    SSD1963_Init();

    ui_render_init();
    ui_page_init();
    ui_input_init();
    ui_anim_init();

    ui_page_set_sidebar_callbacks(ui_main_draw_sidebar, ui_main_handle_event);

    ui_main_init();
    ui_home_init();
    ui_apps_init();
    ui_models_init();
    ui_settings_init();
    ui_games_init();

    apps_init_all();
    games_init_all();

    ui_page_switch(&page_home);

    UI_FullRefresh();
}

/*=============================================================================
 *  Main UI Tick
 *=============================================================================*/

void UI_Tick(void)
{
    ui_event_t *e = ui_input_poll();
    while (e) {
        ui_widget_t *capture = ui_input_get_capture();

        if (capture) {
            ui_widget_event(capture, e);
            if (e->type == UI_EVENT_RELEASE) {
                ui_input_set_capture(NULL);
            }
        } else {
            bool handled = false;

            if (e->type == UI_EVENT_KEY_BACK && ui_page_can_go_back()) {
                ui_page_pop();
                handled = true;
            }

            if (!handled) {
                ui_page_t *page = ui_page_current();
                bool is_fullscreen = page && (page->flags & UI_PAGE_FLAG_FULLSCREEN);

                if (!is_fullscreen) {
                    ui_sidebar_event_cb_t sidebar_event = ui_page_get_sidebar_event_cb();
                    if (sidebar_event) {
                        handled = sidebar_event(e);
                    }
                }
            }

            if (!handled) {
                ui_page_t *page = ui_page_current();
                if (page) {
                    if (e->type == UI_EVENT_PRESS || e->type == UI_EVENT_RELEASE) {
                        for (int16_t i = page->widget_count - 1; i >= 0; i--) {
                            if (page->widgets[i] &&
                                ui_widget_hit_test(page->widgets[i], e->pos.x, e->pos.y)) {
                                ui_widget_event(page->widgets[i], e);
                                if (e->type == UI_EVENT_PRESS) {
                                    ui_input_set_capture(page->widgets[i]);
                                }
                                handled = true;
                                break;
                            }
                        }
                    } else if (e->type == UI_EVENT_DRAG) {
                        for (int16_t i = page->widget_count - 1; i >= 0; i--) {
                            if (page->widgets[i] &&
                                ui_widget_hit_test(page->widgets[i], e->pos.x, e->pos.y)) {
                                ui_widget_event(page->widgets[i], e);
                                ui_input_set_capture(page->widgets[i]);
                                handled = true;
                                break;
                            }
                        }
                    } else {
                        for (uint16_t i = 0; i < page->widget_count; i++) {
                            if (page->widgets[i]) {
                                ui_widget_event(page->widgets[i], e);
                            }
                        }
                    }
                }
            }
        }

        e = ui_input_poll();
    }

    ui_anim_tick(33);
    ui_page_draw();
}

/*=============================================================================
 *  Full Screen Refresh
 *=============================================================================*/

void UI_FullRefresh(void)
{
    ui_screen_clear(UI_COLOR_BG_MAIN);

    ui_page_t *page = ui_page_current();
    bool is_fullscreen = page && (page->flags & UI_PAGE_FLAG_FULLSCREEN);

    if (!is_fullscreen) {
        ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
        ui_main_draw_sidebar(&full);
    }

    ui_page_invalidate_all();
    ui_page_draw();
}
