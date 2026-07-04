/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_games.c
* Author             : E-ink Model Team
* Version            : V4.0.0
* Date               : 2026/06/14
* Description        : Games page implementation.
*                      8 games in a single page grid (3x4 layout, no pagination).
*                      Adapted from Display-1 for E-ink display.
********************************************************************************/
#include "ui_games.h"
#include "ui_main.h"
#include "../Games/games.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include "../MiniUI/font/ui_icons_16.h"

/*=============================================================================
 *  Game Grid Configuration
 *=============================================================================*/

#define GAME_GRID_COLS       3
#define GAME_GRID_ROWS       4
#define GAME_BTN_W           150
#define GAME_BTN_H           90
#define GAME_BTN_GAP_X       20
#define GAME_BTN_GAP_Y       16
#define GAME_GRID_TOP        70
#define GAME_TOTAL           8

/*=============================================================================
 *  Game Entry Data
 *=============================================================================*/

typedef struct {
    const char *name;
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
} game_entry_t;

static const game_entry_t s_games[GAME_TOTAL] = {
    {"Tetris",     icon_shuffle_16_bitmap,  ICON_SHUFFLE_16_WIDTH,  ICON_SHUFFLE_16_HEIGHT },
    {"2048",       icon_copy_16_bitmap,     ICON_COPY_16_WIDTH,     ICON_COPY_16_HEIGHT    },
    {"Snake",      icon_loop_16_bitmap,     ICON_LOOP_16_WIDTH,     ICON_LOOP_16_HEIGHT    },
    {"Breakout",   icon_play_16_bitmap,     ICON_PLAY_16_WIDTH,     ICON_PLAY_16_HEIGHT    },
    {"Airplane",   icon_up_16_bitmap,       ICON_UP_16_WIDTH,       ICON_UP_16_HEIGHT      },
    {"Touch Ball", icon_ok_16_bitmap,       ICON_OK_16_WIDTH,       ICON_OK_16_HEIGHT      },
    {"MineSweeper",icon_warning_16_bitmap,  ICON_WARNING_16_WIDTH,  ICON_WARNING_16_HEIGHT },
    {"Contra",     icon_play_16_bitmap,     ICON_PLAY_16_WIDTH,     ICON_PLAY_16_HEIGHT    },
};

/*=============================================================================
 *  Games Page Widgets
 *=============================================================================*/

static ui_label_t lbl_title;
static ui_icon_button_t btn_games[GAME_TOTAL];
static ui_widget_t *s_games_widgets[1 + GAME_TOTAL];

/*=============================================================================
 *  Game Button Callback — launch game pages
 *=============================================================================*/

static ui_page_t *game_get_page(int idx)
{
    switch (idx) {
        case 0: return game_tetris_get_page();
        case 1: return game_2048_get_page();
        case 2: return game_snake_get_page();
        case 3: return game_breakout_get_page();
        case 4: return game_airplane_get_page();
        case 5: return game_touchball_get_page();
        case 6: return game_minesweeper_get_page();
        case 7: return game_contra_get_page();
        default: return NULL;
    }
}

static void game_button_click(ui_widget_t *w)
{
    int idx = (int)(intptr_t)w->user_data;
    ui_page_t *game_page = game_get_page(idx);
    if (game_page) {
        ui_page_push(game_page);
    }
}

/*=============================================================================
 *  Games Page Initialization
 *=============================================================================*/

void ui_games_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 30;

    ui_rect_t title_rect = {cx - 10, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Games", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    s_games_widgets[0] = (ui_widget_t *)&lbl_title;

    int16_t grid_x_start = cx;
    int16_t grid_y_start = GAME_GRID_TOP;

    for (int i = 0; i < GAME_TOTAL; i++) {
        int16_t col = i % GAME_GRID_COLS;
        int16_t row = i / GAME_GRID_COLS;

        ui_rect_t btn_rect = {
            grid_x_start + col * (GAME_BTN_W + GAME_BTN_GAP_X),
            grid_y_start + row * (GAME_BTN_H + GAME_BTN_GAP_Y),
            GAME_BTN_W,
            GAME_BTN_H
        };

        ui_icon_button_init(&btn_games[i], &btn_rect,
                            s_games[i].icon, s_games[i].icon_w, s_games[i].icon_h,
                            s_games[i].name, &font_montserrat_12);
        ui_icon_button_set_callback(&btn_games[i], game_button_click);
        btn_games[i].base.user_data = (void *)(intptr_t)i;

        s_games_widgets[1 + i] = (ui_widget_t *)&btn_games[i];
    }

    ui_page_set_widgets(&page_games, s_games_widgets, 1 + GAME_TOTAL);
    ui_page_set_callbacks(&page_games, ui_games_enter, NULL, NULL, NULL);
}

void ui_games_enter(ui_page_t *page)
{
    (void)page;
}
