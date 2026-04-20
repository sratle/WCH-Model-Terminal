/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_games.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : Games page implementation.
*                      Game framework with fullscreen toggle.
********************************************************************************/
#include "ui_games.h"
#include "ui_main.h"

/*=============================================================================
 *  Game Entry Data
 *=============================================================================*/

typedef struct {
    const char *name;
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
} game_entry_t;

static const game_entry_t s_games[] = {
    {"Snake",    icon_play_16_bitmap,     ICON_PLAY_16_WIDTH,     ICON_PLAY_16_HEIGHT},
    {"Pong",     icon_refresh_16_bitmap,  ICON_REFRESH_16_WIDTH,  ICON_REFRESH_16_HEIGHT},
    {"Tetris",   icon_shuffle_16_bitmap,  ICON_SHUFFLE_16_WIDTH,  ICON_SHUFFLE_16_HEIGHT},
    {"Maze",     icon_loop_16_bitmap,     ICON_LOOP_16_WIDTH,     ICON_LOOP_16_HEIGHT},
};

#define GAME_COUNT  4

/*=============================================================================
 *  Games Page Widgets
 *=============================================================================*/

static ui_label_t lbl_title;
static ui_icon_button_t btn_games[GAME_COUNT];
static ui_widget_t *s_games_widgets[1 + GAME_COUNT];

/*=============================================================================
 *  Game Button Callback
 *=============================================================================*/

static void game_button_click(ui_widget_t *w)
{
    (void)w;
}

/*=============================================================================
 *  Games Page Drawing
 *=============================================================================*/

void ui_games_draw(ui_page_t *page, ui_rect_t *dirty)
{
    ui_rect_t content = {SIDEBAR_WIDTH, 0, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&content, UI_COLOR_BG_MAIN);
}

/*=============================================================================
 *  Games Page Initialization
 *=============================================================================*/

void ui_games_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t title_rect = {cx, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Games", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    s_games_widgets[0] = (ui_widget_t *)&lbl_title;

    int16_t btn_w = 140;
    int16_t btn_h = 120;
    int16_t btn_gap_x = 20;
    int16_t btn_gap_y = 20;
    int16_t grid_x = cx + 20;
    int16_t grid_y = 80;

    for (int i = 0; i < GAME_COUNT; i++) {
        int16_t col = i % 3;
        int16_t row = i / 3;

        ui_rect_t btn_rect = {
            grid_x + col * (btn_w + btn_gap_x),
            grid_y + row * (btn_h + btn_gap_y),
            btn_w,
            btn_h
        };

        ui_icon_button_init(&btn_games[i], &btn_rect,
                            s_games[i].icon, s_games[i].icon_w, s_games[i].icon_h,
                            s_games[i].name, &font_montserrat_12);
        ui_icon_button_set_callback(&btn_games[i], game_button_click);

        s_games_widgets[1 + i] = (ui_widget_t *)&btn_games[i];
    }

    ui_page_set_widgets(&page_games, s_games_widgets, 1 + GAME_COUNT);
    ui_page_set_callbacks(&page_games, ui_games_enter, NULL, ui_games_draw, NULL);
}

void ui_games_enter(ui_page_t *page)
{
    (void)page;
}
