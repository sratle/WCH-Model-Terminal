/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_system.c
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/22
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
#include "../UART/uart_module.h"

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
 *  Helper: check if event is a pointer-type event (has position)
 *=============================================================================*/

static bool is_pointer_event(ui_event_type_t type)
{
    return type == UI_EVENT_DOWN || type == UI_EVENT_UP ||
           type == UI_EVENT_MOVE || type == UI_EVENT_CLICK ||
           type == UI_EVENT_DOUBLE_CLICK || type == UI_EVENT_LONG_PRESS ||
           type == UI_EVENT_LONG_PRESS_REPEAT ||
           type == UI_EVENT_SWIPE_UP || type == UI_EVENT_SWIPE_DOWN ||
           type == UI_EVENT_SWIPE_LEFT || type == UI_EVENT_SWIPE_RIGHT ||
           type == UI_EVENT_PRESS_CANCEL;
}

/*=============================================================================
 *  Main UI Tick
 *=============================================================================*/

void UI_Tick(void)
{
    /* Process time-based gestures (long-press, click timeout) */
    ui_input_tick();

    /* Poll and dispatch events */
    ui_event_t *e = ui_input_poll();
    while (e) {
        ui_widget_t *capture = ui_input_get_capture();
        ui_page_t *page_before = ui_page_current();  /* Track page changes */

        if (capture) {
            /* Capture widget receives all events until release/cancel */
            ui_widget_event(capture, e);

            /* If the event handler triggered a page change (e.g. ui_page_push
             * inside a button's on_click), skip all further processing for this
             * event. The old capture widget was on the old page; broadcasting or
             * setting capture on the new page would cause spurious interactions. */
            if (ui_page_current() != page_before) {
                e = ui_input_poll();
                continue;
            }

            /* Release capture on UP or swipe (pointer events) */
            if (e->type == UI_EVENT_UP || e->type == UI_EVENT_CLICK ||
                e->type == UI_EVENT_DOUBLE_CLICK) {
                if (e->touch_id == ui_input_get_capture_touch_id() ||
                    e->source != UI_INPUT_TOUCH) {
                    ui_input_set_capture(NULL, UI_TOUCH_ID_NONE);
                }
            } else if (e->type == UI_EVENT_SWIPE_UP || e->type == UI_EVENT_SWIPE_DOWN ||
                       e->type == UI_EVENT_SWIPE_LEFT || e->type == UI_EVENT_SWIPE_RIGHT) {
                ui_event_t cancel_e = *e;
                cancel_e.type = UI_EVENT_PRESS_CANCEL;
                ui_widget_event(capture, &cancel_e);
                ui_input_set_capture(NULL, UI_TOUCH_ID_NONE);
            }

            /* Multi-touch: broadcast DOWN/UP (NOT MOVE) to all widgets for
             * multi-finger interaction (e.g., game buttons). */
            if (e->source == UI_INPUT_TOUCH &&
                e->touch_id != ui_input_get_capture_touch_id() &&
                (e->type == UI_EVENT_DOWN || e->type == UI_EVENT_UP)) {
                ui_page_t *page = ui_page_current();
                if (page) {
                    for (uint16_t i = 0; i < page->widget_count; i++) {
                        if (page->widgets[i]) {
                            ui_widget_event(page->widgets[i], e);
                        }
                    }
                }
            }
        } else {
            bool handled = false;

            /* Handle BACK key globally */
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
                if (!page) {
                    e = ui_input_poll();
                    continue;
                }

                if (is_pointer_event(e->type)) {
                    /* Pointer events: hit-test widgets in reverse Z-order */
                    for (int16_t i = page->widget_count - 1; i >= 0; i--) {
                        if (page->widgets[i] &&
                            ui_widget_hit_test(page->widgets[i], e->pos.x, e->pos.y)) {
                            ui_widget_event(page->widgets[i], e);

                            /* Skip capture if page changed during event handling */
                            if (ui_page_current() != page) break;

                            if (e->type == UI_EVENT_DOWN) {
                                ui_input_set_capture(page->widgets[i],
                                    e->source == UI_INPUT_TOUCH ? e->touch_id : UI_TOUCH_ID_NONE);
                            }
                            handled = true;
                            break;
                        }
                    }
                } else {
                    /* Key / other events: broadcast to all widgets */
                    for (uint16_t i = 0; i < page->widget_count; i++) {
                        if (page->widgets[i]) {
                            ui_widget_event(page->widgets[i], e);
                        }
                    }
                }
            }
        }

        e = ui_input_poll();
    }

    ui_anim_tick(UI_TICK_MS);

    /* Per-frame page update (game logic, time-based animations, etc.) */
    {
        ui_page_t *update_page = ui_page_current();
        if (update_page && update_page->on_update) {
            update_page->on_update(update_page);
        }
    }

    /* Invalidate mouse cursor dirty regions before rendering */
    ui_input_invalidate_cursor();

    /* Unified compositing render path for all page types (UI, apps, games).
     * The page manager handles dirty region tracking, widget compositing,
     * and GRAM flushing automatically. */
    ui_page_draw();
}

/*=============================================================================
 *  Full Screen Refresh
 *=============================================================================*/

void UI_FullRefresh(void)
{
    /* Clear GRAM to background color */
    ui_screen_clear(UI_COLOR_BG_MAIN);

    /* Mark entire screen dirty and let compositing renderer handle it */
    ui_page_invalidate_all();
    ui_page_draw();
}
