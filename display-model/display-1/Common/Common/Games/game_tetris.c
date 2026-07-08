/********************************** (C) COPYRIGHT *******************************
* File Name          : game_tetris.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/20
* Description        : Classic Tetris game.
*                      10x20 board, 7 tetrominoes, next piece preview,
*                      score/level/lines, soft drop, hard drop, wall kick.
*                      Optimized dirty-rect partial refresh.
********************************************************************************/
#include "game_tetris.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Board & Piece Configuration
 *
 *  Screen: 800x480, Title bar: 40px
 *  Game area: 800x440 (y: 40~480)
 *  Cell=20: board=200x400, vertical margin=(440-400)/2=20px each side
 *=============================================================================*/

#define TET_COLS            10
#define TET_ROWS            20
#define TET_CELL            20      /* Cell size in pixels */
#define TET_BOARD_W         (TET_COLS * TET_CELL)   /* 200 */
#define TET_BOARD_H         (TET_ROWS * TET_CELL)   /* 400 */
#define TET_GRID_X          15
#define TET_GRID_Y          (APP_TITLE_BAR_H + (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - TET_BOARD_H) / 2)
                        /* = 40 + 20 = 60 */

/* Right panel */
#define TET_PANEL_X         (TET_GRID_X + TET_BOARD_W + 20)  /* 235 */
#define TET_PANEL_W         (UI_SCREEN_WIDTH - TET_PANEL_X - 15)  /* 550 */
#define TET_PANEL_Y         (APP_TITLE_BAR_H + 10)  /* 50 */

/* Info box dimensions */
#define TET_INFO_LABEL_H    14
#define TET_INFO_BOX_H      28
#define TET_INFO_BOX_R      6       /* Border radius */
#define TET_INFO_GAP        6       /* Gap between info sections */

/* Score section */
#define TET_SCORE_LABEL_Y   TET_PANEL_Y                                  /* 50 */
#define TET_SCORE_BOX_Y     (TET_SCORE_LABEL_Y + TET_INFO_LABEL_H)       /* 64 */

/* Level section */
#define TET_LEVEL_LABEL_Y   (TET_SCORE_BOX_Y + TET_INFO_BOX_H + TET_INFO_GAP)  /* 98 */
#define TET_LEVEL_BOX_Y     (TET_LEVEL_LABEL_Y + TET_INFO_LABEL_H)       /* 112 */

/* Lines section */
#define TET_LINES_LABEL_Y   (TET_LEVEL_BOX_Y + TET_INFO_BOX_H + TET_INFO_GAP)  /* 146 */
#define TET_LINES_BOX_Y     (TET_LINES_LABEL_Y + TET_INFO_LABEL_H)       /* 160 */

/* Best section */
#define TET_BEST_LABEL_Y    (TET_LINES_BOX_Y + TET_INFO_BOX_H + TET_INFO_GAP)  /* 194 */
#define TET_BEST_BOX_Y      (TET_BEST_LABEL_Y + TET_INFO_LABEL_H)       /* 208 */

/* Next piece preview */
#define TET_NEXT_LABEL_Y    (TET_BEST_BOX_Y + TET_INFO_BOX_H + 12)      /* 248 */
#define TET_PREVIEW_X       TET_PANEL_X
#define TET_PREVIEW_Y       (TET_NEXT_LABEL_Y + TET_INFO_LABEL_H)        /* 214 */
#define TET_PREVIEW_W       TET_PANEL_W
#define TET_PREVIEW_H       (4 * 16 + 12)  /* 76: 4 rows of 16px cells + 12px padding */
#define TET_PREVIEW_CELL    16      /* Smaller cell for preview */

/* D-pad + Drop (inverted-T with DROP on the right)
 *   Row 1:        [UP]
 *   Row 2: [LEFT] [DOWN] [RIGHT] [DROP]
 *   All buttons same size, DROP is square
 */
#define TET_DPAD_BTN        54
#define TET_DPAD_GAP        4
#define TET_DPAD_TOP        (TET_PREVIEW_Y + TET_PREVIEW_H + 20)  /* 310 */

/* Control group total width: 4 buttons + 3 gaps */
#define TET_CTRL_GROUP_W    (4 * TET_DPAD_BTN + 3 * TET_DPAD_GAP)  /* 228 */
#define TET_CTRL_LEFT       (TET_PANEL_X + (TET_PANEL_W - TET_CTRL_GROUP_W) / 2)

#define TET_DPAD_UP_X       (TET_CTRL_LEFT + TET_DPAD_BTN + TET_DPAD_GAP)
#define TET_DPAD_UP_Y       TET_DPAD_TOP
#define TET_DPAD_MID_Y      (TET_DPAD_TOP + TET_DPAD_BTN + TET_DPAD_GAP)  /* 368 */
#define TET_DPAD_LEFT_X     TET_CTRL_LEFT
#define TET_DPAD_LEFT_Y     TET_DPAD_MID_Y
#define TET_DPAD_DOWN_X     (TET_CTRL_LEFT + TET_DPAD_BTN + TET_DPAD_GAP)
#define TET_DPAD_DOWN_Y     TET_DPAD_MID_Y
#define TET_DPAD_RIGHT_X    (TET_CTRL_LEFT + 2 * (TET_DPAD_BTN + TET_DPAD_GAP))
#define TET_DPAD_RIGHT_Y    TET_DPAD_MID_Y

/* Drop button (square, same row as L/D/R) */
#define TET_DROP_W          TET_DPAD_BTN
#define TET_DROP_H          TET_DPAD_BTN
#define TET_DROP_X          (TET_CTRL_LEFT + 3 * (TET_DPAD_BTN + TET_DPAD_GAP))
#define TET_DROP_Y          TET_DPAD_MID_Y
                        /* bottom = 368+54 = 422 < 480 */

/*=============================================================================
 *  Tetromino Definitions
 *=============================================================================*/

/* Piece shapes: 4 rotations x 4 cells (row, col) relative to piece origin */
typedef struct {
    int8_t cells[4][4][2];  /* [rotation][cell_idx][row, col] */
    ui_color_t color;
} tet_piece_def_t;

/* 7 standard tetrominoes */
static const tet_piece_def_t s_pieces[7] = {
    /* I-piece (cyan) */
    { .cells = {
        {{0,0},{0,1},{0,2},{0,3}}, {{0,0},{1,0},{2,0},{3,0}},
        {{0,0},{0,1},{0,2},{0,3}}, {{0,0},{1,0},{2,0},{3,0}}
    }, .color = UI_HEX(0x00BCD4) },
    /* O-piece (yellow) */
    { .cells = {
        {{0,0},{0,1},{1,0},{1,1}}, {{0,0},{0,1},{1,0},{1,1}},
        {{0,0},{0,1},{1,0},{1,1}}, {{0,0},{0,1},{1,0},{1,1}}
    }, .color = UI_HEX(0xFFEB3B) },
    /* T-piece (purple) */
    { .cells = {
        {{0,0},{0,1},{0,2},{1,1}}, {{0,0},{1,0},{2,0},{1,1}},
        {{1,0},{1,1},{1,2},{0,1}}, {{0,1},{1,0},{1,1},{2,1}}
    }, .color = UI_HEX(0x9C27B0) },
    /* S-piece (green) */
    { .cells = {
        {{0,1},{0,2},{1,0},{1,1}}, {{0,0},{1,0},{1,1},{2,1}},
        {{0,1},{0,2},{1,0},{1,1}}, {{0,0},{1,0},{1,1},{2,1}}
    }, .color = UI_HEX(0x4CAF50) },
    /* Z-piece (red) */
    { .cells = {
        {{0,0},{0,1},{1,1},{1,2}}, {{0,1},{1,0},{1,1},{2,0}},
        {{0,0},{0,1},{1,1},{1,2}}, {{0,1},{1,0},{1,1},{2,0}}
    }, .color = UI_HEX(0xF44336) },
    /* J-piece (blue) */
    { .cells = {
        {{0,0},{1,0},{1,1},{1,2}}, {{0,0},{0,1},{1,0},{2,0}},
        {{0,0},{0,1},{0,2},{1,2}}, {{0,1},{1,1},{2,0},{2,1}}
    }, .color = UI_HEX(0x2196F3) },
    /* L-piece (orange) */
    { .cells = {
        {{0,2},{1,0},{1,1},{1,2}}, {{0,0},{1,0},{2,0},{2,1}},
        {{0,0},{0,1},{0,2},{1,0}}, {{0,0},{0,1},{1,1},{2,1}}
    }, .color = UI_HEX(0xFF9800) },
};

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    TET_STATE_IDLE,
    TET_STATE_PLAYING,
    TET_STATE_PAUSED,
    TET_STATE_GAMEOVER,
} tet_state_t;

typedef struct {
    uint8_t board[TET_ROWS][TET_COLS];  /* 0=empty, 1-7=piece color index */
    int8_t cur_piece;                    /* 0-6 piece index */
    int8_t cur_rot;                      /* 0-3 rotation */
    int8_t cur_row, cur_col;             /* piece position on board */
    int8_t next_piece;
    int score;
    int level;
    int lines;
    int best;
    tet_state_t state;
    uint32_t last_drop_ms;
    char buf_score[16];
    char buf_level[16];
    char buf_lines[16];
    char buf_best[16];
} tet_game_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_tetris;
static ui_widget_t s_touch_area;
static ui_widget_t s_dpad_up, s_dpad_down, s_dpad_left, s_dpad_right;
static ui_widget_t s_drop_btn;
static ui_widget_t *s_tet_widgets[8];
static tet_game_t s_tet;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void tet_draw_dpad(void);
static void tet_draw_drop_btn(void);

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void tet_itoa(int val, char *out)
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

static int tet_rng_range(int min, int max)
{
    static uint32_t seed = 54321;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));
}

static int tet_random_piece(void)
{
    return tet_rng_range(0, 6);
}

/*=============================================================================
 *  Piece Cell Access
 *=============================================================================*/

/* Get the (row, col) of cell idx for piece p with rotation r */
static void tet_piece_cell(int p, int r, int idx, int8_t *row, int8_t *col)
{
    *row = s_pieces[p].cells[r][idx][0];
    *col = s_pieces[p].cells[r][idx][1];
}

/* Get color for piece index (1-7 stored in board) */
static ui_color_t tet_piece_color(int piece_idx)
{
    if (piece_idx < 1 || piece_idx > 7) return UI_COLOR_GRAY;
    return s_pieces[piece_idx - 1].color;
}

/*=============================================================================
 *  Collision Detection
 *=============================================================================*/

static bool tet_collides(int piece, int rot, int row, int col)
{
    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(piece, rot, i, &cr, &cc);
        int br = row + cr;
        int bc = col + cc;
        if (bc < 0 || bc >= TET_COLS || br >= TET_ROWS) return true;
        if (br < 0) continue;  /* above board is OK */
        if (s_tet.board[br][bc] != 0) return true;
    }
    return false;
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

static void tet_update_texts(void)
{
    tet_itoa(s_tet.score, s_tet.buf_score);
    tet_itoa(s_tet.level, s_tet.buf_level);
    tet_itoa(s_tet.lines, s_tet.buf_lines);
    tet_itoa(s_tet.best, s_tet.buf_best);
}

/*=============================================================================
 *  Best Score Persistence (via CLI passthrough to Core appcfg)
 *=============================================================================*/

static void tet_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    if (!tag || strcmp(tag, "appcfg") != 0) return;
    s_tet.best = atoi(buf);
    tet_update_texts();
    ui_page_invalidate_all();
}

static const uart_cli_cb_t s_tet_cli_cb = {
    .on_cli_complete = tet_on_cli_complete,
};

static void tet_load_best(void)
{
    UART_SetCLICallbacks(&s_tet_cli_cb);
    UART_SendCLI("appcfg get game_tetris highscore");
}

static void tet_save_best(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "appcfg set game_tetris highscore %d", s_tet.best);
    UART_SendCLI(cmd);
}

/* Drop speed in ms based on level */
static uint32_t tet_drop_interval(void)
{
    uint32_t speeds[] = {800, 720, 630, 550, 470, 380, 300, 220, 150, 100, 80, 60, 50, 40, 30};
    if (s_tet.level >= 15) return 20;
    return speeds[s_tet.level];
}

static void tet_spawn_piece(void)
{
    s_tet.cur_piece = s_tet.next_piece;
    s_tet.next_piece = tet_random_piece();
    s_tet.cur_rot = 0;
    s_tet.cur_row = 0;
    s_tet.cur_col = (TET_COLS / 2) - 1;

    /* Check game over */
    if (tet_collides(s_tet.cur_piece, s_tet.cur_rot, s_tet.cur_row, s_tet.cur_col)) {
        s_tet.state = TET_STATE_GAMEOVER;
        if (s_tet.score > s_tet.best) {
            s_tet.best = s_tet.score;
            tet_save_best();
        }
        ui_page_invalidate_all();
    }
}

static void tet_lock_piece(void)
{
    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(s_tet.cur_piece, s_tet.cur_rot, i, &cr, &cc);
        int br = s_tet.cur_row + cr;
        int bc = s_tet.cur_col + cc;
        if (br >= 0 && br < TET_ROWS && bc >= 0 && bc < TET_COLS) {
            s_tet.board[br][bc] = s_tet.cur_piece + 1;
        }
    }
}

/* Clear completed lines, return number cleared */
static int tet_clear_lines(void)
{
    int cleared = 0;
    for (int r = TET_ROWS - 1; r >= 0; r--) {
        bool full = true;
        for (int c = 0; c < TET_COLS; c++) {
            if (s_tet.board[r][c] == 0) { full = false; break; }
        }
        if (full) {
            cleared++;
            /* Shift rows down */
            for (int rr = r; rr > 0; rr--) {
                memcpy(s_tet.board[rr], s_tet.board[rr - 1], TET_COLS);
            }
            memset(s_tet.board[0], 0, TET_COLS);
            r++;  /* Re-check this row */
        }
    }
    return cleared;
}

static void tet_add_score(int lines_cleared)
{
    static const int score_table[] = {0, 100, 300, 500, 800};
    if (lines_cleared < 1 || lines_cleared > 4) return;
    s_tet.score += score_table[lines_cleared] * (s_tet.level + 1);
    s_tet.lines += lines_cleared;
    s_tet.level = s_tet.lines / 10;
}

static bool tet_move(int dcol, int drow)
{
    if (!tet_collides(s_tet.cur_piece, s_tet.cur_rot,
                      s_tet.cur_row + drow, s_tet.cur_col + dcol)) {
        s_tet.cur_row += drow;
        s_tet.cur_col += dcol;
        return true;
    }
    return false;
}

static bool tet_rotate(void)
{
    int new_rot = (s_tet.cur_rot + 1) % 4;
    /* Try basic rotation */
    if (!tet_collides(s_tet.cur_piece, new_rot, s_tet.cur_row, s_tet.cur_col)) {
        s_tet.cur_rot = new_rot;
        return true;
    }
    /* Wall kick: try left/right offsets */
    for (int kick = -1; kick <= 1; kick += 2) {
        if (!tet_collides(s_tet.cur_piece, new_rot, s_tet.cur_row, s_tet.cur_col + kick)) {
            s_tet.cur_rot = new_rot;
            s_tet.cur_col += kick;
            return true;
        }
    }
    /* Try 2-offset for I-piece */
    if (!tet_collides(s_tet.cur_piece, new_rot, s_tet.cur_row, s_tet.cur_col + 2)) {
        s_tet.cur_rot = new_rot;
        s_tet.cur_col += 2;
        return true;
    }
    if (!tet_collides(s_tet.cur_piece, new_rot, s_tet.cur_row, s_tet.cur_col - 2)) {
        s_tet.cur_rot = new_rot;
        s_tet.cur_col -= 2;
        return true;
    }
    return false;
}

/* Calculate ghost piece row (hard drop destination) */
static int tet_ghost_row(void)
{
    int row = s_tet.cur_row;
    while (!tet_collides(s_tet.cur_piece, s_tet.cur_rot, row + 1, s_tet.cur_col)) {
        row++;
    }
    return row;
}

static void tet_hard_drop(void)
{
    int ghost = tet_ghost_row();
    s_tet.score += (ghost - s_tet.cur_row) * 2;
    s_tet.cur_row = ghost;
}

static void tet_start_game(void)
{
    int saved_best = s_tet.best;
    memset(&s_tet, 0, sizeof(s_tet));
    s_tet.best = saved_best;
    s_tet.state = TET_STATE_PLAYING;
    s_tet.next_piece = tet_random_piece();
    tet_spawn_piece();
    tet_update_texts();
    ui_page_invalidate_all();
    s_tet.last_drop_ms = ui_get_real_ms();
}

/*=============================================================================
 *  Dirty Region Helpers
 *=============================================================================*/

/* Cell pixel rect on board */
static ui_rect_t tet_cell_rect(int row, int col)
{
    ui_rect_t r = {
        TET_GRID_X + col * TET_CELL,
        TET_GRID_Y + row * TET_CELL,
        TET_CELL, TET_CELL
    };
    return r;
}

/* Invalidate a single board cell */
static void tet_inv_cell(int row, int col)
{
    ui_rect_t r = tet_cell_rect(row, col);
    ui_page_invalidate(&r);
}

/* Invalidate the current piece (all 4 cells) */
static void tet_inv_cur_piece(void)
{
    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(s_tet.cur_piece, s_tet.cur_rot, i, &cr, &cc);
        int br = s_tet.cur_row + cr;
        int bc = s_tet.cur_col + cc;
        if (br >= 0) tet_inv_cell(br, bc);
    }
}

/* Invalidate ghost piece */
static void tet_inv_ghost(void)
{
    int ghost = tet_ghost_row();
    if (ghost == s_tet.cur_row) return;
    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(s_tet.cur_piece, s_tet.cur_rot, i, &cr, &cc);
        int br = ghost + cr;
        int bc = s_tet.cur_col + cc;
        if (br >= 0) tet_inv_cell(br, bc);
    }
}

/* Invalidate score box */
static void tet_inv_score(void)
{
    ui_rect_t r = {TET_PANEL_X, TET_SCORE_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate level box */
static void tet_inv_level(void)
{
    ui_rect_t r = {TET_PANEL_X, TET_LEVEL_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate lines box */
static void tet_inv_lines(void)
{
    ui_rect_t r = {TET_PANEL_X, TET_LINES_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_page_invalidate(&r);
}

/* Invalidate next piece preview */
static void tet_inv_preview(void)
{
    ui_rect_t r = {TET_PREVIEW_X, TET_PREVIEW_Y, TET_PREVIEW_W, TET_PREVIEW_H};
    ui_page_invalidate(&r);
}

/* Invalidate entire board */
static void tet_inv_board(void)
{
    ui_rect_t r = {TET_GRID_X, TET_GRID_Y, TET_COLS * TET_CELL, TET_ROWS * TET_CELL};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static ui_color_t tet_cell_color(uint8_t val)
{
    if (val == 0) return UI_HEX(0x2C2C2C);
    return tet_piece_color(val);
}

static ui_color_t tet_cell_border_color(uint8_t val)
{
    if (val == 0) return UI_HEX(0x1A1A1A);
    return ui_color_darken(tet_piece_color(val), 25);
}

static ui_color_t tet_cell_highlight_color(uint8_t val)
{
    if (val == 0) return UI_HEX(0x333333);
    return ui_color_lighten(tet_piece_color(val), 30);
}

/* Draw a single cell at pixel position (x, y) */
static void tet_draw_cell(int16_t x, int16_t y, ui_color_t fill, ui_color_t border, ui_color_t highlight)
{
    ui_rect_t r = {x, y, TET_CELL, TET_CELL};
    ui_draw_fill_rect(&r, fill);
    /* Top & left highlight */
    ui_draw_hline(x, y, TET_CELL, highlight);
    ui_draw_vline(x, y, TET_CELL, highlight);
    /* Bottom & right border */
    ui_draw_hline(x, y + TET_CELL - 1, TET_CELL, border);
    ui_draw_vline(x + TET_CELL - 1, y, TET_CELL, border);
}

/* Draw a single cell on the board */
static void tet_draw_board_cell(int row, int col)
{
    int16_t x = TET_GRID_X + col * TET_CELL;
    int16_t y = TET_GRID_Y + row * TET_CELL;
    uint8_t val = s_tet.board[row][col];
    tet_draw_cell(x, y, tet_cell_color(val), tet_cell_border_color(val), tet_cell_highlight_color(val));
}

/* Draw the entire board */
static void tet_draw_board(void)
{
    for (int r = 0; r < TET_ROWS; r++) {
        for (int c = 0; c < TET_COLS; c++) {
            tet_draw_board_cell(r, c);
        }
    }
}

/* Draw current piece and ghost */
static void tet_draw_piece(void)
{
    /* Ghost piece */
    int ghost = tet_ghost_row();
    if (ghost != s_tet.cur_row) {
        ui_color_t ghost_fill = ui_color_darken(tet_piece_color(s_tet.cur_piece + 1), 60);
        ui_color_t ghost_border = ui_color_darken(tet_piece_color(s_tet.cur_piece + 1), 70);
        for (int i = 0; i < 4; i++) {
            int8_t cr, cc;
            tet_piece_cell(s_tet.cur_piece, s_tet.cur_rot, i, &cr, &cc);
            int br = ghost + cr;
            int bc = s_tet.cur_col + cc;
            if (br >= 0 && br < TET_ROWS && bc >= 0 && bc < TET_COLS) {
                int16_t x = TET_GRID_X + bc * TET_CELL;
                int16_t y = TET_GRID_Y + br * TET_CELL;
                ui_rect_t r = {x, y, TET_CELL, TET_CELL};
                ui_draw_fill_rect(&r, ghost_fill);
                ui_draw_rect_border(&r, ghost_border, 1);
            }
        }
    }

    /* Current piece */
    ui_color_t fill = tet_piece_color(s_tet.cur_piece + 1);
    ui_color_t border = ui_color_darken(fill, 25);
    ui_color_t highlight = ui_color_lighten(fill, 30);
    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(s_tet.cur_piece, s_tet.cur_rot, i, &cr, &cc);
        int br = s_tet.cur_row + cr;
        int bc = s_tet.cur_col + cc;
        if (br >= 0 && br < TET_ROWS && bc >= 0 && bc < TET_COLS) {
            int16_t x = TET_GRID_X + bc * TET_CELL;
            int16_t y = TET_GRID_Y + br * TET_CELL;
            tet_draw_cell(x, y, fill, border, highlight);
        }
    }
}

/* Draw next piece preview */
static void tet_draw_preview(void)
{
    ui_rect_t bg = {TET_PREVIEW_X, TET_PREVIEW_Y, TET_PREVIEW_W, TET_PREVIEW_H};
    ui_draw_fill_round_rect(&bg, TET_INFO_BOX_R, UI_HEX(0x3C3C3C));

    /* Center the preview piece */
    /* Find piece bounds */
    int min_r = 4, max_r = 0, min_c = 4, max_c = 0;
    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(s_tet.next_piece, 0, i, &cr, &cc);
        if (cr < min_r) min_r = cr;
        if (cr > max_r) max_r = cr;
        if (cc < min_c) min_c = cc;
        if (cc > max_c) max_c = cc;
    }
    int pw = (max_c - min_c + 1) * TET_PREVIEW_CELL;
    int ph = (max_r - min_r + 1) * TET_PREVIEW_CELL;
    int16_t ox = TET_PREVIEW_X + (TET_PREVIEW_W - pw) / 2 - min_c * TET_PREVIEW_CELL;
    int16_t oy = TET_PREVIEW_Y + (TET_PREVIEW_H - ph) / 2 - min_r * TET_PREVIEW_CELL;

    ui_color_t fill = tet_piece_color(s_tet.next_piece + 1);
    ui_color_t border = ui_color_darken(fill, 25);
    ui_color_t highlight = ui_color_lighten(fill, 30);

    for (int i = 0; i < 4; i++) {
        int8_t cr, cc;
        tet_piece_cell(s_tet.next_piece, 0, i, &cr, &cc);
        int16_t x = ox + cc * TET_PREVIEW_CELL;
        int16_t y = oy + cr * TET_PREVIEW_CELL;
        ui_rect_t r = {x, y, TET_PREVIEW_CELL, TET_PREVIEW_CELL};
        ui_draw_fill_rect(&r, fill);
        ui_draw_hline(x, y, TET_PREVIEW_CELL, highlight);
        ui_draw_vline(x, y, TET_PREVIEW_CELL, highlight);
        ui_draw_hline(x, y + TET_PREVIEW_CELL - 1, TET_PREVIEW_CELL, border);
        ui_draw_vline(x + TET_PREVIEW_CELL - 1, y, TET_PREVIEW_CELL, border);
    }
}

/* Draw right panel (score, level, lines, preview, d-pad, drop) */
static void tet_draw_panel(void)
{
    /* Score */
    ui_draw_text(TET_PANEL_X, TET_SCORE_LABEL_Y, "SCORE", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t score_bg = {TET_PANEL_X, TET_SCORE_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_draw_fill_round_rect(&score_bg, TET_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&score_bg, s_tet.buf_score, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Level */
    ui_draw_text(TET_PANEL_X, TET_LEVEL_LABEL_Y, "LEVEL", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t level_bg = {TET_PANEL_X, TET_LEVEL_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_draw_fill_round_rect(&level_bg, TET_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&level_bg, s_tet.buf_level, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Lines */
    ui_draw_text(TET_PANEL_X, TET_LINES_LABEL_Y, "LINES", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t lines_bg = {TET_PANEL_X, TET_LINES_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_draw_fill_round_rect(&lines_bg, TET_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&lines_bg, s_tet.buf_lines, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Best */
    ui_draw_text(TET_PANEL_X, TET_BEST_LABEL_Y, "BEST", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t best_bg = {TET_PANEL_X, TET_BEST_BOX_Y, TET_PANEL_W, TET_INFO_BOX_H};
    ui_draw_fill_round_rect(&best_bg, TET_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&best_bg, s_tet.buf_best, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Next label */
    ui_draw_text(TET_PANEL_X, TET_NEXT_LABEL_Y, "NEXT", &font_montserrat_12, UI_HEX(0xAAAAAA));
    tet_draw_preview();

    /* D-pad */
    tet_draw_dpad();

    /* Drop button */
    tet_draw_drop_btn();
}

static void tet_draw_dpad(void)
{
    ui_color_t bg = UI_HEX(0x3C3C3C);
    ui_color_t fg = UI_COLOR_WHITE;

    ui_rect_t up_r = {TET_DPAD_UP_X, TET_DPAD_UP_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_draw_fill_round_rect(&up_r, 6, bg);
    ui_draw_text_in_rect(&up_r, "^", &font_montserrat_16, fg, 1);

    ui_rect_t dn_r = {TET_DPAD_DOWN_X, TET_DPAD_DOWN_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_draw_fill_round_rect(&dn_r, 6, bg);
    ui_draw_text_in_rect(&dn_r, "v", &font_montserrat_16, fg, 1);

    ui_rect_t lt_r = {TET_DPAD_LEFT_X, TET_DPAD_LEFT_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_draw_fill_round_rect(&lt_r, 6, bg);
    ui_draw_text_in_rect(&lt_r, "<", &font_montserrat_16, fg, 1);

    ui_rect_t rt_r = {TET_DPAD_RIGHT_X, TET_DPAD_RIGHT_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_draw_fill_round_rect(&rt_r, 6, bg);
    ui_draw_text_in_rect(&rt_r, ">", &font_montserrat_16, fg, 1);
}

static void tet_draw_drop_btn(void)
{
    ui_rect_t r = {TET_DROP_X, TET_DROP_Y, TET_DROP_W, TET_DROP_H};
    ui_draw_fill_round_rect(&r, 6, UI_HEX(0x00BCD4));
    ui_draw_text_in_rect(&r, "DROP", &font_montserrat_12, UI_COLOR_WHITE, 1);
}

/* Draw idle screen with overlay */
static void tet_draw_idle_screen(void)
{
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, UI_HEX(0x1A1A1A));

    tet_draw_board();
    tet_draw_panel();

    /* Overlay on board */
    ui_rect_t overlay = {TET_GRID_X, TET_GRID_Y,
                         TET_COLS * TET_CELL, TET_ROWS * TET_CELL};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 160);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {TET_GRID_X, TET_GRID_Y + TET_ROWS * TET_CELL / 2 - 30,
                       TET_COLS * TET_CELL, 30};
    ui_draw_text_in_rect(&title, "TETRIS", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t sub = {TET_GRID_X, TET_GRID_Y + TET_ROWS * TET_CELL / 2 + 5,
                     TET_COLS * TET_CELL, 24};
    ui_draw_text_in_rect(&sub, "Press to start", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/* Draw game over overlay */
static void tet_draw_gameover_overlay(void)
{
    ui_rect_t overlay = {TET_GRID_X, TET_GRID_Y,
                         TET_COLS * TET_CELL, TET_ROWS * TET_CELL};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 180);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title = {TET_GRID_X, TET_GRID_Y + TET_ROWS * TET_CELL / 2 - 30,
                       TET_COLS * TET_CELL, 30};
    ui_draw_text_in_rect(&title, "GAME OVER", &font_montserrat_16, UI_HEX(0xF44336), 1);

    char buf[16];
    tet_itoa(s_tet.score, buf);
    ui_rect_t sr = {TET_GRID_X, TET_GRID_Y + TET_ROWS * TET_CELL / 2 + 5,
                    TET_COLS * TET_CELL, 24};
    ui_draw_text_in_rect(&sr, buf, &font_montserrat_12, UI_COLOR_WHITE, 1);

    ui_rect_t hr = {TET_GRID_X, TET_GRID_Y + TET_ROWS * TET_CELL / 2 + 35,
                    TET_COLS * TET_CELL, 24};
    ui_draw_text_in_rect(&hr, "Press to restart", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/*=============================================================================
 *  Game Tick (auto-drop)
 *=============================================================================*/

static void tet_game_tick(void)
{
    if (s_tet.state != TET_STATE_PLAYING) return;

    uint32_t now = ui_get_real_ms();
    if (now - s_tet.last_drop_ms >= tet_drop_interval()) {
        s_tet.last_drop_ms = now;

        /* Invalidate old piece position + ghost */
        tet_inv_cur_piece();
        tet_inv_ghost();

        if (!tet_move(0, 1)) {
            /* Lock piece */
            tet_lock_piece();
            int cleared = tet_clear_lines();
            if (cleared > 0) {
                tet_add_score(cleared);
                tet_update_texts();
                /* Invalidate entire board (rows shifted) + panel */
                tet_inv_board();
                tet_inv_score();
                tet_inv_level();
                tet_inv_lines();
            }
            tet_spawn_piece();
            tet_inv_preview();
        }

        /* Invalidate new piece position + ghost */
        tet_inv_cur_piece();
        tet_inv_ghost();
    }
}

/*=============================================================================
 *  Input Handling
 *=============================================================================*/

static void tet_do_move_action(int dcol, int drow)
{
    if (s_tet.state != TET_STATE_PLAYING) return;

    /* Invalidate old position */
    tet_inv_cur_piece();
    tet_inv_ghost();

    if (tet_move(dcol, drow)) {
        if (drow > 0) s_tet.score += 1;  /* Soft drop bonus */
        tet_update_texts();
        tet_inv_score();
    }

    /* Invalidate new position */
    tet_inv_cur_piece();
    tet_inv_ghost();
}

static void tet_do_rotate(void)
{
    if (s_tet.state != TET_STATE_PLAYING) return;

    tet_inv_cur_piece();
    tet_inv_ghost();

    tet_rotate();

    tet_inv_cur_piece();
    tet_inv_ghost();
}

static void tet_do_hard_drop(void)
{
    if (s_tet.state != TET_STATE_PLAYING) return;

    tet_inv_cur_piece();
    tet_inv_ghost();

    tet_hard_drop();
    tet_lock_piece();

    int cleared = tet_clear_lines();
    if (cleared > 0) {
        tet_add_score(cleared);
        tet_update_texts();
        tet_inv_board();
        tet_inv_score();
        tet_inv_level();
        tet_inv_lines();
    }

    tet_spawn_piece();
    tet_inv_preview();
    tet_inv_cur_piece();
    tet_inv_ghost();
    tet_update_texts();
    tet_inv_score();
}

static void tet_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    /* Keyboard events */
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER)
            tet_start_game();
        else
            tet_do_rotate();
        return;
    } else if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER)
            tet_start_game();
        else
            tet_do_move_action(0, 1);
        return;
    } else if (e->type == UI_EVENT_KEY_LEFT_ARROW) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER)
            tet_start_game();
        else
            tet_do_move_action(-1, 0);
        return;
    } else if (e->type == UI_EVENT_KEY_RIGHT_ARROW) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER)
            tet_start_game();
        else
            tet_do_move_action(1, 0);
        return;
    } else if (e->type == UI_EVENT_KEY_OK) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER)
            tet_start_game();
        else
            tet_do_hard_drop();
        return;
    }

    /* Touch / swipe events */
    if (e->type == UI_EVENT_SWIPE_LEFT) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
            tet_start_game();
        } else {
            tet_do_move_action(-1, 0);
        }
    } else if (e->type == UI_EVENT_SWIPE_RIGHT) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
            tet_start_game();
        } else {
            tet_do_move_action(1, 0);
        }
    } else if (e->type == UI_EVENT_SWIPE_DOWN) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
            tet_start_game();
        } else {
            tet_do_move_action(0, 1);
        }
    } else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
            tet_start_game();
        } else {
            tet_do_hard_drop();
        }
    } else if (e->type == UI_EVENT_CLICK) {
        if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
            tet_start_game();
        } else {
            /* Only rotate when tapping on the board area */
            int16_t tx = e->pos.x;
            int16_t ty = e->pos.y;
            if (tx >= TET_GRID_X && tx < TET_GRID_X + TET_BOARD_W &&
                ty >= TET_GRID_Y && ty < TET_GRID_Y + TET_BOARD_H) {
                tet_do_rotate();
            }
        }
    }
}

static void tet_dpad_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type != UI_EVENT_CLICK) return;
    if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
        tet_start_game();
        return;
    }

    if (w == &s_dpad_up)         tet_do_rotate();
    else if (w == &s_dpad_down)  tet_do_move_action(0, 1);
    else if (w == &s_dpad_left)  tet_do_move_action(-1, 0);
    else if (w == &s_dpad_right) tet_do_move_action(1, 0);
}

static void tet_drop_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK) return;
    if (s_tet.state == TET_STATE_IDLE || s_tet.state == TET_STATE_GAMEOVER) {
        tet_start_game();
        return;
    }
    tet_do_hard_drop();
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void tet_game_enter(ui_page_t *page)
{
    (void)page;
    memset(&s_tet, 0, sizeof(s_tet));
    s_tet.state = TET_STATE_IDLE;
    tet_update_texts();
    tet_load_best();
    ui_page_invalidate_all();
}

/* Per-frame game logic update */
static void tet_game_update(ui_page_t *page)
{
    (void)page;
    tet_game_tick();
}

/* Per-row game rendering (compositing renderer auto-clips to target row) */
static void tet_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Game area background */
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, UI_HEX(0x1A1A1A));

    if (s_tet.state == TET_STATE_IDLE) {
        tet_draw_idle_screen();
    } else {
        tet_draw_board();
        tet_draw_piece();
        tet_draw_panel();

        if (s_tet.state == TET_STATE_GAMEOVER) {
            tet_draw_gameover_overlay();
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_tetris_init(void)
{
    ui_app_page_init(&s_game_tetris, "Tetris", 0x200);

    /* Touch area for swipe/click */
    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = tet_touch_event;

    /* D-pad buttons */
    ui_rect_t up_r = {TET_DPAD_UP_X, TET_DPAD_UP_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_widget_init(&s_dpad_up, &up_r);
    s_dpad_up.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_up.event_cb = tet_dpad_event;

    ui_rect_t dn_r = {TET_DPAD_DOWN_X, TET_DPAD_DOWN_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_widget_init(&s_dpad_down, &dn_r);
    s_dpad_down.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_down.event_cb = tet_dpad_event;

    ui_rect_t lt_r = {TET_DPAD_LEFT_X, TET_DPAD_LEFT_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_widget_init(&s_dpad_left, &lt_r);
    s_dpad_left.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_left.event_cb = tet_dpad_event;

    ui_rect_t rt_r = {TET_DPAD_RIGHT_X, TET_DPAD_RIGHT_Y, TET_DPAD_BTN, TET_DPAD_BTN};
    ui_widget_init(&s_dpad_right, &rt_r);
    s_dpad_right.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_right.event_cb = tet_dpad_event;

    /* Drop button */
    ui_rect_t drop_r = {TET_DROP_X, TET_DROP_Y, TET_DROP_W, TET_DROP_H};
    ui_widget_init(&s_drop_btn, &drop_r);
    s_drop_btn.bg_color = UI_COLOR_TRANSPARENT;
    s_drop_btn.event_cb = tet_drop_event;

    /* Widget order: high index = high priority for events */
    s_tet_widgets[0] = &s_touch_area;
    s_tet_widgets[1] = &s_dpad_up;
    s_tet_widgets[2] = &s_dpad_down;
    s_tet_widgets[3] = &s_dpad_left;
    s_tet_widgets[4] = &s_dpad_right;
    s_tet_widgets[5] = &s_drop_btn;
    s_tet_widgets[6] = (ui_widget_t *)&s_game_tetris.lbl_title;
    s_tet_widgets[7] = (ui_widget_t *)&s_game_tetris.btn_back;

    ui_page_set_callbacks(&s_game_tetris.page, tet_game_enter, NULL,
                          tet_game_draw, NULL);
    ui_page_set_update_cb(&s_game_tetris.page, tet_game_update);
    ui_page_set_widgets(&s_game_tetris.page, s_tet_widgets, 8);
    ui_page_register(&s_game_tetris.page);
}

ui_page_t *game_tetris_get_page(void)
{
    return &s_game_tetris.page;
}
