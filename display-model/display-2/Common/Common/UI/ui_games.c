/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_games.c
* Author             : E-ink Model Team
* Version            : V5.0.0
* Date               : 2026/07/19
* Description        : Games page implementation.
*                      Two turn-based games (2048, Minesweeper) shown as
*                      large centered buttons — suited to e-ink partial
*                      refresh.  Action games were removed to save FLASH.
********************************************************************************/
#include "ui_games.h"
#include "ui_main.h"
#include "../Games/games.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include "../MiniUI/font/ui_icons_16.h"

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

static const game_entry_t s_games[] = {
    { "2048",        icon_copy_16_bitmap,    ICON_COPY_16_WIDTH,    ICON_COPY_16_HEIGHT,    game_2048_get_page        },
    { "MineSweeper", icon_warning_16_bitmap, ICON_WARNING_16_WIDTH, ICON_WARNING_16_HEIGHT, game_minesweeper_get_page },
};

#define GAME_TOTAL      (sizeof(s_games) / sizeof(s_games[0]))
#define GAME_BTN_W      180
#define GAME_BTN_H      160
#define GAME_BTN_GAP    24

/*=============================================================================
 *  Games Page Widgets
 *=============================================================================*/

static ui_label_t lbl_title;
static ui_icon_button_t btn_games[GAME_TOTAL];
static ui_widget_t *s_games_widgets[1 + GAME_TOTAL];

/*=============================================================================
 *  Game Button Callback — launch game page
 *=============================================================================*/

static void game_button_click(ui_widget_t *w)
{
    int idx = (int)(intptr_t)w->user_data;
    if (idx >= 0 && idx < (int)GAME_TOTAL && s_games[idx].get_page) {
        ui_page_t *game_page = s_games[idx].get_page();
        if (game_page) {
            ui_page_push(game_page);
        }
    }
}

/*=============================================================================
 *  Games Page Initialization
 *=============================================================================*/

void ui_games_init(void)
{
    /* Content area: x in [SIDEBAR_WIDTH, UI_SCREEN_WIDTH) */
    int16_t content_w = UI_SCREEN_WIDTH - SIDEBAR_WIDTH;
    int16_t grid_w = GAME_TOTAL * GAME_BTN_W + (GAME_TOTAL - 1) * GAME_BTN_GAP;
    int16_t grid_x = SIDEBAR_WIDTH + (content_w - grid_w) / 2;
    int16_t grid_y = (UI_SCREEN_HEIGHT - GAME_BTN_H) / 2 + 10;

    ui_rect_t title_rect = {SIDEBAR_WIDTH + 20, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Games", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    s_games_widgets[0] = (ui_widget_t *)&lbl_title;

    for (int i = 0; i < (int)GAME_TOTAL; i++) {
        ui_rect_t btn_rect = {
            grid_x + i * (GAME_BTN_W + GAME_BTN_GAP),
            grid_y,
            GAME_BTN_W,
            GAME_BTN_H
        };

        ui_icon_button_init(&btn_games[i], &btn_rect,
                            s_games[i].icon, s_games[i].icon_w, s_games[i].icon_h,
                            s_games[i].name, &font_montserrat_12);
        ui_icon_button_set_callback(&btn_games[i], game_button_click);
        btn_games[i].base.user_data = (void *)(intptr_t)i;
        btn_games[i].radius = 12;

        s_games_widgets[1 + i] = (ui_widget_t *)&btn_games[i];
    }

    ui_page_set_widgets(&page_games, s_games_widgets, 1 + GAME_TOTAL);
    ui_page_set_callbacks(&page_games, ui_games_enter, NULL, NULL, NULL);
}

void ui_games_enter(ui_page_t *page)
{
    (void)page;
}
