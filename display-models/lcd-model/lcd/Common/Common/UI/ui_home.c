/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_home.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : Home page implementation.
*                      Date/time display and module status cards.
********************************************************************************/
#include "ui_home.h"
#include "ui_main.h"
#include <stdio.h>

/*=============================================================================
 *  Home Page Widgets
 *=============================================================================*/

static ui_label_t lbl_date;
static ui_label_t lbl_time;
static ui_card_t card_core;
static ui_card_t card_bt;
static ui_card_t card_wifi;
static ui_card_t card_touch;

static ui_widget_t *s_home_widgets[6];

/*=============================================================================
 *  Home Page Drawing
 *=============================================================================*/

void ui_home_draw(ui_page_t *page, ui_rect_t *dirty)
{
    /* Draw content area background */
    ui_rect_t content = {SIDEBAR_WIDTH, 0, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&content, UI_COLOR_BG_MAIN);
    
    /* Draw widgets */
    for (uint16_t i = 0; i < page->widget_count; i++) {
        if (page->widgets[i]) {
            ui_widget_draw(page->widgets[i], dirty);
        }
    }
}

/*=============================================================================
 *  Home Page Initialization
 *=============================================================================*/

void ui_home_init(void)
{
    /* Date label */
    ui_rect_t date_rect = {SIDEBAR_WIDTH + 20, 20, 200, 30};
    ui_label_init(&lbl_date, &date_rect, "2025/04/19", &font_montserrat_12);
    ui_label_set_color(&lbl_date, UI_COLOR_TEXT_PRIMARY);
    
    /* Time label */
    ui_rect_t time_rect = {SIDEBAR_WIDTH + 20, 55, 200, 40};
    ui_label_init(&lbl_time, &time_rect, "14:30:00", &font_montserrat_16);
    ui_label_set_color(&lbl_time, UI_COLOR_PRIMARY);
    
    /* Module status cards */
    ui_rect_t card_rect = {SIDEBAR_WIDTH + 20, 120, 130, 80};
    ui_card_init(&card_core, &card_rect);
    card_core.base.bg_color = UI_COLOR_BG_CARD;
    
    card_rect.x += 140;
    ui_card_init(&card_bt, &card_rect);
    card_bt.base.bg_color = UI_COLOR_BG_CARD;
    
    card_rect.x += 140;
    ui_card_init(&card_wifi, &card_rect);
    card_wifi.base.bg_color = UI_COLOR_BG_CARD;
    
    card_rect.x += 140;
    ui_card_init(&card_touch, &card_rect);
    card_touch.base.bg_color = UI_COLOR_BG_CARD;
    
    /* Setup widget array */
    s_home_widgets[0] = (ui_widget_t *)&lbl_date;
    s_home_widgets[1] = (ui_widget_t *)&lbl_time;
    s_home_widgets[2] = (ui_widget_t *)&card_core;
    s_home_widgets[3] = (ui_widget_t *)&card_bt;
    s_home_widgets[4] = (ui_widget_t *)&card_wifi;
    s_home_widgets[5] = (ui_widget_t *)&card_touch;
    
    /* Setup page */
    ui_page_set_widgets(&page_home, s_home_widgets, 6);
    ui_page_set_callbacks(&page_home, ui_home_enter, NULL, ui_home_draw, NULL);
}

void ui_home_enter(ui_page_t *page)
{
    /* Update date/time when entering */
    ui_label_set_text(&lbl_date, "2025/04/19");
    ui_label_set_text(&lbl_time, "14:30:00");
}

void ui_home_event(ui_widget_t *w, ui_event_t *e)
{
    /* Handle home page specific events */
}
