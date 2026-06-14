/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_splash.c
* Author             : E-ink Model Team
* Version            : V2.0.0
* Date               : 2026/06/12
* Description        : Splash screen implementation.
*                      Shows logo + version for 2 seconds, then transitions
*                      to the main page.
*                      Adapted from Display-1 for E-ink display.
********************************************************************************/
#include "ui_splash.h"
#include "ui_main.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>

/*=============================================================================
 *  Splash Configuration
 *=============================================================================*/

#define SPLASH_DURATION_MS   2000

/*=============================================================================
 *  Splash Page State
 *=============================================================================*/

ui_page_t page_splash;
static uint32_t s_splash_elapsed = 0;
static bool s_splash_done = false;

/*=============================================================================
 *  Splash Page Callbacks
 *=============================================================================*/

static void splash_on_enter(ui_page_t *page)
{
    (void)page;
    s_splash_elapsed = 0;
    s_splash_done = false;
}

static void splash_on_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    ui_screen_clear(UI_COLOR_WHITE);

    /* Draw "ELAB" logo centered */
    const char *logo = "ELAB";
    int16_t logo_w = ui_text_width(logo, &font_montserrat_16);
    int16_t logo_x = (UI_SCREEN_WIDTH - logo_w) / 2;
    int16_t logo_y = UI_SCREEN_HEIGHT / 2 - 40;
    ui_draw_text(logo_x, logo_y, logo, &font_montserrat_16, UI_COLOR_BLACK);

    /* Draw version string */
    const char *ver = "E-ink Display v2.0";
    int16_t ver_w = ui_text_width(ver, &font_montserrat_12);
    int16_t ver_x = (UI_SCREEN_WIDTH - ver_w) / 2;
    int16_t ver_y = logo_y + 40;
    ui_draw_text(ver_x, ver_y, ver, &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);

    /* Draw separator line */
    int16_t line_y = ver_y + 30;
    int16_t line_w = 120;
    ui_rect_t line_rect = {(UI_SCREEN_WIDTH - line_w) / 2, line_y, line_w, 1};
    ui_draw_fill_rect(&line_rect, UI_COLOR_TEXT_SECONDARY);

    /* Draw "Loading..." text */
    const char *loading = "Loading...";
    int16_t load_w = ui_text_width(loading, &font_montserrat_12);
    int16_t load_x = (UI_SCREEN_WIDTH - load_w) / 2;
    int16_t load_y = line_y + 20;
    ui_draw_text(load_x, load_y, loading, &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
}

static void splash_on_update(ui_page_t *page, uint32_t dt_ms)
{
    (void)page;
    if (s_splash_done) return;

    s_splash_elapsed += dt_ms;
    if (s_splash_elapsed >= SPLASH_DURATION_MS) {
        s_splash_done = true;

        ui_main_init();
        ui_page_switch(&page_home);
    }
}

/*=============================================================================
 *  Splash Page Initialization
 *=============================================================================*/

void ui_splash_init(void)
{
    ui_page_struct_init_fullscreen(&page_splash, "Splash", 100);
    ui_page_set_callbacks(&page_splash, splash_on_enter, NULL, splash_on_draw, NULL);
    ui_page_set_update_cb(&page_splash, splash_on_update);
    ui_page_register(&page_splash);
}
