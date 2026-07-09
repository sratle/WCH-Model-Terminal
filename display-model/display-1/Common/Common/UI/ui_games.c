/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_games.c
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/25
* Description        : Games page implementation.
*                      8 games in a single page grid (3x4 layout, no pagination).
*                      V4.0: Keyboard navigation support (arrow keys + Enter).
********************************************************************************/
#include "ui_games.h"
#include "ui_main.h"
#include "../Games/games.h"
#include "../Games/game_airplane.h"
#include "../Games/game_minesweeper.h"

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
#define GAME_TOTAL           6

/*=============================================================================
 *  Game Entry Data
 *=============================================================================*/

typedef struct {
    const char *name;
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
    ui_page_t *(*get_page)(void);
} game_entry_t;

static const game_entry_t s_games[GAME_TOTAL] = {
    {"Tetris",     icon_shuffle_16_bitmap,  ICON_SHUFFLE_16_WIDTH,  ICON_SHUFFLE_16_HEIGHT,  game_tetris_get_page},
    {"2048",       icon_copy_16_bitmap,     ICON_COPY_16_WIDTH,     ICON_COPY_16_HEIGHT,     game_2048_get_page},
    {"Snake",      icon_loop_16_bitmap,     ICON_LOOP_16_WIDTH,     ICON_LOOP_16_HEIGHT,     game_snake_get_page},
    {"Breakout",   icon_play_16_bitmap,     ICON_PLAY_16_WIDTH,     ICON_PLAY_16_HEIGHT,     game_breakout_get_page},
    {"Airplane",   icon_up_16_bitmap,       ICON_UP_16_WIDTH,       ICON_UP_16_HEIGHT,       game_airplane_get_page},
    {"MineSweeper",icon_warning_16_bitmap,  ICON_WARNING_16_WIDTH,  ICON_WARNING_16_HEIGHT,  game_minesweeper_get_page},
};

/*=============================================================================
 *  Games Page Widgets
 *=============================================================================*/

static int8_t s_focus_idx = -1;   /* Keyboard focus index (-1 = none) */

static ui_label_t lbl_title;
static ui_icon_button_t btn_games[GAME_TOTAL];
static ui_widget_t *s_games_widgets[1 + GAME_TOTAL];

/*=============================================================================
 *  Focus Management
 *=============================================================================*/

static void games_clear_focus(void)
{
    if (s_focus_idx >= 0 && s_focus_idx < GAME_TOTAL) {
        btn_games[s_focus_idx].base.flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate((ui_widget_t *)&btn_games[s_focus_idx]);
    }
    s_focus_idx = -1;
}

static void games_set_focus(int8_t idx)
{
    games_clear_focus();
    if (idx >= 0 && idx < GAME_TOTAL) {
        s_focus_idx = idx;
        btn_games[s_focus_idx].base.flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate((ui_widget_t *)&btn_games[s_focus_idx]);
    }
}

/*=============================================================================
 *  Game Button Callback
 *=============================================================================*/

static void game_button_click(ui_widget_t *w)
{
    int game_idx = (int)(intptr_t)w->user_data;
    if (game_idx >= 0 && game_idx < GAME_TOTAL) {
        ui_page_t *target = s_games[game_idx].get_page();
        if (target) {
            ui_page_push(target);
        }
    }
}

/*=============================================================================
 *  Keyboard Event Handler (page-level)
 *=============================================================================*/

static bool games_handle_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_focus_idx < 0) {
            games_set_focus(0);
        } else if (s_focus_idx + GAME_GRID_COLS < GAME_TOTAL) {
            games_set_focus(s_focus_idx + GAME_GRID_COLS);
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_focus_idx < 0) {
            games_set_focus(0);
        } else if (s_focus_idx >= GAME_GRID_COLS) {
            games_set_focus(s_focus_idx - GAME_GRID_COLS);
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_RIGHT_ARROW) {
        if (s_focus_idx < 0) {
            games_set_focus(0);
        } else if (s_focus_idx + 1 < GAME_TOTAL) {
            /* Check we stay in the same row */
            int cur_row = s_focus_idx / GAME_GRID_COLS;
            int new_row = (s_focus_idx + 1) / GAME_GRID_COLS;
            if (new_row == cur_row) {
                games_set_focus(s_focus_idx + 1);
            }
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_LEFT_ARROW) {
        if (s_focus_idx < 0) {
            games_set_focus(0);
        } else if (s_focus_idx > 0) {
            /* Check we stay in the same row */
            int cur_row = s_focus_idx / GAME_GRID_COLS;
            int new_row = (s_focus_idx - 1) / GAME_GRID_COLS;
            if (new_row == cur_row) {
                games_set_focus(s_focus_idx - 1);
            }
        }
        return true;
    }

    if (e->type == UI_EVENT_KEY_OK) {
        if (s_focus_idx >= 0 && s_focus_idx < GAME_TOTAL) {
            game_button_click((ui_widget_t *)&btn_games[s_focus_idx]);
            return true;
        }
        /* No focus: set focus to first item */
        games_set_focus(0);
        return true;
    }

    return false;
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
    ui_page_set_event_cb(&page_games, games_handle_event);
}

void ui_games_enter(ui_page_t *page)
{
    (void)page;
    games_clear_focus();
}
