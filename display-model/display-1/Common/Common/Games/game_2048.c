/********************************** (C) COPYRIGHT *******************************
* File Name          : game_2048.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/20
* Description        : 2048 puzzle game.
*                      Swipe to move tiles. Merge same numbers to reach 16384.
*                      One undo opportunity per game.
********************************************************************************/
#include "game_2048.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration
 *=============================================================================*/

#define G4_GRID_SIZE        4
#define G4_TILE_SIZE        90      /* Tile width/height in pixels */
#define G4_TILE_GAP         6       /* Gap between tiles */
#define G4_GRID_PAD         8       /* Padding inside grid background */
#define G4_GRID_TOTAL       (G4_GRID_SIZE * G4_TILE_SIZE + (G4_GRID_SIZE + 1) * G4_TILE_GAP)
#define G4_GRID_X           20
#define G4_GRID_Y           (APP_TITLE_BAR_H + (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H - G4_GRID_TOTAL) / 2)

/* Right panel */
#define G4_PANEL_X          (G4_GRID_X + G4_GRID_TOTAL + 30)
#define G4_PANEL_W          (UI_SCREEN_WIDTH - G4_PANEL_X - 20)
#define G4_PANEL_Y          (APP_TITLE_BAR_H + 20)

/* Undo button */
#define G4_UNDO_W           100
#define G4_UNDO_H           36
#define G4_UNDO_X           (G4_PANEL_X + (G4_PANEL_W - G4_UNDO_W) / 2)
#define G4_UNDO_Y           (G4_GRID_Y + G4_GRID_TOTAL - G4_UNDO_H)

/* D-pad buttons (between MOVES and UNDO) */
#define G4_DPAD_BTN         44      /* D-pad button size */
#define G4_DPAD_GAP         4       /* Gap between buttons */
#define G4_DPAD_CX          (G4_PANEL_X + G4_PANEL_W / 2)
#define G4_DPAD_CY          (G4_PANEL_Y + 18 + 50 + 18 + 50 + 18 + 50 + 30 + G4_DPAD_BTN / 2)
#define G4_DPAD_UP_X        (G4_DPAD_CX - G4_DPAD_BTN / 2)
#define G4_DPAD_UP_Y        (G4_DPAD_CY - G4_DPAD_BTN - G4_DPAD_GAP - G4_DPAD_BTN / 2)
#define G4_DPAD_DOWN_X      (G4_DPAD_CX - G4_DPAD_BTN / 2)
#define G4_DPAD_DOWN_Y      (G4_DPAD_CY + G4_DPAD_BTN / 2 + G4_DPAD_GAP)
#define G4_DPAD_LEFT_X      (G4_DPAD_CX - G4_DPAD_BTN / 2 - G4_DPAD_GAP - G4_DPAD_BTN)
#define G4_DPAD_LEFT_Y      (G4_DPAD_CY - G4_DPAD_BTN / 2)
#define G4_DPAD_RIGHT_X     (G4_DPAD_CX + G4_DPAD_BTN / 2 + G4_DPAD_GAP)
#define G4_DPAD_RIGHT_Y     (G4_DPAD_CY - G4_DPAD_BTN / 2)

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    G4_STATE_IDLE,
    G4_STATE_PLAYING,
    G4_STATE_GAMEOVER,
    G4_STATE_WIN,
} g4_state_t;

typedef struct {
    uint16_t grid[G4_GRID_SIZE][G4_GRID_SIZE];
    uint16_t prev_grid[G4_GRID_SIZE][G4_GRID_SIZE];  /* For undo */
    int score;
    int prev_score;
    int moves;
    int best;
    bool undo_available;
    g4_state_t state;
    char buf_score[16];
    char buf_moves[16];
    char buf_best[16];
} g4_game_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_2048;
static ui_widget_t s_touch_area;
static ui_widget_t s_dpad_up, s_dpad_down, s_dpad_left, s_dpad_right;
static ui_widget_t *s_g4_widgets[7];
static g4_game_t s_g4;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void g4_draw_dpad(void);
static void g4_inv_changed_tiles(void);
static void g4_inv_score(void);
static void g4_inv_moves(void);
static void g4_inv_undo(void);

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void g4_itoa(int val, char *out)
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

static int g4_rng_range(int min, int max)
{
    static uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));
}

/*=============================================================================
 *  Tile Colors
 *=============================================================================*/

static ui_color_t g4_tile_bg(uint16_t val)
{
    switch (val) {
    case 0:    return UI_HEX(0xCDCCBE);
    case 2:    return UI_HEX(0xEEE4DA);
    case 4:    return UI_HEX(0xEDE0C8);
    case 8:    return UI_HEX(0xF2B179);
    case 16:   return UI_HEX(0xF59563);
    case 32:   return UI_HEX(0xF67C5F);
    case 64:   return UI_HEX(0xF65E3B);
    case 128:  return UI_HEX(0xEDCF72);
    case 256:  return UI_HEX(0xEDCC61);
    case 512:  return UI_HEX(0xEDC850);
    case 1024: return UI_HEX(0xEDC53F);
    case 2048: return UI_HEX(0xEDC22E);
    case 4096: return UI_HEX(0x3C3A32);
    case 8192: return UI_HEX(0x3C3A32);
    case 16384:return UI_HEX(0x3C3A32);
    default:   return UI_HEX(0x3C3A32);
    }
}

static ui_color_t g4_tile_fg(uint16_t val)
{
    if (val <= 4) return UI_HEX(0x776E65);
    return UI_COLOR_WHITE;
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

static void g4_update_texts(void)
{
    g4_itoa(s_g4.score, s_g4.buf_score);
    g4_itoa(s_g4.moves, s_g4.buf_moves);
    g4_itoa(s_g4.best, s_g4.buf_best);
}

/*=============================================================================
 *  Best Score Persistence (via CLI passthrough to Core appcfg)
 *=============================================================================*/

static void g4_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    if (!tag || strcmp(tag, "appcfg") != 0) return;
    /* Response is a plain number string */
    s_g4.best = atoi(buf);
    g4_update_texts();
    ui_page_invalidate_all();
}

static const uart_cli_cb_t s_g4_cli_cb = {
    .on_cli_complete = g4_on_cli_complete,
};

static void g4_load_best(void)
{
    UART_SetCLICallbacks(&s_g4_cli_cb);
    UART_SendCLI("appcfg get game_2048 highscore");
}

static void g4_save_best(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "appcfg set game_2048 highscore %d", s_g4.best);
    UART_SendCLI(cmd);
}

static int g4_count_empty(void)
{
    int count = 0;
    for (int r = 0; r < G4_GRID_SIZE; r++)
        for (int c = 0; c < G4_GRID_SIZE; c++)
            if (s_g4.grid[r][c] == 0) count++;
    return count;
}

static void g4_spawn_tile(void)
{
    int empty = g4_count_empty();
    if (empty == 0) return;
    int target = g4_rng_range(0, empty - 1);
    int idx = 0;
    for (int r = 0; r < G4_GRID_SIZE; r++) {
        for (int c = 0; c < G4_GRID_SIZE; c++) {
            if (s_g4.grid[r][c] == 0) {
                if (idx == target) {
                    /* Equal probability of 2 or 4 */
                    s_g4.grid[r][c] = (g4_rng_range(0, 1) == 0) ? 2 : 4;
                    return;
                }
                idx++;
            }
        }
    }
}

static bool g4_can_move(void)
{
    for (int r = 0; r < G4_GRID_SIZE; r++) {
        for (int c = 0; c < G4_GRID_SIZE; c++) {
            if (s_g4.grid[r][c] == 0) return true;
            if (c < G4_GRID_SIZE - 1 && s_g4.grid[r][c] == s_g4.grid[r][c + 1]) return true;
            if (r < G4_GRID_SIZE - 1 && s_g4.grid[r][c] == s_g4.grid[r + 1][c]) return true;
        }
    }
    return false;
}

static bool g4_has_16384(void)
{
    for (int r = 0; r < G4_GRID_SIZE; r++)
        for (int c = 0; c < G4_GRID_SIZE; c++)
            if (s_g4.grid[r][c] >= 16384) return true;
    return false;
}

/* Slide a single row to the left, return score gained */
static int g4_slide_row(uint16_t row[G4_GRID_SIZE])
{
    int score = 0;
    /* Remove zeros */
    uint16_t tmp[G4_GRID_SIZE] = {0};
    int pos = 0;
    for (int c = 0; c < G4_GRID_SIZE; c++) {
        if (row[c] != 0) tmp[pos++] = row[c];
    }
    /* Merge adjacent equals */
    for (int c = 0; c < G4_GRID_SIZE - 1; c++) {
        if (tmp[c] != 0 && tmp[c] == tmp[c + 1]) {
            tmp[c] *= 2;
            score += tmp[c];
            tmp[c + 1] = 0;
        }
    }
    /* Remove zeros again */
    uint16_t result[G4_GRID_SIZE] = {0};
    pos = 0;
    for (int c = 0; c < G4_GRID_SIZE; c++) {
        if (tmp[c] != 0) result[pos++] = tmp[c];
    }
    /* Copy back */
    bool changed = false;
    for (int c = 0; c < G4_GRID_SIZE; c++) {
        if (row[c] != result[c]) changed = true;
        row[c] = result[c];
    }
    return changed ? score : -1;  /* -1 means no change */
}

typedef enum {
    G4_DIR_LEFT,
    G4_DIR_RIGHT,
    G4_DIR_UP,
    G4_DIR_DOWN,
} g4_dir_t;

static bool g4_do_move(g4_dir_t dir)
{
    bool any_changed = false;
    int total_score = 0;

    /* Save state for undo before move */
    memcpy(s_g4.prev_grid, s_g4.grid, sizeof(s_g4.grid));
    s_g4.prev_score = s_g4.score;

    if (dir == G4_DIR_LEFT) {
        for (int r = 0; r < G4_GRID_SIZE; r++) {
            int s = g4_slide_row(s_g4.grid[r]);
            if (s >= 0) { any_changed = true; total_score += s; }
        }
    } else if (dir == G4_DIR_RIGHT) {
        for (int r = 0; r < G4_GRID_SIZE; r++) {
            /* Reverse, slide left, reverse */
            uint16_t rev[G4_GRID_SIZE];
            for (int c = 0; c < G4_GRID_SIZE; c++)
                rev[c] = s_g4.grid[r][G4_GRID_SIZE - 1 - c];
            int s = g4_slide_row(rev);
            if (s >= 0) { any_changed = true; total_score += s; }
            for (int c = 0; c < G4_GRID_SIZE; c++)
                s_g4.grid[r][G4_GRID_SIZE - 1 - c] = rev[c];
        }
    } else if (dir == G4_DIR_UP) {
        for (int c = 0; c < G4_GRID_SIZE; c++) {
            uint16_t col[G4_GRID_SIZE];
            for (int r = 0; r < G4_GRID_SIZE; r++) col[r] = s_g4.grid[r][c];
            int s = g4_slide_row(col);
            if (s >= 0) { any_changed = true; total_score += s; }
            for (int r = 0; r < G4_GRID_SIZE; r++) s_g4.grid[r][c] = col[r];
        }
    } else { /* G4_DIR_DOWN */
        for (int c = 0; c < G4_GRID_SIZE; c++) {
            uint16_t col[G4_GRID_SIZE];
            for (int r = 0; r < G4_GRID_SIZE; r++)
                col[r] = s_g4.grid[G4_GRID_SIZE - 1 - r][c];
            int s = g4_slide_row(col);
            if (s >= 0) { any_changed = true; total_score += s; }
            for (int r = 0; r < G4_GRID_SIZE; r++)
                s_g4.grid[G4_GRID_SIZE - 1 - r][c] = col[r];
        }
    }

    if (!any_changed) {
        /* Restore prev_grid since no move happened */
        return false;
    }

    s_g4.score += total_score;
    s_g4.moves++;
    s_g4.undo_available = true;  /* Each successful move enables undo */

    g4_spawn_tile();
    g4_update_texts();

    /* Check win/lose */
    if (g4_has_16384()) {
        s_g4.state = G4_STATE_WIN;
        if (s_g4.score > s_g4.best) {
            s_g4.best = s_g4.score;
            g4_save_best();
        }
        ui_page_invalidate_all();
    } else if (!g4_can_move()) {
        s_g4.state = G4_STATE_GAMEOVER;
        if (s_g4.score > s_g4.best) {
            s_g4.best = s_g4.score;
            g4_save_best();
        }
        ui_page_invalidate_all();
    }

    return true;
}

static void g4_undo(void)
{
    if (!s_g4.undo_available) return;
    memcpy(s_g4.grid, s_g4.prev_grid, sizeof(s_g4.grid));
    s_g4.score = s_g4.prev_score;
    s_g4.moves--;
    s_g4.undo_available = false;
    s_g4.state = G4_STATE_PLAYING;
    g4_update_texts();
}

static void g4_reset(void)
{
    int saved_best = s_g4.best;
    memset(&s_g4, 0, sizeof(s_g4));
    s_g4.best = saved_best;
    s_g4.state = G4_STATE_IDLE;
    g4_update_texts();
}

static void g4_start_game(void)
{
    int saved_best = s_g4.best;
    memset(&s_g4, 0, sizeof(s_g4));
    s_g4.best = saved_best;
    s_g4.state = G4_STATE_PLAYING;
    g4_spawn_tile();
    g4_spawn_tile();
    g4_update_texts();
    ui_page_invalidate_all();
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void g4_draw_tile(int16_t x, int16_t y, uint16_t val)
{
    ui_rect_t r = {x, y, G4_TILE_SIZE, G4_TILE_SIZE};
    ui_draw_fill_round_rect(&r, 6, g4_tile_bg(val));

    if (val == 0) return;

    char buf[8];
    g4_itoa(val, buf);
    const ui_font_t *font = &font_montserrat_16;
    if (val >= 16384) font = &font_montserrat_12;
    else if (val >= 1024) font = &font_montserrat_12;
    ui_draw_text_in_rect(&r, buf, font, g4_tile_fg(val), 1);
}

static void g4_draw_grid(void)
{
    /* Grid background */
    ui_rect_t bg = {G4_GRID_X, G4_GRID_Y, G4_GRID_TOTAL, G4_GRID_TOTAL};
    ui_draw_fill_round_rect(&bg, 10, UI_HEX(0xBBADA0));

    /* Tiles */
    for (int r = 0; r < G4_GRID_SIZE; r++) {
        for (int c = 0; c < G4_GRID_SIZE; c++) {
            int16_t tx = G4_GRID_X + G4_TILE_GAP + c * (G4_TILE_SIZE + G4_TILE_GAP);
            int16_t ty = G4_GRID_Y + G4_TILE_GAP + r * (G4_TILE_SIZE + G4_TILE_GAP);
            g4_draw_tile(tx, ty, s_g4.grid[r][c]);
        }
    }
}

static void g4_draw_panel(void)
{
    int16_t y = G4_PANEL_Y;

    /* Score */
    ui_draw_text(G4_PANEL_X, y, "SCORE", &font_montserrat_12, UI_HEX(0x776E65));
    y += 18;
    ui_rect_t score_bg = {G4_PANEL_X, y, G4_PANEL_W, 36};
    ui_draw_fill_round_rect(&score_bg, 6, UI_HEX(0xBBADA0));
    ui_draw_text_in_rect(&score_bg, s_g4.buf_score, &font_montserrat_16, UI_COLOR_WHITE, 1);
    y += 50;

    /* Moves */
    ui_draw_text(G4_PANEL_X, y, "MOVES", &font_montserrat_12, UI_HEX(0x776E65));
    y += 18;
    ui_rect_t moves_bg = {G4_PANEL_X, y, G4_PANEL_W, 36};
    ui_draw_fill_round_rect(&moves_bg, 6, UI_HEX(0xBBADA0));
    ui_draw_text_in_rect(&moves_bg, s_g4.buf_moves, &font_montserrat_16, UI_COLOR_WHITE, 1);
    y += 50;

    /* Best */
    ui_draw_text(G4_PANEL_X, y, "BEST", &font_montserrat_12, UI_HEX(0x776E65));
    y += 18;
    ui_rect_t best_bg = {G4_PANEL_X, y, G4_PANEL_W, 36};
    ui_draw_fill_round_rect(&best_bg, 6, UI_HEX(0xBBADA0));
    ui_draw_text_in_rect(&best_bg, s_g4.buf_best, &font_montserrat_16, UI_COLOR_WHITE, 1);
    y += 50;

    /* Undo button */
    ui_color_t undo_bg = s_g4.undo_available ? UI_HEX(0x8F7A66) : UI_HEX(0xCDCCBE);
    ui_color_t undo_fg = s_g4.undo_available ? UI_COLOR_WHITE : UI_HEX(0x999999);
    ui_rect_t undo_r = {G4_UNDO_X, G4_UNDO_Y, G4_UNDO_W, G4_UNDO_H};
    ui_draw_fill_round_rect(&undo_r, 6, undo_bg);
    ui_draw_text_in_rect(&undo_r, "UNDO", &font_montserrat_12, undo_fg, 1);

    /* D-pad buttons */
    g4_draw_dpad();
}

static void g4_draw_dpad(void)
{
    ui_color_t bg = UI_HEX(0xBBADA0);
    ui_color_t fg = UI_COLOR_WHITE;

    /* Up */
    ui_rect_t up_r = {G4_DPAD_UP_X, G4_DPAD_UP_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_draw_fill_round_rect(&up_r, 6, bg);
    ui_draw_text_in_rect(&up_r, "^", &font_montserrat_16, fg, 1);

    /* Down */
    ui_rect_t dn_r = {G4_DPAD_DOWN_X, G4_DPAD_DOWN_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_draw_fill_round_rect(&dn_r, 6, bg);
    ui_draw_text_in_rect(&dn_r, "v", &font_montserrat_16, fg, 1);

    /* Left */
    ui_rect_t lt_r = {G4_DPAD_LEFT_X, G4_DPAD_LEFT_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_draw_fill_round_rect(&lt_r, 6, bg);
    ui_draw_text_in_rect(&lt_r, "<", &font_montserrat_16, fg, 1);

    /* Right */
    ui_rect_t rt_r = {G4_DPAD_RIGHT_X, G4_DPAD_RIGHT_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_draw_fill_round_rect(&rt_r, 6, bg);
    ui_draw_text_in_rect(&rt_r, ">", &font_montserrat_16, fg, 1);
}

static void g4_dpad_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type != UI_EVENT_CLICK) return;

    g4_dir_t dir;
    if (w == &s_dpad_up)         dir = G4_DIR_UP;
    else if (w == &s_dpad_down)  dir = G4_DIR_DOWN;
    else if (w == &s_dpad_left)  dir = G4_DIR_LEFT;
    else if (w == &s_dpad_right) dir = G4_DIR_RIGHT;
    else return;

    if (s_g4.state == G4_STATE_IDLE) {
        g4_start_game();
        return;
    }
    if (s_g4.state == G4_STATE_GAMEOVER || s_g4.state == G4_STATE_WIN) {
        g4_start_game();
        return;
    }
    if (s_g4.state == G4_STATE_PLAYING) {
        if (g4_do_move(dir)) {
            g4_inv_changed_tiles();
            g4_inv_score();
            g4_inv_moves();
            g4_inv_undo();
        }
    }
}

static void g4_draw_idle_screen(void)
{
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, UI_HEX(0xFAF8EF));

    g4_draw_grid();
    g4_draw_panel();

    /* Overlay hint */
    ui_rect_t hint = {G4_GRID_X, G4_GRID_Y, G4_GRID_TOTAL, G4_GRID_TOTAL};
    ui_draw_fill_round_rect(&hint, 10, UI_HEX(0xEEE4DA));

    ui_rect_t title = {G4_GRID_X, G4_GRID_Y + G4_GRID_TOTAL / 2 - 40,
                       G4_GRID_TOTAL, 40};
    ui_draw_text_in_rect(&title, "2048", &font_montserrat_16, UI_HEX(0x776E65), 1);

    ui_rect_t sub = {G4_GRID_X, G4_GRID_Y + G4_GRID_TOTAL / 2,
                     G4_GRID_TOTAL, 30};
    ui_draw_text_in_rect(&sub, "Swipe to start", &font_montserrat_12,
                         UI_HEX(0x776E65), 1);
}

static void g4_draw_overlay(const char *title, ui_color_t title_color)
{
    ui_rect_t overlay = {G4_GRID_X, G4_GRID_Y, G4_GRID_TOTAL, G4_GRID_TOTAL};
    ui_draw_fill_round_rect(&overlay, 10, UI_HEX(0xEEE4DA));

    ui_rect_t tr = {G4_GRID_X, G4_GRID_Y + G4_GRID_TOTAL / 2 - 30,
                    G4_GRID_TOTAL, 30};
    ui_draw_text_in_rect(&tr, title, &font_montserrat_16, title_color, 1);

    char buf[32];
    g4_itoa(s_g4.score, buf);
    ui_rect_t sr = {G4_GRID_X, G4_GRID_Y + G4_GRID_TOTAL / 2 + 5,
                    G4_GRID_TOTAL, 24};
    ui_draw_text_in_rect(&sr, buf, &font_montserrat_12, UI_HEX(0x776E65), 1);

    ui_rect_t hr = {G4_GRID_X, G4_GRID_Y + G4_GRID_TOTAL / 2 + 35,
                    G4_GRID_TOTAL, 24};
    ui_draw_text_in_rect(&hr, "Swipe to restart", &font_montserrat_12,
                         UI_HEX(0x999999), 1);
}

/* Get tile rect for grid position (r, c) */
static ui_rect_t g4_tile_rect(int r, int c)
{
    int16_t tx = G4_GRID_X + G4_TILE_GAP + c * (G4_TILE_SIZE + G4_TILE_GAP);
    int16_t ty = G4_GRID_Y + G4_TILE_GAP + r * (G4_TILE_SIZE + G4_TILE_GAP);
    ui_rect_t rect = {tx, ty, G4_TILE_SIZE, G4_TILE_SIZE};
    return rect;
}

/* Invalidate tiles that differ between two grids */
static void g4_inv_diff_tiles(const uint16_t before[G4_GRID_SIZE][G4_GRID_SIZE])
{
    for (int r = 0; r < G4_GRID_SIZE; r++) {
        for (int c = 0; c < G4_GRID_SIZE; c++) {
            if (s_g4.grid[r][c] != before[r][c]) {
                ui_rect_t tr = g4_tile_rect(r, c);
                ui_page_invalidate(&tr);
            }
        }
    }
}

/* Invalidate only changed tiles (compare prev_grid vs grid) */
static void g4_inv_changed_tiles(void)
{
    g4_inv_diff_tiles(s_g4.prev_grid);
}

/* Invalidate score box */
static void g4_inv_score(void)
{
    int16_t y = G4_PANEL_Y + 18;
    ui_rect_t r = {G4_PANEL_X, y, G4_PANEL_W, 36};
    ui_page_invalidate(&r);
}

/* Invalidate moves box */
static void g4_inv_moves(void)
{
    int16_t y = G4_PANEL_Y + 18 + 50 + 18;
    ui_rect_t r = {G4_PANEL_X, y, G4_PANEL_W, 36};
    ui_page_invalidate(&r);
}

/* Invalidate undo button */
static void g4_inv_undo(void)
{
    ui_rect_t r = {G4_UNDO_X, G4_UNDO_Y, G4_UNDO_W, G4_UNDO_H};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void g4_game_enter(ui_page_t *page)
{
    (void)page;
    g4_reset();
    g4_load_best();
    ui_page_invalidate_all();
}

static void g4_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    /* Keyboard events */
    if (e->type == UI_EVENT_KEY_LEFT_ARROW || e->type == UI_EVENT_KEY_RIGHT_ARROW ||
        e->type == UI_EVENT_KEY_UP_ARROW || e->type == UI_EVENT_KEY_DOWN_ARROW) {
        if (s_g4.state == G4_STATE_IDLE || s_g4.state == G4_STATE_GAMEOVER || s_g4.state == G4_STATE_WIN) {
            g4_start_game();
            return;
        }
        if (s_g4.state == G4_STATE_PLAYING) {
            g4_dir_t dir;
            if (e->type == UI_EVENT_KEY_LEFT_ARROW)       dir = G4_DIR_LEFT;
            else if (e->type == UI_EVENT_KEY_RIGHT_ARROW)  dir = G4_DIR_RIGHT;
            else if (e->type == UI_EVENT_KEY_UP_ARROW)     dir = G4_DIR_UP;
            else                                            dir = G4_DIR_DOWN;
            if (g4_do_move(dir)) {
                g4_inv_changed_tiles();
                g4_inv_score();
                g4_inv_moves();
                g4_inv_undo();
            }
        }
        return;
    } else if (e->type == UI_EVENT_KEY_OK) {
        if (s_g4.state == G4_STATE_IDLE || s_g4.state == G4_STATE_GAMEOVER || s_g4.state == G4_STATE_WIN)
            g4_start_game();
        return;
    }

    /* Touch / swipe events */
    if (e->type == UI_EVENT_SWIPE_LEFT || e->type == UI_EVENT_SWIPE_RIGHT ||
        e->type == UI_EVENT_SWIPE_UP || e->type == UI_EVENT_SWIPE_DOWN) {

        if (s_g4.state == G4_STATE_IDLE) {
            g4_start_game();
            return;
        }
        if (s_g4.state == G4_STATE_GAMEOVER || s_g4.state == G4_STATE_WIN) {
            g4_start_game();
            return;
        }
        if (s_g4.state == G4_STATE_PLAYING) {
            g4_dir_t dir;
            if (e->type == UI_EVENT_SWIPE_LEFT)  dir = G4_DIR_LEFT;
            else if (e->type == UI_EVENT_SWIPE_RIGHT) dir = G4_DIR_RIGHT;
            else if (e->type == UI_EVENT_SWIPE_UP)    dir = G4_DIR_UP;
            else                                       dir = G4_DIR_DOWN;

            if (g4_do_move(dir)) {
                g4_inv_changed_tiles();
                g4_inv_score();
                g4_inv_moves();
                g4_inv_undo();
            }
        }
    } else if (e->type == UI_EVENT_CLICK) {
        /* Check undo button tap */
        if (s_g4.state == G4_STATE_PLAYING && s_g4.undo_available) {
            int16_t tx = e->pos.x;
            int16_t ty = e->pos.y;
            if (tx >= G4_UNDO_X && tx < G4_UNDO_X + G4_UNDO_W &&
                ty >= G4_UNDO_Y && ty < G4_UNDO_Y + G4_UNDO_H) {
                uint16_t saved_grid[G4_GRID_SIZE][G4_GRID_SIZE];
                memcpy(saved_grid, s_g4.grid, sizeof(s_g4.grid));
                g4_undo();
                g4_inv_diff_tiles(saved_grid);
                g4_inv_score();
                g4_inv_moves();
                g4_inv_undo();
            }
        }
    }
}

/* Per-row game rendering (compositing renderer auto-clips to target row) */
static void g4_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Game area background */
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, UI_HEX(0xFAF8EF));

    if (s_g4.state == G4_STATE_IDLE) {
        g4_draw_idle_screen();
    } else {
        g4_draw_grid();
        g4_draw_panel();

        if (s_g4.state == G4_STATE_GAMEOVER) {
            g4_draw_overlay("Game Over!", UI_HEX(0xF65E3B));
        } else if (s_g4.state == G4_STATE_WIN) {
            g4_draw_overlay("You Win!", UI_HEX(0xEDC22E));
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_2048_init(void)
{
    ui_app_page_init(&s_game_2048, "2048", 0x201);

    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = g4_touch_event;

    /* D-pad buttons */
    ui_rect_t up_r = {G4_DPAD_UP_X, G4_DPAD_UP_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_widget_init(&s_dpad_up, &up_r);
    s_dpad_up.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_up.event_cb = g4_dpad_event;

    ui_rect_t dn_r = {G4_DPAD_DOWN_X, G4_DPAD_DOWN_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_widget_init(&s_dpad_down, &dn_r);
    s_dpad_down.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_down.event_cb = g4_dpad_event;

    ui_rect_t lt_r = {G4_DPAD_LEFT_X, G4_DPAD_LEFT_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_widget_init(&s_dpad_left, &lt_r);
    s_dpad_left.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_left.event_cb = g4_dpad_event;

    ui_rect_t rt_r = {G4_DPAD_RIGHT_X, G4_DPAD_RIGHT_Y, G4_DPAD_BTN, G4_DPAD_BTN};
    ui_widget_init(&s_dpad_right, &rt_r);
    s_dpad_right.bg_color = UI_COLOR_TRANSPARENT;
    s_dpad_right.event_cb = g4_dpad_event;

    s_g4_widgets[0] = &s_touch_area;
    s_g4_widgets[1] = &s_dpad_up;
    s_g4_widgets[2] = &s_dpad_down;
    s_g4_widgets[3] = &s_dpad_left;
    s_g4_widgets[4] = &s_dpad_right;
    s_g4_widgets[5] = (ui_widget_t *)&s_game_2048.lbl_title;
    s_g4_widgets[6] = (ui_widget_t *)&s_game_2048.btn_back;

    ui_page_set_widgets(&s_game_2048.page, s_g4_widgets, 7);
    ui_page_set_callbacks(&s_game_2048.page, g4_game_enter, NULL,
                          g4_game_draw, NULL);
    ui_page_register(&s_game_2048.page);
}

ui_page_t *game_2048_get_page(void)
{
    return &s_game_2048.page;
}
