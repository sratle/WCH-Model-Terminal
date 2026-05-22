/********************************** (C) COPYRIGHT *******************************
* File Name          : game_snake.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/20
* Description        : Classic Snake game.
*                      Grid-based movement, food spawning, score/level/best.
*                      Optimized dirty-rect partial refresh.
********************************************************************************/
#include "game_snake.h"
#include "../UI/ui_app_common.h"
#include <string.h>

/*=============================================================================
 *  Layout Configuration
 *
 *  Screen: 800x480, Title bar: 40px
 *  Game area: 800x440 (y: 40~480)
 *
 *  Layout:
 *    [Grid area]           [Panel: SCORE / LEVEL / BEST]
 *                          [Panel continued]
 *                          [D-pad controls]
 *
 *  Grid: 20x20 cells, cell=20px, total=400x400
 *  Grid centered vertically in game area
 *=============================================================================*/

#define SNK_COLS            20
#define SNK_ROWS            20
#define SNK_CELL            20      /* Cell size in pixels */
#define SNK_GRID_W          (SNK_COLS * SNK_CELL)   /* 400 */
#define SNK_GRID_H          (SNK_ROWS * SNK_CELL)   /* 400 */
#define SNK_GRID_X          15
#define SNK_GRID_Y          (APP_TITLE_BAR_H + (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - SNK_GRID_H) / 2)
                        /* = 40 + 20 = 60 */

/* Right panel */
#define SNK_PANEL_X         (SNK_GRID_X + SNK_GRID_W + 20)  /* 435 */
#define SNK_PANEL_W         (UI_SCREEN_WIDTH - SNK_PANEL_X - 15)  /* 350 */
#define SNK_PANEL_Y         (APP_TITLE_BAR_H + 10)  /* 50 */

/* Info box dimensions */
#define SNK_INFO_LABEL_H    14
#define SNK_INFO_BOX_H      28
#define SNK_INFO_BOX_R      6
#define SNK_INFO_GAP        6

/* Score section */
#define SNK_SCORE_LABEL_Y   SNK_PANEL_Y
#define SNK_SCORE_BOX_Y     (SNK_SCORE_LABEL_Y + SNK_INFO_LABEL_H)

/* Level section */
#define SNK_LEVEL_LABEL_Y   (SNK_SCORE_BOX_Y + SNK_INFO_BOX_H + SNK_INFO_GAP)
#define SNK_LEVEL_BOX_Y     (SNK_LEVEL_LABEL_Y + SNK_INFO_LABEL_H)

/* Best section */
#define SNK_BEST_LABEL_Y    (SNK_LEVEL_BOX_Y + SNK_INFO_BOX_H + SNK_INFO_GAP)
#define SNK_BEST_BOX_Y      (SNK_BEST_LABEL_Y + SNK_INFO_LABEL_H)

/* D-pad controls (inverted-T layout)
 *   Row 1:        [UP]
 *   Row 2: [LEFT] [DOWN] [RIGHT]
 */
#define SNK_DPAD_BTN        70
#define SNK_DPAD_GAP        4
#define SNK_DPAD_TOP        (SNK_BEST_BOX_Y + SNK_INFO_BOX_H + 30)

/* Control group total width: 3 buttons + 2 gaps */
#define SNK_CTRL_GROUP_W    (3 * SNK_DPAD_BTN + 2 * SNK_DPAD_GAP)  /* 170 */
#define SNK_CTRL_LEFT       (SNK_PANEL_X + (SNK_PANEL_W - SNK_CTRL_GROUP_W) / 2)

#define SNK_DPAD_UP_X       (SNK_CTRL_LEFT + SNK_DPAD_BTN + SNK_DPAD_GAP)
#define SNK_DPAD_UP_Y       SNK_DPAD_TOP
#define SNK_DPAD_MID_Y      (SNK_DPAD_TOP + SNK_DPAD_BTN + SNK_DPAD_GAP)
#define SNK_DPAD_LEFT_X     SNK_CTRL_LEFT
#define SNK_DPAD_LEFT_Y     SNK_DPAD_MID_Y
#define SNK_DPAD_DOWN_X     (SNK_CTRL_LEFT + SNK_DPAD_BTN + SNK_DPAD_GAP)
#define SNK_DPAD_DOWN_Y     SNK_DPAD_MID_Y
#define SNK_DPAD_RIGHT_X    (SNK_CTRL_LEFT + 2 * (SNK_DPAD_BTN + SNK_DPAD_GAP))
#define SNK_DPAD_RIGHT_Y    SNK_DPAD_MID_Y

/*=============================================================================
 *  Snake Colors
 *=============================================================================*/

#define SNK_BG_COLOR        UI_HEX(0x1A1A1A)   /* Game area background */
#define SNK_GRID_BG         UI_HEX(0x2C2C2C)   /* Grid cell background */
#define SNK_GRID_BORDER     UI_HEX(0x1A1A1A)   /* Grid cell border */
#define SNK_SNAKE_HEAD      UI_HEX(0x4CAF50)   /* Snake head color */
#define SNK_SNAKE_BODY      UI_HEX(0x66BB6A)   /* Snake body color */
#define SNK_SNAKE_TAIL      UI_HEX(0x81C784)   /* Snake tail (lighter) */
#define SNK_FOOD_COLOR      UI_HEX(0xF44336)   /* Food color */
#define SNK_FOOD_HIGHLIGHT  UI_HEX(0xEF5350)   /* Food highlight */

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    SNK_DIR_UP,
    SNK_DIR_DOWN,
    SNK_DIR_LEFT,
    SNK_DIR_RIGHT,
} snk_dir_t;

typedef enum {
    SNK_STATE_IDLE,
    SNK_STATE_PLAYING,
    SNK_STATE_GAMEOVER,
} snk_state_t;

/* Snake segment stored as grid coordinates */
typedef struct {
    int8_t col;
    int8_t row;
} snk_pos_t;

#define SNK_MAX_LENGTH  (SNK_COLS * SNK_ROWS)

typedef struct {
    snk_pos_t body[SNK_MAX_LENGTH];  /* body[0] = head */
    int16_t length;
    snk_dir_t dir;
    snk_dir_t next_dir;       /* Buffered next direction (prevent 180 turn) */
    snk_pos_t food;            /* Food position on grid */
    int score;
    int level;
    int best;
    snk_state_t state;
    uint32_t last_move_ms;
    char buf_score[16];
    char buf_level[16];
    char buf_best[16];
} snk_game_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_snake;
static ui_widget_t s_touch_area;
static ui_widget_t s_dpad_up, s_dpad_down, s_dpad_left, s_dpad_right;
static ui_widget_t *s_snk_widgets[7];
static snk_game_t s_snk;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void snk_draw_dpad(void);

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void snk_itoa(int val, char *out)
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

static int snk_rng_range(int min, int max)
{
    static uint32_t seed = 13579;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));
}

static void snk_update_texts(void)
{
    snk_itoa(s_snk.score, s_snk.buf_score);
    snk_itoa(s_snk.level, s_snk.buf_level);
    snk_itoa(s_snk.best, s_snk.buf_best);
}

/*=============================================================================
 *  Grid Pixel Helpers
 *=============================================================================*/

/* Get pixel rect for a grid cell */
static ui_rect_t snk_cell_rect(int row, int col)
{
    ui_rect_t r = {
        SNK_GRID_X + col * SNK_CELL,
        SNK_GRID_Y + row * SNK_CELL,
        SNK_CELL, SNK_CELL
    };
    return r;
}

/* Invalidate a single grid cell */
static void snk_inv_cell(int row, int col)
{
    if (row < 0 || row >= SNK_ROWS || col < 0 || col >= SNK_COLS) return;
    ui_rect_t r = snk_cell_rect(row, col);
    ui_page_invalidate(&r);
}

/* Invalidate score box */
static void snk_inv_score(void)
{
    ui_rect_t r = {SNK_PANEL_X, SNK_SCORE_BOX_Y, SNK_PANEL_W, SNK_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate level box */
static void snk_inv_level(void)
{
    ui_rect_t r = {SNK_PANEL_X, SNK_LEVEL_BOX_Y, SNK_PANEL_W, SNK_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate best box */
static void snk_inv_best(void)
{
    ui_rect_t r = {SNK_PANEL_X, SNK_BEST_BOX_Y, SNK_PANEL_W, SNK_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate entire grid */
static void __attribute__((unused)) snk_inv_grid(void)
{
    ui_rect_t r = {SNK_GRID_X, SNK_GRID_Y, SNK_GRID_W, SNK_GRID_H};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

/* Check if a position is occupied by the snake body */
static bool snk_is_body(int col, int row)
{
    for (int i = 0; i < s_snk.length; i++) {
        if (s_snk.body[i].col == col && s_snk.body[i].row == row)
            return true;
    }
    return false;
}

/* Spawn food at a random empty cell */
static void snk_spawn_food(void)
{
    /* Count empty cells */
    int empty_count = SNK_COLS * SNK_ROWS - s_snk.length;
    if (empty_count <= 0) return;

    int target = snk_rng_range(0, empty_count - 1);
    int idx = 0;
    for (int r = 0; r < SNK_ROWS; r++) {
        for (int c = 0; c < SNK_COLS; c++) {
            if (!snk_is_body(c, r)) {
                if (idx == target) {
                    s_snk.food.col = c;
                    s_snk.food.row = r;
                    return;
                }
                idx++;
            }
        }
    }
}

/* Move speed in ms based on level */
static uint32_t snk_move_interval(void)
{
    /* Level 0: 350ms, each level 20ms faster, min 80ms */
    uint32_t interval = 350 - s_snk.level * 20;
    return interval < 80 ? 80 : interval;
}

static void snk_start_game(void)
{
    memset(&s_snk, 0, sizeof(s_snk));
    s_snk.state = SNK_STATE_PLAYING;

    /* Start snake in the center, length 3, moving right */
    int start_col = SNK_COLS / 2;
    int start_row = SNK_ROWS / 2;
    s_snk.body[0].col = start_col;
    s_snk.body[0].row = start_row;
    s_snk.body[1].col = start_col - 1;
    s_snk.body[1].row = start_row;
    s_snk.body[2].col = start_col - 2;
    s_snk.body[2].row = start_row;
    s_snk.length = 3;
    s_snk.dir = SNK_DIR_RIGHT;
    s_snk.next_dir = SNK_DIR_RIGHT;

    snk_spawn_food();
    snk_update_texts();
    ui_page_invalidate_all();
    s_snk.last_move_ms = ui_get_real_ms();
}

static void snk_set_direction(snk_dir_t new_dir)
{
    /* Prevent 180-degree turns */
    if (s_snk.dir == SNK_DIR_UP && new_dir == SNK_DIR_DOWN) return;
    if (s_snk.dir == SNK_DIR_DOWN && new_dir == SNK_DIR_UP) return;
    if (s_snk.dir == SNK_DIR_LEFT && new_dir == SNK_DIR_RIGHT) return;
    if (s_snk.dir == SNK_DIR_RIGHT && new_dir == SNK_DIR_LEFT) return;
    s_snk.next_dir = new_dir;
}

static void snk_game_tick(void)
{
    if (s_snk.state != SNK_STATE_PLAYING) return;

    uint32_t now = ui_get_real_ms();
    if (now - s_snk.last_move_ms < snk_move_interval()) return;
    s_snk.last_move_ms = now;

    /* Apply buffered direction */
    s_snk.dir = s_snk.next_dir;

    /* Calculate new head position */
    snk_pos_t new_head = s_snk.body[0];
    switch (s_snk.dir) {
        case SNK_DIR_UP:    new_head.row--; break;
        case SNK_DIR_DOWN:  new_head.row++; break;
        case SNK_DIR_LEFT:  new_head.col--; break;
        case SNK_DIR_RIGHT: new_head.col++; break;
    }

    /* Check wall collision */
    if (new_head.col < 0 || new_head.col >= SNK_COLS ||
        new_head.row < 0 || new_head.row >= SNK_ROWS) {
        s_snk.state = SNK_STATE_GAMEOVER;
        if (s_snk.score > s_snk.best) s_snk.best = s_snk.score;
        snk_update_texts();
        ui_page_invalidate_all();
        return;
    }

    /* Check self collision (exclude tail, it will move away unless eating) */
    bool eating = (new_head.col == s_snk.food.col && new_head.row == s_snk.food.row);
    int check_len = eating ? s_snk.length : s_snk.length - 1;
    for (int i = 0; i < check_len; i++) {
        if (s_snk.body[i].col == new_head.col && s_snk.body[i].row == new_head.row) {
            s_snk.state = SNK_STATE_GAMEOVER;
            if (s_snk.score > s_snk.best) s_snk.best = s_snk.score;
            snk_update_texts();
            ui_page_invalidate_all();
            return;
        }
    }

    /* Invalidate old tail position (will be erased unless eating) */
    snk_pos_t old_tail = s_snk.body[s_snk.length - 1];
    /* Invalidate old head position (will become body) */
    snk_pos_t old_head = s_snk.body[0];

    /* Shift body: move from tail to head */
    if (!eating) {
        /* Invalidate old tail cell */
        snk_inv_cell(old_tail.row, old_tail.col);
        /* Shift body segments back */
        for (int i = s_snk.length - 1; i > 0; i--) {
            s_snk.body[i] = s_snk.body[i - 1];
        }
    } else {
        /* Eating: grow snake, don't remove tail */
        /* Shift body segments back, length increases */
        for (int i = s_snk.length; i > 0; i--) {
            s_snk.body[i] = s_snk.body[i - 1];
        }
        s_snk.length++;

        /* Score */
        s_snk.score += 10 * (s_snk.level + 1);
        s_snk.level = s_snk.score / 100;
        snk_update_texts();

        /* Invalidate old food position (will be redrawn as head) */
        snk_inv_cell(s_snk.food.row, s_snk.food.col);

        /* Invalidate score/level/best */
        snk_inv_score();
        snk_inv_level();
        snk_inv_best();

        /* Spawn new food */
        snk_spawn_food();
        /* Invalidate new food position */
        snk_inv_cell(s_snk.food.row, s_snk.food.col);
    }

    /* Set new head */
    s_snk.body[0] = new_head;

    /* Invalidate old head (needs redraw: was head, now body) */
    snk_inv_cell(old_head.row, old_head.col);
    /* Invalidate new head */
    snk_inv_cell(new_head.row, new_head.col);
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

/* Draw a single grid cell as background */
static void snk_draw_cell_bg(int row, int col)
{
    ui_rect_t r = snk_cell_rect(row, col);
    ui_draw_fill_rect(&r, SNK_GRID_BG);
    /* Subtle grid border */
    ui_draw_hline(r.x, r.y, SNK_CELL, SNK_GRID_BORDER);
    ui_draw_vline(r.x, r.y, SNK_CELL, SNK_GRID_BORDER);
}

/* Draw snake head at grid position */
static void snk_draw_head(int row, int col)
{
    ui_rect_t r = snk_cell_rect(row, col);
    ui_color_t fill = SNK_SNAKE_HEAD;
    ui_color_t highlight = ui_color_lighten(fill, 30);
    ui_color_t border = ui_color_darken(fill, 25);

    ui_draw_fill_rect(&r, fill);
    ui_draw_hline(r.x, r.y, SNK_CELL, highlight);
    ui_draw_vline(r.x, r.y, SNK_CELL, highlight);
    ui_draw_hline(r.x, r.y + SNK_CELL - 1, SNK_CELL, border);
    ui_draw_vline(r.x + SNK_CELL - 1, r.y, SNK_CELL, border);

    /* Draw eyes based on direction */
    int16_t cx = r.x + SNK_CELL / 2;
    int16_t cy = r.y + SNK_CELL / 2;
    ui_color_t eye_color = UI_HEX(0x1B5E20);

    switch (s_snk.dir) {
        case SNK_DIR_UP:
            ui_draw_pixel(cx - 4, cy - 3, eye_color);
            ui_draw_pixel(cx + 4, cy - 3, eye_color);
            break;
        case SNK_DIR_DOWN:
            ui_draw_pixel(cx - 4, cy + 3, eye_color);
            ui_draw_pixel(cx + 4, cy + 3, eye_color);
            break;
        case SNK_DIR_LEFT:
            ui_draw_pixel(cx - 3, cy - 4, eye_color);
            ui_draw_pixel(cx - 3, cy + 4, eye_color);
            break;
        case SNK_DIR_RIGHT:
            ui_draw_pixel(cx + 3, cy - 4, eye_color);
            ui_draw_pixel(cx + 3, cy + 4, eye_color);
            break;
    }
}

/* Draw snake body segment at grid position */
static void snk_draw_body(int row, int col)
{
    ui_rect_t r = snk_cell_rect(row, col);
    ui_color_t fill = SNK_SNAKE_BODY;
    ui_color_t highlight = ui_color_lighten(fill, 20);
    ui_color_t border = ui_color_darken(fill, 20);

    ui_draw_fill_rect(&r, fill);
    ui_draw_hline(r.x, r.y, SNK_CELL, highlight);
    ui_draw_vline(r.x, r.y, SNK_CELL, highlight);
    ui_draw_hline(r.x, r.y + SNK_CELL - 1, SNK_CELL, border);
    ui_draw_vline(r.x + SNK_CELL - 1, r.y, SNK_CELL, border);
}

/* Draw snake tail segment at grid position */
static void snk_draw_tail(int row, int col)
{
    ui_rect_t r = snk_cell_rect(row, col);
    ui_color_t fill = SNK_SNAKE_TAIL;
    ui_color_t highlight = ui_color_lighten(fill, 20);
    ui_color_t border = ui_color_darken(fill, 20);

    ui_draw_fill_rect(&r, fill);
    ui_draw_hline(r.x, r.y, SNK_CELL, highlight);
    ui_draw_vline(r.x, r.y, SNK_CELL, highlight);
    ui_draw_hline(r.x, r.y + SNK_CELL - 1, SNK_CELL, border);
    ui_draw_vline(r.x + SNK_CELL - 1, r.y, SNK_CELL, border);
}

/* Draw food at grid position */
static void snk_draw_food(int row, int col)
{
    int16_t cx = SNK_GRID_X + col * SNK_CELL + SNK_CELL / 2;
    int16_t cy = SNK_GRID_Y + row * SNK_CELL + SNK_CELL / 2;
    int16_t r = SNK_CELL / 2 - 2;

    ui_draw_fill_circle(cx, cy, r, SNK_FOOD_COLOR);
    /* Small highlight */
    ui_draw_pixel(cx - 2, cy - 2, SNK_FOOD_HIGHLIGHT);
}

/* Draw entire grid background */
static void snk_draw_grid_bg(void)
{
    for (int r = 0; r < SNK_ROWS; r++) {
        for (int c = 0; c < SNK_COLS; c++) {
            snk_draw_cell_bg(r, c);
        }
    }
}

/* Draw entire snake */
static void snk_draw_snake(void)
{
    for (int i = 0; i < s_snk.length; i++) {
        if (i == 0) {
            snk_draw_head(s_snk.body[i].row, s_snk.body[i].col);
        } else if (i == s_snk.length - 1) {
            snk_draw_tail(s_snk.body[i].row, s_snk.body[i].col);
        } else {
            snk_draw_body(s_snk.body[i].row, s_snk.body[i].col);
        }
    }
}

/* Draw food */
static void snk_draw_food_current(void)
{
    snk_draw_food(s_snk.food.row, s_snk.food.col);
}

/* Draw right panel */
static void snk_draw_panel(void)
{
    /* Score label */
    ui_rect_t sl = {SNK_PANEL_X, SNK_SCORE_LABEL_Y, SNK_PANEL_W, SNK_INFO_LABEL_H};
    ui_draw_text_in_rect(&sl, "SCORE", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Score box */
    ui_rect_t sb = {SNK_PANEL_X, SNK_SCORE_BOX_Y, SNK_PANEL_W, SNK_INFO_BOX_H};
    ui_draw_fill_round_rect(&sb, SNK_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&sb, s_snk.buf_score, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Level label */
    ui_rect_t ll = {SNK_PANEL_X, SNK_LEVEL_LABEL_Y, SNK_PANEL_W, SNK_INFO_LABEL_H};
    ui_draw_text_in_rect(&ll, "LEVEL", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Level box */
    ui_rect_t lb = {SNK_PANEL_X, SNK_LEVEL_BOX_Y, SNK_PANEL_W, SNK_INFO_BOX_H};
    ui_draw_fill_round_rect(&lb, SNK_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&lb, s_snk.buf_level, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Best label */
    ui_rect_t bl = {SNK_PANEL_X, SNK_BEST_LABEL_Y, SNK_PANEL_W, SNK_INFO_LABEL_H};
    ui_draw_text_in_rect(&bl, "BEST", &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Best box */
    ui_rect_t bb = {SNK_PANEL_X, SNK_BEST_BOX_Y, SNK_PANEL_W, SNK_INFO_BOX_H};
    ui_draw_fill_round_rect(&bb, SNK_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&bb, s_snk.buf_best, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* D-pad */
    snk_draw_dpad();
}

/* Draw D-pad control buttons */
static void snk_draw_dpad(void)
{
    ui_color_t btn_bg = UI_HEX(0x3C3C3C);
    ui_color_t btn_border = UI_HEX(0x555555);
    ui_color_t arrow_color = UI_COLOR_WHITE;

    /* Up */
    ui_rect_t up_r = {SNK_DPAD_UP_X, SNK_DPAD_UP_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_draw_fill_round_rect(&up_r, 6, btn_bg);
    ui_draw_round_rect_border(&up_r, 6, btn_border, 1);
    /* Up arrow */
    int16_t cx = SNK_DPAD_UP_X + SNK_DPAD_BTN / 2;
    int16_t cy = SNK_DPAD_UP_Y + SNK_DPAD_BTN / 2;
    ui_draw_line(cx - 8, cy + 5, cx, cy - 8, arrow_color);
    ui_draw_line(cx, cy - 8, cx + 8, cy + 5, arrow_color);

    /* Down */
    ui_rect_t dn_r = {SNK_DPAD_DOWN_X, SNK_DPAD_DOWN_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_draw_fill_round_rect(&dn_r, 6, btn_bg);
    ui_draw_round_rect_border(&dn_r, 6, btn_border, 1);
    cx = SNK_DPAD_DOWN_X + SNK_DPAD_BTN / 2;
    cy = SNK_DPAD_DOWN_Y + SNK_DPAD_BTN / 2;
    ui_draw_line(cx - 8, cy - 5, cx, cy + 8, arrow_color);
    ui_draw_line(cx, cy + 8, cx + 8, cy - 5, arrow_color);

    /* Left */
    ui_rect_t lt_r = {SNK_DPAD_LEFT_X, SNK_DPAD_LEFT_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_draw_fill_round_rect(&lt_r, 6, btn_bg);
    ui_draw_round_rect_border(&lt_r, 6, btn_border, 1);
    cx = SNK_DPAD_LEFT_X + SNK_DPAD_BTN / 2;
    cy = SNK_DPAD_LEFT_Y + SNK_DPAD_BTN / 2;
    ui_draw_line(cx + 5, cy - 8, cx - 8, cy, arrow_color);
    ui_draw_line(cx - 8, cy, cx + 5, cy + 8, arrow_color);

    /* Right */
    ui_rect_t rt_r = {SNK_DPAD_RIGHT_X, SNK_DPAD_RIGHT_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_draw_fill_round_rect(&rt_r, 6, btn_bg);
    ui_draw_round_rect_border(&rt_r, 6, btn_border, 1);
    cx = SNK_DPAD_RIGHT_X + SNK_DPAD_BTN / 2;
    cy = SNK_DPAD_RIGHT_Y + SNK_DPAD_BTN / 2;
    ui_draw_line(cx - 5, cy - 8, cx + 8, cy, arrow_color);
    ui_draw_line(cx + 8, cy, cx - 5, cy + 8, arrow_color);
}

/* Draw idle screen overlay */
static void snk_draw_idle_overlay(void)
{
    /* Semi-transparent overlay on grid */
    ui_rect_t overlay = {SNK_GRID_X, SNK_GRID_Y, SNK_GRID_W, SNK_GRID_H};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 180);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {SNK_GRID_X, SNK_GRID_Y + SNK_GRID_H / 2 - 30,
                       SNK_GRID_W, 30};
    ui_draw_text_in_rect(&title, "SNAKE", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t sub = {SNK_GRID_X, SNK_GRID_Y + SNK_GRID_H / 2 + 5,
                     SNK_GRID_W, 24};
    ui_draw_text_in_rect(&sub, "Swipe or press to start", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/* Draw game over overlay */
static void snk_draw_gameover_overlay(void)
{
    ui_rect_t overlay = {SNK_GRID_X, SNK_GRID_Y, SNK_GRID_W, SNK_GRID_H};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 180);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {SNK_GRID_X, SNK_GRID_Y + SNK_GRID_H / 2 - 30,
                       SNK_GRID_W, 30};
    ui_draw_text_in_rect(&title, "GAME OVER", &font_montserrat_16,
                         UI_HEX(0xF44336), 1);

    char buf[16];
    snk_itoa(s_snk.score, buf);
    ui_rect_t sr = {SNK_GRID_X, SNK_GRID_Y + SNK_GRID_H / 2 + 5,
                    SNK_GRID_W, 24};
    ui_draw_text_in_rect(&sr, buf, &font_montserrat_12, UI_COLOR_WHITE, 1);

    ui_rect_t hr = {SNK_GRID_X, SNK_GRID_Y + SNK_GRID_H / 2 + 35,
                    SNK_GRID_W, 24};
    ui_draw_text_in_rect(&hr, "Press to restart", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/*=============================================================================
 *  Input Handling
 *=============================================================================*/

static void snk_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
            snk_start_game();
        } else {
            snk_set_direction(SNK_DIR_UP);
        }
    } else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
            snk_start_game();
        } else {
            snk_set_direction(SNK_DIR_DOWN);
        }
    } else if (e->type == UI_EVENT_SWIPE_LEFT) {
        if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
            snk_start_game();
        } else {
            snk_set_direction(SNK_DIR_LEFT);
        }
    } else if (e->type == UI_EVENT_SWIPE_RIGHT) {
        if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
            snk_start_game();
        } else {
            snk_set_direction(SNK_DIR_RIGHT);
        }
    } else if (e->type == UI_EVENT_CLICK) {
        if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
            snk_start_game();
        }
    } else if (e->type == UI_EVENT_KEY_UP_ARROW) {
        snk_set_direction(SNK_DIR_UP);
    } else if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        snk_set_direction(SNK_DIR_DOWN);
    } else if (e->type == UI_EVENT_KEY_LEFT_ARROW) {
        snk_set_direction(SNK_DIR_LEFT);
    } else if (e->type == UI_EVENT_KEY_RIGHT_ARROW) {
        snk_set_direction(SNK_DIR_RIGHT);
    } else if (e->type == UI_EVENT_KEY_OK) {
        if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
            snk_start_game();
        }
    }
}

static void snk_dpad_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type != UI_EVENT_DOWN && e->type != UI_EVENT_LONG_PRESS &&
        e->type != UI_EVENT_LONG_PRESS_REPEAT) return;
    if (s_snk.state == SNK_STATE_IDLE || s_snk.state == SNK_STATE_GAMEOVER) {
        snk_start_game();
        return;
    }

    if (w == &s_dpad_up)         snk_set_direction(SNK_DIR_UP);
    else if (w == &s_dpad_down)  snk_set_direction(SNK_DIR_DOWN);
    else if (w == &s_dpad_left)  snk_set_direction(SNK_DIR_LEFT);
    else if (w == &s_dpad_right) snk_set_direction(SNK_DIR_RIGHT);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void snk_game_enter(ui_page_t *page)
{
    (void)page;
    memset(&s_snk, 0, sizeof(s_snk));
    s_snk.state = SNK_STATE_IDLE;
    snk_update_texts();
    ui_page_invalidate_all();
}

/* Per-frame game logic update */
static void snk_game_update(ui_page_t *page)
{
    (void)page;
    snk_game_tick();
}

/* Per-row game rendering (compositing renderer auto-clips to target row) */
static void snk_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Game area background */
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, SNK_BG_COLOR);

    if (s_snk.state == SNK_STATE_IDLE) {
        snk_draw_grid_bg();
        snk_draw_panel();
        snk_draw_idle_overlay();
    } else {
        snk_draw_grid_bg();
        snk_draw_snake();
        snk_draw_food_current();
        snk_draw_panel();

        if (s_snk.state == SNK_STATE_GAMEOVER) {
            snk_draw_gameover_overlay();
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_snake_init(void)
{
    ui_app_page_init(&s_game_snake, "Snake", 0x202);

    /* Touch area for swipe/click */
    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = snk_touch_event;

    /* D-pad buttons */
    ui_rect_t up_r = {SNK_DPAD_UP_X, SNK_DPAD_UP_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_widget_init(&s_dpad_up, &up_r);
    s_dpad_up.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_up.event_cb = snk_dpad_event;

    ui_rect_t dn_r = {SNK_DPAD_DOWN_X, SNK_DPAD_DOWN_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_widget_init(&s_dpad_down, &dn_r);
    s_dpad_down.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_down.event_cb = snk_dpad_event;

    ui_rect_t lt_r = {SNK_DPAD_LEFT_X, SNK_DPAD_LEFT_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_widget_init(&s_dpad_left, &lt_r);
    s_dpad_left.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_left.event_cb = snk_dpad_event;

    ui_rect_t rt_r = {SNK_DPAD_RIGHT_X, SNK_DPAD_RIGHT_Y, SNK_DPAD_BTN, SNK_DPAD_BTN};
    ui_widget_init(&s_dpad_right, &rt_r);
    s_dpad_right.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_right.event_cb = snk_dpad_event;

    /* Widget order: high index = high priority for events */
    s_snk_widgets[0] = &s_touch_area;
    s_snk_widgets[1] = &s_dpad_up;
    s_snk_widgets[2] = &s_dpad_down;
    s_snk_widgets[3] = &s_dpad_left;
    s_snk_widgets[4] = &s_dpad_right;
    s_snk_widgets[5] = (ui_widget_t *)&s_game_snake.lbl_title;
    s_snk_widgets[6] = (ui_widget_t *)&s_game_snake.btn_back;

    ui_page_set_widgets(&s_game_snake.page, s_snk_widgets, 7);
    ui_page_set_callbacks(&s_game_snake.page, snk_game_enter, NULL,
                          snk_game_draw, NULL);
    ui_page_set_update_cb(&s_game_snake.page, snk_game_update);
    ui_page_register(&s_game_snake.page);
}

ui_page_t *game_snake_get_page(void)
{
    return &s_game_snake.page;
}
