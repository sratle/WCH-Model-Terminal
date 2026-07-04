/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_system.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI system initialization and main loop for E-ink.
*                      Bridges MiniUI with the e-paper driver and provides
*                      the main tick/update/draw cycle.
********************************************************************************/
#include "miniui.h"
#include "../Eink/epaper.h"
#include "debug.h"
#include "ch32v30x.h"

/*=============================================================================
 *  System State
 *=============================================================================*/

static bool s_initialized = false;
static uint32_t s_last_tick = 0;

/*=============================================================================
 *  System Initialization
 *=============================================================================*/

void ui_system_init(void)
{
    printf("[ui_system] initializing...\r\n");

    /* 1. Initialize e-paper hardware */
    Epaper_Init();
    printf("[ui_system] e-paper initialized\r\n");

    /* 2. Clear screen to white */
    Epaper_ClearWhite();
    Epaper_Update();
    printf("[ui_system] screen cleared\r\n");

    /* 3. Initialize MiniUI modules */
    ui_render_init();
    ui_page_init();
    ui_input_init();
    printf("[ui_system] MiniUI modules initialized\r\n");

    s_last_tick = ui_get_real_ms();
    s_initialized = true;

    printf("[ui_system] ready\r\n");
}

/*=============================================================================
 *  System Main Loop
 *
 *  Call this from the main application loop.
 *  It handles: page update → page draw → input processing.
 *=============================================================================*/

void ui_system_tick(void)
{
    if (!s_initialized) return;

    /* Calculate delta time */
    uint32_t now = ui_get_real_ms();
    uint32_t dt = now - s_last_tick;
    s_last_tick = now;

    /* Update current page */
    ui_page_t *page = ui_page_current();
    if (page && page->on_update) {
        page->on_update(page, dt);
    }

    /* Draw dirty regions (partial refresh to e-paper) */
    ui_page_draw();

    /* Process input */
    ui_input_process();
}

/*=============================================================================
 *  Full Screen Refresh
 *
 *  Forces a complete screen refresh. Use sparingly as it's slow.
 *=============================================================================*/

void ui_system_full_refresh(void)
{
    if (!s_initialized) return;
    ui_render_flush_to_display();
}

/*=============================================================================
 *  Screen Clear
 *=============================================================================*/

void ui_system_clear(void)
{
    ui_screen_clear(UI_COLOR_WHITE);
    ui_page_invalidate_all();
}
