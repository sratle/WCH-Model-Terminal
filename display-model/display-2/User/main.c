/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Description        : Display-2 E-paper demo with MiniUI.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
*******************************************************************************/

#include "debug.h"
#include "../Common/Common/Eink/epaper.h"
#include "../Common/Common/MiniUI/miniui.h"
#include "../Common/Common/MiniUI/font/font_montserrat_12.h"
#include "../Common/Common/MiniUI/font/font_montserrat_16.h"

/* ui_system functions */
void ui_system_init(void);
void ui_system_tick(void);

/*=============================================================================
 *  Demo Page
 *=============================================================================*/

static ui_label_t s_title_label;
static ui_label_t s_info_label;
static ui_button_t s_test_btn;
static ui_widget_t *s_demo_widgets[] = {
    (ui_widget_t *)&s_title_label,
    (ui_widget_t *)&s_info_label,
    (ui_widget_t *)&s_test_btn,
};

static ui_page_t s_demo_page;

static int s_btn_click_count = 0;

static void on_btn_click(ui_widget_t *w)
{
    s_btn_click_count++;
    char buf[32];
    snprintf(buf, sizeof(buf), "Clicked: %d", s_btn_click_count);
    ui_label_set_text(&s_info_label, buf);
    (void)w;
}

static void demo_page_on_enter(ui_page_t *page)
{
    (void)page;
    /* Title */
    ui_rect_t title_rect = { 16, 20, 616, 30 };
    ui_label_init(&s_title_label, &title_rect, "MiniUI on E-ink", &font_montserrat_16);
    ui_label_set_align(&s_title_label, UI_ALIGN_CENTER);

    /* Info label */
    ui_rect_t info_rect = { 16, 60, 616, 24 };
    ui_label_init(&s_info_label, &info_rect, "Ready", &font_montserrat_16);
    ui_label_set_align(&s_info_label, UI_ALIGN_CENTER);

    /* Test button */
    ui_rect_t btn_rect = { 224, 200, 200, 48 };
    ui_button_init(&s_test_btn, &btn_rect, "Click Me", &font_montserrat_16);
    ui_button_set_callback(&s_test_btn, on_btn_click);

    ui_page_set_widgets(page, s_demo_widgets, 3);
}

static void demo_page_on_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    /* Draw a separator line below title area */
    ui_draw_hline(16, 55, 616, UI_COLOR_BLACK);
}

/*=============================================================================
 *  Main
 *=============================================================================*/

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    printf("\r\n=== Display-2 MiniUI Demo ===\r\n");

    /* Initialize MiniUI system (includes e-paper init) */
    ui_system_init();

    /* Setup demo page */
    ui_page_struct_init_fullscreen(&s_demo_page, "Demo", 1);
    ui_page_set_callbacks(&s_demo_page, demo_page_on_enter, NULL, demo_page_on_draw, NULL);
    ui_page_register(&s_demo_page);
    ui_page_switch(&s_demo_page);

    printf("[main] entering main loop\r\n");

    while (1)
    {
        ui_system_tick();
        Delay_Ms(50);  /* 20 FPS tick rate for E-ink */
    }
}
