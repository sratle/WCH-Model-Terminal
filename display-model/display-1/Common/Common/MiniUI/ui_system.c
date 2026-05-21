/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_system.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : MiniUI system initialization and main loop.
********************************************************************************/
#include "miniui.h"
#include "debug.h"
#include "../settings.h"
#include "../UI/ui_main.h"
#include "../UI/ui_home.h"
#include "../UI/ui_apps.h"
#include "../UI/ui_models.h"
#include "../UI/ui_settings.h"
#include "../UI/ui_games.h"
#include "../Apps/apps.h"
#include "../Games/games.h"
#include "../FMC/fmc_driver.h"
#include "../SSD1963/ssd1963.h"
#include "../SSD1963/lcd_config.h"

#define UI_TICK_MS  10

/* Cycle-to-millisecond conversion for real-time measurements */
static uint32_t s_cycles_per_ms = 0;

/*=============================================================================
 *  Real-Time Millisecond Counter (via RISC-V mcycle CSR)
 *=============================================================================*/

static void ui_cycle_timer_init(void)
{
    s_cycles_per_ms = SystemCoreClock / 1000;
    if (s_cycles_per_ms == 0) s_cycles_per_ms = 400000;
}

uint32_t ui_get_real_ms(void)
{
    return __get_UCYCLE() / s_cycles_per_ms;
}

/*=============================================================================
 *  UI System Initialization
 *=============================================================================*/

void UI_Init(void)
{
    printf("[UI_Init] start\r\n");

    // printf("[UI_Init] -> FMC_Driver_Init()\r\n");
    // FMC_Driver_Init();

    printf("[UI_Init] -> LCD_Init()\r\n");
    LCD_Init(LCD_ORIENTATION_NORMAL);

    printf("[UI_Init] -> SSD1963_Init()\r\n");
    SSD1963_Init();

    printf("[UI_Init] -> settings_init()\r\n");
    settings_init();

    printf("[UI_Init] -> ui_render_init()\r\n");
    ui_render_init();
    printf("[UI_Init] -> ui_page_init()\r\n");
    ui_page_init();
    printf("[UI_Init] -> ui_input_init()\r\n");
    ui_input_init();
    printf("[UI_Init] -> ui_anim_init()\r\n");
    ui_anim_init();

    printf("[UI_Init] -> ui_cycle_timer_init()\r\n");
    ui_cycle_timer_init();

    ui_page_set_sidebar_callbacks(ui_main_draw_sidebar, ui_main_handle_event);
    ui_page_set_sidebar_width(SIDEBAR_WIDTH);

    printf("[UI_Init] -> ui_main_init()\r\n");
    ui_main_init();
    printf("[UI_Init] -> ui_home_init()\r\n");
    ui_home_init();
    printf("[UI_Init] -> ui_apps_init()\r\n");
    ui_apps_init();
    printf("[UI_Init] -> ui_models_init()\r\n");
    ui_models_init();
    printf("[UI_Init] -> ui_settings_init()\r\n");
    ui_settings_init();
    printf("[UI_Init] -> ui_games_init()\r\n");
    ui_games_init();

    printf("[UI_Init] -> apps_init_all()\r\n");
    apps_init_all();
    printf("[UI_Init] -> games_init_all()\r\n");
    games_init_all();

    printf("[UI_Init] -> ui_page_switch(&page_home)\r\n");
    ui_page_switch(&page_home);

    printf("[UI_Init] -> UI_FullRefresh()\r\n");
    UI_FullRefresh();

    printf("[UI_Init] done\r\n");
}

/*=============================================================================
 *  Main UI Tick
 *=============================================================================*/

void UI_Tick(void)
{
    /* Check for hold events before polling */
    ui_input_check_hold();

    ui_event_t *e = ui_input_poll();
    while (e) {
        ui_widget_t *capture = ui_input_get_capture();

        if (capture) {
            ui_widget_event(capture, e);
            if (e->type == UI_EVENT_RELEASE) {
                ui_input_set_capture(NULL);
            } else if (e->type == UI_EVENT_SWIPE_UP || e->type == UI_EVENT_SWIPE_DOWN ||
                       e->type == UI_EVENT_SWIPE_LEFT || e->type == UI_EVENT_SWIPE_RIGHT) {
                ui_event_t cancel_e = *e;
                cancel_e.type = UI_EVENT_PRESS_CANCEL;
                ui_widget_event(capture, &cancel_e);
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
                    } else if (e->type == UI_EVENT_DRAG || e->type == UI_EVENT_SWIPE_UP ||
                               e->type == UI_EVENT_SWIPE_DOWN || e->type == UI_EVENT_SWIPE_LEFT ||
                               e->type == UI_EVENT_SWIPE_RIGHT) {
                        for (int16_t i = page->widget_count - 1; i >= 0; i--) {
                            if (page->widgets[i] &&
                                ui_widget_hit_test(page->widgets[i], e->pos.x, e->pos.y)) {
                                ui_widget_event(page->widgets[i], e);
                                if (e->type == UI_EVENT_DRAG) {
                                    ui_input_set_capture(page->widgets[i]);
                                }
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

    ui_anim_tick(UI_TICK_MS);

    /* Auto-refresh game pages: call draw callback every frame.
     * Games manage their own dirty regions via ui_page_invalidate().
     * We skip ui_page_draw for games since we handle drawing here. */
    ui_page_t *draw_page = ui_page_current();
    if (draw_page && (draw_page->flags & UI_PAGE_FLAG_GAME)) {
        if (draw_page->on_draw) {
            ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
            draw_page->on_draw(draw_page, &full);
        }
        /* Skip ui_page_draw - game already drew what it needed */
        return;
    }

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
