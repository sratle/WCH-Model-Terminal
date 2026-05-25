/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_splash.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/05/25
* Description        : System splash screen implementation.
*                      Fullscreen startup page: shows logo + "initing...."
*                      for 3 seconds, then auto-switches to Home page.
********************************************************************************/
#include "ui_splash.h"
#include "ui_main.h"
#include <stdio.h>

#define SPLASH_DURATION_MS   3000

ui_page_t page_splash;

static uint32_t s_start_ms = 0;
static bool     s_switched  = false;

/* Animated dots for "initing" text */
static int      s_dot_count = 0;
static uint32_t s_dot_timer = 0;

/*=============================================================================
 *  Splash Draw Callback — fullscreen custom rendering
 *=============================================================================*/

static void splash_on_draw(ui_page_t *page, ui_rect_t *batch)
{
    (void)page;

    /* Dark background */
    static bool bg_cleared = false;
    if (!bg_cleared) {
        ui_screen_clear(UI_COLOR_BG_MAIN);
        bg_cleared = true;
    }

    /* Logo: "ELAB" centered, large (use 16pt font, drawn at center) */
    {
        int16_t logo_w = ui_text_width("ELAB", &font_montserrat_16);
        int16_t logo_x = (UI_SCREEN_WIDTH - logo_w) / 2;
        int16_t logo_y = UI_SCREEN_HEIGHT / 2 - 40;
        ui_draw_text(logo_x, logo_y, "ELAB", &font_montserrat_16, UI_COLOR_PRIMARY);
    }

    /* Subtitle: "Modular Terminal" below logo */
    {
        int16_t sub_w = ui_text_width("Modular Terminal", &font_montserrat_12);
        int16_t sub_x = (UI_SCREEN_WIDTH - sub_w) / 2;
        int16_t sub_y = UI_SCREEN_HEIGHT / 2 - 10;
        ui_draw_text(sub_x, sub_y, "Modular Terminal", &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
    }

    /* Init status: "initing" + animated dots */
    {
        char init_text[20];
        int n = snprintf(init_text, sizeof(init_text), "initing");
        for (int i = 0; i < s_dot_count && n < (int)sizeof(init_text) - 1; i++) {
            init_text[n++] = '.';
        }
        init_text[n] = '\0';

        int16_t init_w = ui_text_width(init_text, &font_montserrat_12);
        int16_t init_x = (UI_SCREEN_WIDTH - init_w) / 2;
        int16_t init_y = UI_SCREEN_HEIGHT / 2 + 30;
        ui_draw_text(init_x, init_y, init_text, &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
    }
}

/*=============================================================================
 *  Splash Update Callback — time-based logic (timer + dot animation)
 *=============================================================================*/

static void splash_on_update(ui_page_t *page)
{
    (void)page;

    if (s_switched) return;

    uint32_t now = ui_get_real_ms();

    /* Animate dots every 500ms */
    if (now - s_dot_timer >= 500) {
        s_dot_timer = now;
        s_dot_count++;
        if (s_dot_count > 4) s_dot_count = 1;
        ui_page_invalidate_all();
    }

    /* Check if splash duration elapsed */
    if (now - s_start_ms >= SPLASH_DURATION_MS) {
        s_switched = true;
        ui_page_switch(&page_home);
    }
}

/*=============================================================================
 *  Splash Enter Callback
 *=============================================================================*/

static void splash_on_enter(ui_page_t *page)
{
    (void)page;

    s_start_ms  = ui_get_real_ms();
    s_dot_timer = s_start_ms;
    s_dot_count = 1;
    s_switched  = false;
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void ui_splash_init(void)
{
    ui_page_struct_init_fullscreen(&page_splash, "Splash", 99);

    ui_page_set_callbacks(&page_splash, splash_on_enter, NULL,
                          splash_on_draw, NULL);
    ui_page_set_update_cb(&page_splash, splash_on_update);

    ui_page_register(&page_splash);
}
