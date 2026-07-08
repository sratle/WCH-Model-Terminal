/********************************** (C) COPYRIGHT *******************************
* File Name          : game_minesweeper.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/20
* Description        : Classic Minesweeper game.
*                      9x9 (10 mines) and 16x16 (30 mines) modes.
*                      Long press = flag, double-click = reveal.
*                      First click never hits a mine.
*                      Optimized dirty-rect partial refresh.
********************************************************************************/
#include "game_minesweeper.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration
 *
 *  Screen: 800x480, Title bar: 40px
 *  Game area: 800x440 (y: 40~480)
 *
 *  Classic Minesweeper layout:
 *    [Grid area]           [Panel: MINES / TIME / MODE]
 *                          [Panel continued]
 *                          [Mode buttons]
 *                          [Restart button]
 *=============================================================================*/

/* Mode definitions */
#define MINE_MODE_EASY_COLS    9
#define MINE_MODE_EASY_ROWS    9
#define MINE_MODE_EASY_MINES   10
#define MINE_MODE_HARD_COLS    16
#define MINE_MODE_HARD_ROWS    16
#define MINE_MODE_HARD_MINES   30

#define MINE_MAX_COLS          16
#define MINE_MAX_ROWS          16

/* Cell size: adaptive based on mode */
#define MINE_CELL_EASY         24
#define MINE_CELL_HARD         20

/* Grid position (centered horizontally in left area) */
#define MINE_GRID_AREA_W       440    /* Left area width for grid */
#define MINE_GRID_X_BASE       15

/* Right panel */
#define MINE_PANEL_X           470
#define MINE_PANEL_W           310
#define MINE_PANEL_Y           (APP_TITLE_BAR_H + 10)  /* 50 */

/* Info box dimensions */
#define MINE_INFO_LABEL_H      14
#define MINE_INFO_BOX_H        28
#define MINE_INFO_BOX_R        6
#define MINE_INFO_GAP          6

/* Mines counter section */
#define MINE_MINES_LABEL_Y     MINE_PANEL_Y
#define MINE_MINES_BOX_Y       (MINE_MINES_LABEL_Y + MINE_INFO_LABEL_H)

/* Timer section */
#define MINE_TIME_LABEL_Y      (MINE_MINES_BOX_Y + MINE_INFO_BOX_H + MINE_INFO_GAP)
#define MINE_TIME_BOX_Y        (MINE_TIME_LABEL_Y + MINE_INFO_LABEL_H)

/* Mode section */
#define MINE_MODE_LABEL_Y      (MINE_TIME_BOX_Y + MINE_INFO_BOX_H + MINE_INFO_GAP)
#define MINE_MODE_BOX_Y        (MINE_MODE_LABEL_Y + MINE_INFO_LABEL_H)

/* Best time section */
#define MINE_BEST_LABEL_Y      (MINE_MODE_BOX_Y + MINE_INFO_BOX_H + MINE_INFO_GAP)
#define MINE_BEST_BOX_Y        (MINE_BEST_LABEL_Y + MINE_INFO_LABEL_H)

/* Mode buttons */
#define MINE_MODE_BTN_W        130
#define MINE_MODE_BTN_H        40
#define MINE_MODE_GAP          10
#define MINE_MODE_TOP          (MINE_BEST_BOX_Y + MINE_INFO_BOX_H + 16)

/* Restart button */
#define MINE_RESTART_W         (2 * MINE_MODE_BTN_W + MINE_MODE_GAP)
#define MINE_RESTART_H         44
#define MINE_RESTART_TOP       (MINE_MODE_TOP + MINE_MODE_BTN_H + 16)

/*=============================================================================
 *  Cell States & Colors
 *=============================================================================*/

/* Cell flags */
#define CELL_REVEALED   0x01
#define CELL_FLAGGED    0x02
#define CELL_MINE       0x04

/* Colors */
#define MINE_BG_COLOR           UI_HEX(0x1A1A1A)
#define MINE_CELL_HIDDEN        UI_HEX(0x5C6BC0)   /* Unrevealed cell */
#define MINE_CELL_HIDDEN_HI     UI_HEX(0x7986CB)
#define MINE_CELL_HIDDEN_BD     UI_HEX(0x3F51B5)
#define MINE_CELL_REVEALED      UI_HEX(0x2C2C2C)   /* Revealed empty cell */
#define MINE_CELL_REVEALED_BD   UI_HEX(0x1A1A1A)
#define MINE_CELL_FLAGGED       UI_HEX(0xFF7043)    /* Flagged cell */
#define MINE_CELL_FLAGGED_HI    UI_HEX(0xFF8A65)
#define MINE_CELL_FLAGGED_BD    UI_HEX(0xE64A19)
#define MINE_CELL_MINE_HIT      UI_HEX(0xF44336)    /* Hit mine */
#define MINE_CELL_MINE          UI_HEX(0x424242)     /* Revealed mine (not hit) */

/* Number colors (1-8) */
static const ui_color_t s_num_colors[8] = {
    UI_HEX(0x2196F3),  /* 1 - Blue */
    UI_HEX(0x4CAF50),  /* 2 - Green */
    UI_HEX(0xF44336),  /* 3 - Red */
    UI_HEX(0x7B1FA2),  /* 4 - Purple */
    UI_HEX(0xFF5722),  /* 5 - Orange */
    UI_HEX(0x00897B),  /* 6 - Teal */
    UI_HEX(0x212121),  /* 7 - Black */
    UI_HEX(0x9E9E9E),  /* 8 - Gray */
};

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    MINE_MODE_EASY,     /* 9x9, 10 mines */
    MINE_MODE_HARD,     /* 16x16, 30 mines */
} mine_mode_t;

typedef enum {
    MINE_STATE_IDLE,        /* Waiting to start */
    MINE_STATE_PLAYING,     /* In game */
    MINE_STATE_WIN,         /* All non-mine cells revealed */
    MINE_STATE_LOSE,        /* Hit a mine */
} mine_state_t;

typedef struct {
    uint8_t flags[MINE_MAX_ROWS][MINE_MAX_COLS];  /* CELL_REVEALED | CELL_FLAGGED | CELL_MINE */
    uint8_t numbers[MINE_MAX_ROWS][MINE_MAX_COLS]; /* Adjacent mine count (0-8) */
    int cols;
    int rows;
    int total_mines;
    int flag_count;
    int revealed_count;
    mine_mode_t mode;
    mine_state_t state;
    bool first_click;       /* True until first reveal */
    int frame_count;        /* Frame counter for timer (25 FPS) */
    int elapsed_sec;        /* Elapsed seconds */
    int best_time;          /* Best completion time (seconds), 0=none */
    char buf_mines[16];
    char buf_time[16];
    char buf_best[16];
} mine_game_t;

/*=============================================================================
/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_mine;
static ui_widget_t s_touch_area;
static ui_widget_t s_btn_easy, s_btn_hard;
static ui_widget_t s_btn_restart;
static ui_widget_t *s_mine_widgets[6];
static mine_game_t s_mine;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void mine_draw_mode_btns(void);
static void mine_draw_restart_btn(void);

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void mine_itoa(int val, char *out)
{
    char tmp[12];
    int i = 0;
    bool neg = val < 0;
    if (neg) val = -val;
    if (val == 0) { out[0] = '0'; out[1] = '\0'; return; }
    while (val > 0) { tmp[i++] = '0' + val % 10; val /= 10; }
    int j = 0;
    if (neg) out[j++] = '-';
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
}

static int mine_rng_range(int min, int max)
{
    static uint32_t seed = 24680;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));
}

static void mine_update_texts(void)
{
    int mines_left = s_mine.total_mines - s_mine.flag_count;
    mine_itoa(mines_left, s_mine.buf_mines);
    mine_itoa((int)s_mine.elapsed_sec, s_mine.buf_time);
    if (s_mine.best_time > 0) {
        mine_itoa(s_mine.best_time, s_mine.buf_best);
    } else {
        s_mine.buf_best[0] = '-'; s_mine.buf_best[1] = '\0';
    }
}

/*=============================================================================
 *  Best Time Persistence (via CLI passthrough to Core appcfg)
 *=============================================================================*/

static void mine_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    if (!tag || strcmp(tag, "appcfg") != 0) return;
    s_mine.best_time = atoi(buf);
    mine_update_texts();
    ui_page_invalidate_all();
}

static const uart_cli_cb_t s_mine_cli_cb = {
    .on_cli_complete = mine_on_cli_complete,
};

static void mine_load_best(void)
{
    UART_SetCLICallbacks(&s_mine_cli_cb);
    UART_SendCLI("appcfg get game_minesweeper besttime");
}

static void mine_save_best(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "appcfg set game_minesweeper besttime %d", s_mine.best_time);
    UART_SendCLI(cmd);
}

/*=============================================================================
 *  Grid Pixel Helpers
 *=============================================================================*/

static int mine_cell_size(void)
{
    return (s_mine.mode == MINE_MODE_EASY) ? MINE_CELL_EASY : MINE_CELL_HARD;
}

static int mine_grid_w(void)
{
    return s_mine.cols * mine_cell_size();
}

static int mine_grid_h(void)
{
    return s_mine.rows * mine_cell_size();
}

static int mine_grid_x(void)
{
    return MINE_GRID_X_BASE + (MINE_GRID_AREA_W - mine_grid_w()) / 2;
}

static int mine_grid_y(void)
{
    return APP_TITLE_BAR_H + (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - mine_grid_h()) / 2;
}

static ui_rect_t mine_cell_rect(int row, int col)
{
    int cs = mine_cell_size();
    ui_rect_t r = {
        mine_grid_x() + col * cs,
        mine_grid_y() + row * cs,
        cs, cs
    };
    return r;
}

/* Invalidate a single grid cell */
static void mine_inv_cell(int row, int col)
{
    if (row < 0 || row >= s_mine.rows || col < 0 || col >= s_mine.cols) return;
    ui_rect_t r = mine_cell_rect(row, col);
    ui_page_invalidate(&r);
}

/* Invalidate mines counter */
static void mine_inv_mines(void)
{
    ui_rect_t r = {MINE_PANEL_X, MINE_MINES_BOX_Y, MINE_PANEL_W, MINE_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate timer */
static void mine_inv_time(void)
{
    ui_rect_t r = {MINE_PANEL_X, MINE_TIME_BOX_Y, MINE_PANEL_W, MINE_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate entire grid */
static void mine_inv_grid(void)
{
    ui_rect_t r = {mine_grid_x(), mine_grid_y(), mine_grid_w(), mine_grid_h()};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

/* Place mines, avoiding (safe_row, safe_col) and its neighbors */
static void mine_place_mines(int safe_row, int safe_col)
{
    memset(s_mine.flags, 0, sizeof(s_mine.flags));

    int placed = 0;
    while (placed < s_mine.total_mines) {
        int r = mine_rng_range(0, s_mine.rows - 1);
        int c = mine_rng_range(0, s_mine.cols - 1);

        /* Skip if already a mine */
        if (s_mine.flags[r][c] & CELL_MINE) continue;

        /* Skip safe zone (3x3 around first click) */
        if (r >= safe_row - 1 && r <= safe_row + 1 &&
            c >= safe_col - 1 && c <= safe_col + 1) continue;

        s_mine.flags[r][c] |= CELL_MINE;
        placed++;
    }

    /* Calculate numbers */
    memset(s_mine.numbers, 0, sizeof(s_mine.numbers));
    for (int r = 0; r < s_mine.rows; r++) {
        for (int c = 0; c < s_mine.cols; c++) {
            if (s_mine.flags[r][c] & CELL_MINE) continue;
            int count = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < s_mine.rows && nc >= 0 && nc < s_mine.cols) {
                        if (s_mine.flags[nr][nc] & CELL_MINE) count++;
                    }
                }
            }
            s_mine.numbers[r][c] = count;
        }
    }
}

/* Flood-fill reveal for empty cells (number == 0) */
static void mine_reveal_flood(int row, int col)
{
    if (row < 0 || row >= s_mine.rows || col < 0 || col >= s_mine.cols) return;
    if (s_mine.flags[row][col] & CELL_REVEALED) return;
    if (s_mine.flags[row][col] & CELL_FLAGGED) return;

    s_mine.flags[row][col] |= CELL_REVEALED;
    s_mine.revealed_count++;
    mine_inv_cell(row, col);

    /* If this is an empty cell (0 adjacent mines), reveal neighbors */
    if (s_mine.numbers[row][col] == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                mine_reveal_flood(row + dr, col + dc);
            }
        }
    }
}

/* Reveal a cell. Returns true if a mine was hit. */
static bool mine_reveal_cell(int row, int col)
{
    if (row < 0 || row >= s_mine.rows || col < 0 || col >= s_mine.cols) return false;
    if (s_mine.flags[row][col] & CELL_REVEALED) return false;
    if (s_mine.flags[row][col] & CELL_FLAGGED) return false;

    /* First click: place mines avoiding this cell */
    if (s_mine.first_click) {
        s_mine.first_click = false;
        mine_place_mines(row, col);
        s_mine.frame_count = 0;
    }

    /* Hit a mine */
    if (s_mine.flags[row][col] & CELL_MINE) {
        s_mine.flags[row][col] |= CELL_REVEALED;
        s_mine.state = MINE_STATE_LOSE;

        /* Reveal all mines */
        for (int r = 0; r < s_mine.rows; r++) {
            for (int c = 0; c < s_mine.cols; c++) {
                if (s_mine.flags[r][c] & CELL_MINE) {
                    s_mine.flags[r][c] |= CELL_REVEALED;
                }
            }
        }
        mine_inv_grid();
        mine_inv_mines();
        return true;
    }

    /* Safe cell */
    mine_reveal_flood(row, col);

    /* Check win condition */
    int total_safe = s_mine.rows * s_mine.cols - s_mine.total_mines;
    if (s_mine.revealed_count >= total_safe) {
        s_mine.state = MINE_STATE_WIN;
        /* Save best time if this is a new record */
        if (s_mine.best_time == 0 || s_mine.elapsed_sec < s_mine.best_time) {
            s_mine.best_time = (int)s_mine.elapsed_sec;
            mine_save_best();
        }
        /* Auto-flag remaining mines */
        for (int r = 0; r < s_mine.rows; r++) {
            for (int c = 0; c < s_mine.cols; c++) {
                if ((s_mine.flags[r][c] & CELL_MINE) && !(s_mine.flags[r][c] & CELL_FLAGGED)) {
                    s_mine.flags[r][c] |= CELL_FLAGGED;
                    s_mine.flag_count++;
                }
            }
        }
        mine_inv_grid();
        mine_inv_mines();
    }

    return false;
}

/* Toggle flag on a cell */
static void mine_toggle_flag(int row, int col)
{
    if (row < 0 || row >= s_mine.rows || col < 0 || col >= s_mine.cols) return;
    if (s_mine.flags[row][col] & CELL_REVEALED) return;

    if (s_mine.flags[row][col] & CELL_FLAGGED) {
        s_mine.flags[row][col] &= ~CELL_FLAGGED;
        s_mine.flag_count--;
    } else {
        s_mine.flags[row][col] |= CELL_FLAGGED;
        s_mine.flag_count++;
    }

    mine_inv_cell(row, col);
    mine_inv_mines();
}

/* Get grid cell from pixel coordinates. Returns true if valid. */
static bool mine_pixel_to_cell(int16_t px, int16_t py, int *row, int *col)
{
    int gx = mine_grid_x();
    int gy = mine_grid_y();
    int cs = mine_cell_size();

    if (px < gx || py < gy) return false;
    int c = (px - gx) / cs;
    int r = (py - gy) / cs;
    if (r < 0 || r >= s_mine.rows || c < 0 || c >= s_mine.cols) return false;

    *row = r;
    *col = c;
    return true;
}

static void mine_start_game(mine_mode_t mode)
{
    int saved_best = s_mine.best_time;
    memset(&s_mine, 0, sizeof(s_mine));
    s_mine.best_time = saved_best;
    s_mine.mode = mode;
    s_mine.state = MINE_STATE_PLAYING;
    s_mine.first_click = true;

    if (mode == MINE_MODE_EASY) {
        s_mine.cols = MINE_MODE_EASY_COLS;
        s_mine.rows = MINE_MODE_EASY_ROWS;
        s_mine.total_mines = MINE_MODE_EASY_MINES;
    } else {
        s_mine.cols = MINE_MODE_HARD_COLS;
        s_mine.rows = MINE_MODE_HARD_ROWS;
        s_mine.total_mines = MINE_MODE_HARD_MINES;
    }

    mine_update_texts();
    ui_page_invalidate_all();
}

static void mine_restart(void)
{
    mine_start_game(s_mine.mode);
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

/* Draw a single hidden (unrevealed) cell */
static void mine_draw_cell_hidden(int row, int col)
{
    ui_rect_t r = mine_cell_rect(row, col);
    ui_draw_fill_rect(&r, MINE_CELL_HIDDEN);
    ui_draw_hline(r.x, r.y, r.w, MINE_CELL_HIDDEN_HI);
    ui_draw_vline(r.x, r.y, r.h, MINE_CELL_HIDDEN_HI);
    ui_draw_hline(r.x, r.y + r.h - 1, r.w, MINE_CELL_HIDDEN_BD);
    ui_draw_vline(r.x + r.w - 1, r.y, r.h, MINE_CELL_HIDDEN_BD);
}

/* Draw a flagged cell */
static void mine_draw_cell_flagged(int row, int col)
{
    ui_rect_t r = mine_cell_rect(row, col);
    ui_draw_fill_rect(&r, MINE_CELL_FLAGGED);
    ui_draw_hline(r.x, r.y, r.w, MINE_CELL_FLAGGED_HI);
    ui_draw_vline(r.x, r.y, r.h, MINE_CELL_FLAGGED_HI);
    ui_draw_hline(r.x, r.y + r.h - 1, r.w, MINE_CELL_FLAGGED_BD);
    ui_draw_vline(r.x + r.w - 1, r.y, r.h, MINE_CELL_FLAGGED_BD);

    /* Draw flag icon (small triangle + pole) */
    int cs = mine_cell_size();
    int16_t cx = r.x + cs / 2;
    int16_t cy = r.y + cs / 2;
    ui_color_t flag_color = UI_COLOR_WHITE;
    /* Pole */
    ui_draw_vline(cx + 1, cy - cs / 4, cs / 2, flag_color);
    /* Flag triangle */
    ui_draw_line(cx + 1, cy - cs / 4, cx - cs / 5, cy - cs / 8, flag_color);
    ui_draw_line(cx - cs / 5, cy - cs / 8, cx + 1, cy, flag_color);
    /* Base */
    ui_draw_hline(cx - cs / 5, cy + cs / 4, cs / 3, flag_color);
}

/* Draw a revealed cell with number */
static void mine_draw_cell_revealed(int row, int col)
{
    ui_rect_t r = mine_cell_rect(row, col);
    int cs = mine_cell_size();

    bool is_hit = (s_mine.state == MINE_STATE_LOSE &&
                   (s_mine.flags[row][col] & CELL_MINE) &&
                   !(s_mine.flags[row][col] & CELL_FLAGGED));

    if (s_mine.flags[row][col] & CELL_MINE) {
        /* Mine cell */
        ui_color_t bg = is_hit ? MINE_CELL_MINE_HIT : MINE_CELL_MINE;
        ui_draw_fill_rect(&r, bg);

        /* Draw mine icon (circle + spikes) */
        int16_t cx = r.x + cs / 2;
        int16_t cy = r.y + cs / 2;
        int16_t mr = cs / 5;
        ui_draw_fill_circle(cx, cy, mr, UI_COLOR_BLACK);
        /* Spikes */
        ui_draw_hline(cx - mr - 2, cy, 2 * mr + 4, UI_COLOR_BLACK);
        ui_draw_vline(cx, cy - mr - 2, 2 * mr + 4, UI_COLOR_BLACK);
        /* Highlight dot */
        ui_draw_pixel(cx - mr / 2, cy - mr / 2, UI_COLOR_WHITE);
    } else {
        /* Empty or numbered cell */
        ui_draw_fill_rect(&r, MINE_CELL_REVEALED);
        ui_draw_rect_border(&r, MINE_CELL_REVEALED_BD, 1);

        uint8_t num = s_mine.numbers[row][col];
        if (num > 0 && num <= 8) {
            char buf[2] = {'0' + num, '\0'};
            ui_draw_text_in_rect(&r, buf, &font_montserrat_16,
                                 s_num_colors[num - 1], 1);
        }
    }
}

/* Draw a single cell based on its state */
static void mine_draw_cell(int row, int col)
{
    uint8_t f = s_mine.flags[row][col];

    if (f & CELL_FLAGGED) {
        mine_draw_cell_flagged(row, col);
    } else if (f & CELL_REVEALED) {
        mine_draw_cell_revealed(row, col);
    } else {
        mine_draw_cell_hidden(row, col);
    }
}

/* Draw entire grid */
static void mine_draw_grid(void)
{
    for (int r = 0; r < s_mine.rows; r++) {
        for (int c = 0; c < s_mine.cols; c++) {
            mine_draw_cell(r, c);
        }
    }
}

/* Draw right panel */
static void mine_draw_panel(void)
{
    /* Mines label */
    ui_rect_t ml = {MINE_PANEL_X, MINE_MINES_LABEL_Y, MINE_PANEL_W, MINE_INFO_LABEL_H};
    ui_draw_text_in_rect(&ml, "MINES", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Mines box */
    ui_rect_t mb = {MINE_PANEL_X, MINE_MINES_BOX_Y, MINE_PANEL_W, MINE_INFO_BOX_H};
    ui_draw_fill_round_rect(&mb, MINE_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&mb, s_mine.buf_mines, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Time label */
    ui_rect_t tl = {MINE_PANEL_X, MINE_TIME_LABEL_Y, MINE_PANEL_W, MINE_INFO_LABEL_H};
    ui_draw_text_in_rect(&tl, "TIME", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Time box */
    ui_rect_t tb = {MINE_PANEL_X, MINE_TIME_BOX_Y, MINE_PANEL_W, MINE_INFO_BOX_H};
    ui_draw_fill_round_rect(&tb, MINE_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&tb, s_mine.buf_time, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Mode label */
    ui_rect_t mol = {MINE_PANEL_X, MINE_MODE_LABEL_Y, MINE_PANEL_W, MINE_INFO_LABEL_H};
    ui_draw_text_in_rect(&mol, "MODE", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Mode box */
    ui_rect_t mob = {MINE_PANEL_X, MINE_MODE_BOX_Y, MINE_PANEL_W, MINE_INFO_BOX_H};
    ui_draw_fill_round_rect(&mob, MINE_INFO_BOX_R, UI_HEX(0x3C3C3C));
    const char *mode_str = (s_mine.mode == MINE_MODE_EASY) ? "9x9" : "16x16";
    ui_draw_text_in_rect(&mob, mode_str, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Best time */
    ui_rect_t bl = {MINE_PANEL_X, MINE_BEST_LABEL_Y, MINE_PANEL_W, MINE_INFO_LABEL_H};
    ui_draw_text_in_rect(&bl, "BEST", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);
    ui_rect_t bb = {MINE_PANEL_X, MINE_BEST_BOX_Y, MINE_PANEL_W, MINE_INFO_BOX_H};
    ui_draw_fill_round_rect(&bb, MINE_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&bb, s_mine.buf_best, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Mode buttons */
    mine_draw_mode_btns();

    /* Restart button */
    mine_draw_restart_btn();
}

/* Draw mode selection buttons */
static void mine_draw_mode_btns(void)
{
    ui_color_t easy_bg = (s_mine.mode == MINE_MODE_EASY) ? UI_HEX(0x7EC8C8) : UI_HEX(0x3C3C3C);
    ui_color_t hard_bg = (s_mine.mode == MINE_MODE_HARD) ? UI_HEX(0x7EC8C8) : UI_HEX(0x3C3C3C);
    ui_color_t easy_text = (s_mine.mode == MINE_MODE_EASY) ? UI_COLOR_WHITE : UI_COLOR_TEXT_SECONDARY;
    ui_color_t hard_text = (s_mine.mode == MINE_MODE_HARD) ? UI_COLOR_WHITE : UI_COLOR_TEXT_SECONDARY;

    /* Easy button */
    ui_rect_t er = {MINE_PANEL_X, MINE_MODE_TOP, MINE_MODE_BTN_W, MINE_MODE_BTN_H};
    ui_draw_fill_round_rect(&er, 6, easy_bg);
    ui_draw_text_in_rect(&er, "9x9 Easy", &font_montserrat_12, easy_text, 1);

    /* Hard button */
    ui_rect_t hr = {MINE_PANEL_X + MINE_MODE_BTN_W + MINE_MODE_GAP, MINE_MODE_TOP,
                    MINE_MODE_BTN_W, MINE_MODE_BTN_H};
    ui_draw_fill_round_rect(&hr, 6, hard_bg);
    ui_draw_text_in_rect(&hr, "16x16 Hard", &font_montserrat_12, hard_text, 1);
}

/* Draw restart button */
static void mine_draw_restart_btn(void)
{
    ui_rect_t r = {MINE_PANEL_X, MINE_RESTART_TOP, MINE_RESTART_W, MINE_RESTART_H};
    ui_draw_fill_round_rect(&r, 6, UI_HEX(0xF5B7B1));
    ui_draw_text_in_rect(&r, "RESTART", &font_montserrat_16,
                         UI_HEX(0x4A4A4A), 1);
}

/* Draw idle screen overlay */
static void mine_draw_idle_overlay(void)
{
    int gx = mine_grid_x();
    int gy = mine_grid_y();
    int gw = mine_grid_w();
    int gh = mine_grid_h();

    ui_rect_t overlay = {gx, gy, gw, gh};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 180);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {gx, gy + gh / 2 - 30, gw, 30};
    ui_draw_text_in_rect(&title, "MINESWEEPER", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t sub = {gx, gy + gh / 2 + 5, gw, 24};
    ui_draw_text_in_rect(&sub, "Select mode to start", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/* Draw win overlay */
static void mine_draw_win_overlay(void)
{
    int gx = mine_grid_x();
    int gy = mine_grid_y();
    int gw = mine_grid_w();
    int gh = mine_grid_h();

    ui_rect_t overlay = {gx, gy, gw, gh};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 160);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {gx, gy + gh / 2 - 30, gw, 30};
    ui_draw_text_in_rect(&title, "YOU WIN!", &font_montserrat_16,
                         UI_HEX(0x4CAF50), 1);

    char buf[32];
    /* Simple format: "Time: Xs" */
    int tlen = 0;
    const char *prefix = "Time: ";
    while (prefix[tlen] && tlen < 31) { buf[tlen] = prefix[tlen]; tlen++; }
    int val = (int)s_mine.elapsed_sec;
    char tbuf[12];
    mine_itoa(val, tbuf);
    int ti = 0;
    while (tbuf[ti] && tlen < 31) { buf[tlen++] = tbuf[ti++]; }
    buf[tlen++] = 's';
    buf[tlen] = '\0';

    ui_rect_t sr = {gx, gy + gh / 2 + 5, gw, 24};
    ui_draw_text_in_rect(&sr, buf, &font_montserrat_12, UI_COLOR_WHITE, 1);

    ui_rect_t hr = {gx, gy + gh / 2 + 35, gw, 24};
    ui_draw_text_in_rect(&hr, "Press RESTART", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/* Draw lose overlay */
static void mine_draw_lose_overlay(void)
{
    int gx = mine_grid_x();
    int gy = mine_grid_y();
    int gw = mine_grid_w();
    int gh = mine_grid_h();

    ui_rect_t overlay = {gx, gy, gw, gh};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 160);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {gx, gy + gh / 2 - 30, gw, 30};
    ui_draw_text_in_rect(&title, "GAME OVER", &font_montserrat_16,
                         UI_HEX(0xF44336), 1);

    ui_rect_t hr = {gx, gy + gh / 2 + 5, gw, 24};
    ui_draw_text_in_rect(&hr, "Press RESTART", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/*=============================================================================
 *  Input Handling
 *=============================================================================*/

static bool s_hold_active = false;       /* True after first LONG_PRESS, cleared on UP */

static void mine_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    if (e->type == UI_EVENT_LONG_PRESS) {
        /* Long press = flag */
        if (!s_hold_active) {
            s_hold_active = true;

            int row, col;
            if (mine_pixel_to_cell(e->pos.x, e->pos.y, &row, &col)) {
                if (s_mine.state == MINE_STATE_PLAYING) {
                    mine_toggle_flag(row, col);
                }
            }
        }
    } else if (e->type == UI_EVENT_UP) {
        s_hold_active = false;
        /* Do NOT reveal on UP. Reveal only happens on DOUBLE_CLICK. */
    } else if (e->type == UI_EVENT_DOUBLE_CLICK) {
        /* Double click = reveal */
        int row, col;
        if (mine_pixel_to_cell(e->pos.x, e->pos.y, &row, &col)) {
            if (s_mine.state == MINE_STATE_PLAYING) {
                mine_reveal_cell(row, col);
            }
        }
    } else if (e->type == UI_EVENT_CLICK) {
        /* Single click does NOT reveal; only double-click reveals. */
    } else if (e->type == UI_EVENT_SWIPE_UP ||
               e->type == UI_EVENT_SWIPE_DOWN ||
               e->type == UI_EVENT_SWIPE_LEFT ||
               e->type == UI_EVENT_SWIPE_RIGHT) {
        /* Swipe — no action */
    } else if (e->type == UI_EVENT_KEY_OK) {
        /* Keyboard OK = start/restart */
        if (s_mine.state == MINE_STATE_IDLE) {
            mine_start_game(MINE_MODE_EASY);
        }
    }
}

static void mine_easy_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK) return;
    mine_start_game(MINE_MODE_EASY);
}

static void mine_hard_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK) return;
    mine_start_game(MINE_MODE_HARD);
}

static void mine_restart_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK) return;
    if (s_mine.state == MINE_STATE_IDLE) return;  /* No game to restart */
    mine_restart();
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void mine_game_enter(ui_page_t *page)
{
    (void)page;
    memset(&s_mine, 0, sizeof(s_mine));
    s_mine.mode = MINE_MODE_EASY;
    s_mine.cols = MINE_MODE_EASY_COLS;
    s_mine.rows = MINE_MODE_EASY_ROWS;
    s_mine.total_mines = MINE_MODE_EASY_MINES;
    s_mine.state = MINE_STATE_IDLE;
    mine_update_texts();
    mine_load_best();
    ui_page_invalidate_all();
}

/* Per-frame game logic update (timer) */
static void mine_game_update(ui_page_t *page)
{
    (void)page;

    /* Update timer (frame-based, 25 FPS) */
    if (s_mine.state == MINE_STATE_PLAYING && !s_mine.first_click) {
        s_mine.frame_count++;
        int new_sec = s_mine.frame_count / 25;
        if (new_sec > 999) new_sec = 999;
        if (new_sec != s_mine.elapsed_sec) {
            s_mine.elapsed_sec = new_sec;
            mine_update_texts();
            mine_inv_time();
        }
    }
}

/* Per-row game rendering (compositing renderer auto-clips to target row) */
static void mine_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Game area background */
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, MINE_BG_COLOR);

    if (s_mine.state == MINE_STATE_IDLE) {
        mine_draw_grid();
        mine_draw_panel();
        mine_draw_idle_overlay();
    } else {
        mine_draw_grid();
        mine_draw_panel();

        if (s_mine.state == MINE_STATE_WIN) {
            mine_draw_win_overlay();
        } else if (s_mine.state == MINE_STATE_LOSE) {
            mine_draw_lose_overlay();
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_minesweeper_init(void)
{
    ui_app_page_init(&s_game_mine, "Minesweeper", 0x206);

    /* Touch area for grid interaction */
    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = mine_touch_event;

    /* Easy mode button */
    ui_rect_t easy_r = {MINE_PANEL_X, MINE_MODE_TOP, MINE_MODE_BTN_W, MINE_MODE_BTN_H};
    ui_widget_init(&s_btn_easy, &easy_r);
    s_btn_easy.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_easy.event_cb = mine_easy_event;

    /* Hard mode button */
    ui_rect_t hard_r = {MINE_PANEL_X + MINE_MODE_BTN_W + MINE_MODE_GAP, MINE_MODE_TOP,
                        MINE_MODE_BTN_W, MINE_MODE_BTN_H};
    ui_widget_init(&s_btn_hard, &hard_r);
    s_btn_hard.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_hard.event_cb = mine_hard_event;

    /* Restart button */
    ui_rect_t restart_r = {MINE_PANEL_X, MINE_RESTART_TOP, MINE_RESTART_W, MINE_RESTART_H};
    ui_widget_init(&s_btn_restart, &restart_r);
    s_btn_restart.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_restart.event_cb = mine_restart_event;

    /* Widget order: high index = high priority for events */
    s_mine_widgets[0] = &s_touch_area;
    s_mine_widgets[1] = &s_btn_easy;
    s_mine_widgets[2] = &s_btn_hard;
    s_mine_widgets[3] = &s_btn_restart;
    s_mine_widgets[4] = (ui_widget_t *)&s_game_mine.lbl_title;
    s_mine_widgets[5] = (ui_widget_t *)&s_game_mine.btn_back;

    ui_page_set_widgets(&s_game_mine.page, s_mine_widgets, 6);
    ui_page_set_callbacks(&s_game_mine.page, mine_game_enter, NULL,
                          mine_game_draw, NULL);
    ui_page_set_update_cb(&s_game_mine.page, mine_game_update);
    ui_page_register(&s_game_mine.page);
}

ui_page_t *game_minesweeper_get_page(void)
{
    return &s_game_mine.page;
}
