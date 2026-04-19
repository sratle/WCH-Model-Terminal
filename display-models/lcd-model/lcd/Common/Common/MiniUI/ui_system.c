/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_system.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI system initialization and main loop.
********************************************************************************/
#include "miniui.h"
#include "../UI/ui_main.h"
#include "../UI/ui_home.h"
#include "../SSD1963/ssd1963.h"
#include "../SSD1963/lcd_config.h"

/*=============================================================================
 *  UI System Initialization
 *=============================================================================*/

void UI_Init(void)
{
    /* Initialize LCD panel control signals (PA4~PA7) */
    LCD_Init(LCD_ORIENTATION_NORMAL);

    /* Initialize SSD1963 display controller */
    SSD1963_Init();

    /* Initialize render engine */
    ui_render_init();
    
    /* Initialize page manager */
    ui_page_init();
    
    /* Initialize input system */
    ui_input_init();
    
    /* Initialize animation system */
    ui_anim_init();
    
    /* Initialize main UI framework */
    ui_main_init();
    
    /* Initialize home page */
    ui_home_init();
    
    /* Show home page */
    ui_page_switch(&page_home);
    
    /* Full screen refresh */
    UI_FullRefresh();
}

/*=============================================================================
 *  Main UI Tick
 *=============================================================================*/

void UI_Tick(void)
{
    /* Process input events */
    ui_event_t *e = ui_input_poll();
    while (e) {
        /* Handle sidebar events first */
        ui_main_handle_event(e);
        
        /* Then dispatch to current page widgets */
        ui_page_t *page = ui_page_current();
        if (page) {
            for (uint16_t i = 0; i < page->widget_count; i++) {
                if (page->widgets[i] && ui_widget_hit_test(page->widgets[i], e->pos.x, e->pos.y)) {
                    ui_widget_event(page->widgets[i], e);
                    break; /* Only send to topmost widget */
                }
            }
        }
        
        e = ui_input_poll();
    }
    
    /* Update animations */
    ui_anim_tick(33); /* ~30fps */
    
    /* Draw dirty regions */
    ui_page_draw();
}

/*=============================================================================
 *  Full Screen Refresh
 *=============================================================================*/

void UI_FullRefresh(void)
{
    /* Clear screen */
    ui_screen_clear(UI_COLOR_BG_MAIN);
    
    /* Draw sidebar */
    ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_main_draw_sidebar(&full);
    
    /* Draw current page */
    ui_page_invalidate_all();
    ui_page_draw();
}
