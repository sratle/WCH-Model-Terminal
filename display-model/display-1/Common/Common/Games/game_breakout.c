/********************************** (C) COPYRIGHT *******************************
* File Name          : game_breakout.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/20
* Description        : Classic Breakout game.
*                      10x6 brick grid, paddle, ball, score/lives/level.
*                      Optimized dirty-rect partial refresh.
********************************************************************************/
#include "game_breakout.h"
#include "../UI/ui_app_common.h"
#include <string.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Configuration
 *
 *  Screen: 800x480, Title bar: 40px
 *  Game area: 800x440 (y: 40~480)
 *
 *  Layout:
 *    [Bricks area]         [Panel: SCORE / LIVES / LEVEL]
 *    [Play area]           [Panel continued]
 *    [Paddle]              [L/R buttons]
 *
 *  Bricks: 10 cols x 6 rows, cell=60x16, total=600x96
 *  Play area: 600x440, centered horizontally
 *  Panel: right side, 170px wide
 *=============================================================================*/

#define BRK_COLS            10
#define BRK_ROWS            6
#define BRK_CELL_W          58      /* Brick width */
#define BRK_CELL_H          18      /* Brick height */
#define BRK_GAP             2       /* Gap between bricks */
#define BRK_GRID_W          (BRK_COLS * (BRK_CELL_W + BRK_GAP) - BRK_GAP)  /* 598 */
#define BRK_GRID_H          (BRK_ROWS * (BRK_CELL_H + BRK_GAP) - BRK_GAP)  /* 118 */
#define BRK_GRID_X          10
#define BRK_GRID_Y          (APP_TITLE_BAR_H + 10)  /* 50 */

/* Play area (bricks + ball field) */
#define BRK_PLAY_X          BRK_GRID_X
#define BRK_PLAY_W          BRK_GRID_W              /* 598 */
#define BRK_PLAY_BOTTOM     (UI_SCREEN_HEIGHT - 10) /* 470 */

/* Paddle */
#define BRK_PADDLE_W        80
#define BRK_PADDLE_H        12
#define BRK_PADDLE_Y        (BRK_PLAY_BOTTOM - 30)  /* 440 */
#define BRK_PADDLE_SPEED    8
#define BRK_PADDLE_HOLD_SPEED 14  /* Speed when holding button */

/* Ball */
#define BRK_BALL_R          5       /* Ball radius */
#define BRK_BALL_SPEED_INIT 8

/* Right panel */
#define BRK_PANEL_X         (BRK_PLAY_X + BRK_PLAY_W + 12)  /* 620 */
#define BRK_PANEL_W         (UI_SCREEN_WIDTH - BRK_PANEL_X - 10)  /* 170 */
#define BRK_PANEL_Y         (APP_TITLE_BAR_H + 10)  /* 50 */

/* Info box dimensions */
#define BRK_INFO_LABEL_H    14
#define BRK_INFO_BOX_H      28
#define BRK_INFO_BOX_R      6
#define BRK_INFO_GAP        6

/* Score section */
#define BRK_SCORE_LABEL_Y   BRK_PANEL_Y
#define BRK_SCORE_BOX_Y     (BRK_SCORE_LABEL_Y + BRK_INFO_LABEL_H)

/* Lives section */
#define BRK_LIVES_LABEL_Y   (BRK_SCORE_BOX_Y + BRK_INFO_BOX_H + BRK_INFO_GAP)
#define BRK_LIVES_BOX_Y     (BRK_LIVES_LABEL_Y + BRK_INFO_LABEL_H)

/* Level section */
#define BRK_LEVEL_LABEL_Y   (BRK_LIVES_BOX_Y + BRK_INFO_BOX_H + BRK_INFO_GAP)
#define BRK_LEVEL_BOX_Y     (BRK_LEVEL_LABEL_Y + BRK_INFO_LABEL_H)

/* Control buttons (L/R at bottom of panel) */
#define BRK_CTRL_BTN_W      70
#define BRK_CTRL_BTN_H      54
#define BRK_CTRL_GAP        8
#define BRK_CTRL_TOP        (BRK_LEVEL_BOX_Y + BRK_INFO_BOX_H + 20)
#define BRK_CTRL_LEFT_X     BRK_PANEL_X
#define BRK_CTRL_LEFT_Y     BRK_CTRL_TOP
#define BRK_CTRL_RIGHT_X    (BRK_PANEL_X + BRK_CTRL_BTN_W + BRK_CTRL_GAP)
#define BRK_CTRL_RIGHT_Y    BRK_CTRL_TOP

/* Launch button */
#define BRK_LAUNCH_W        (2 * BRK_CTRL_BTN_W + BRK_CTRL_GAP)
#define BRK_LAUNCH_H        40
#define BRK_LAUNCH_X        BRK_PANEL_X
#define BRK_LAUNCH_Y        (BRK_CTRL_TOP + BRK_CTRL_BTN_H + 10)

/*=============================================================================
 *  Brick Colors (per row, top to bottom)
 *=============================================================================*/

static const ui_color_t s_brick_colors[BRK_ROWS] = {
    UI_HEX(0xF44336),  /* Red */
    UI_HEX(0xFF9800),  /* Orange */
    UI_HEX(0xFFEB3B),  /* Yellow */
    UI_HEX(0x4CAF50),  /* Green */
    UI_HEX(0x2196F3),  /* Blue */
    UI_HEX(0x9C27B0),  /* Purple */
};

static const int s_brick_scores[BRK_ROWS] = {
    60, 50, 40, 30, 20, 10
};

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    BRK_STATE_IDLE,
    BRK_STATE_LAUNCH,     /* Ball on paddle, waiting to launch */
    BRK_STATE_PLAYING,
    BRK_STATE_GAMEOVER,
    BRK_STATE_WIN,
} brk_state_t;

/* Fixed-point 16.16 for ball position/velocity */
typedef int32_t brk_fixed_t;
#define BRK_FIX_SHIFT      16
#define BRK_INT_TO_FIX(i)  ((brk_fixed_t)(i) << BRK_FIX_SHIFT)
#define BRK_FIX_TO_INT(f)  ((int16_t)((f) >> BRK_FIX_SHIFT))
#define BRK_FIX_FRAC(f)    ((f) & 0xFFFF)
#define BRK_FIX_ONE        BRK_INT_TO_FIX(1)

typedef struct {
    /* Bricks: 0=destroyed, 1=present */
    uint8_t bricks[BRK_ROWS][BRK_COLS];

    /* Paddle position (center x) */
    int16_t paddle_x;

    /* Ball position (fixed-point, center) */
    brk_fixed_t ball_x, ball_y;
    /* Ball velocity (fixed-point) */
    brk_fixed_t ball_vx, ball_vy;

    /* Last-drawn ball/paddle positions for dirty rect (pixel coords) */
    int16_t drawn_ball_x, drawn_ball_y;
    int16_t drawn_paddle_x;

    int score;
    int lives;
    int level;
    brk_state_t state;
    bool need_full_redraw;
    uint32_t last_tick_ms;

    char buf_score[16];
    char buf_lives[16];
    char buf_level[16];
} brk_game_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_breakout;
static ui_widget_t s_touch_area;
static ui_widget_t s_btn_left, s_btn_right;
static ui_widget_t s_btn_launch;
static ui_widget_t *s_brk_widgets[6];
static brk_game_t s_brk;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void brk_draw_ctrl_btns(void);
static void brk_draw_launch_btn(void);

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void brk_itoa(int val, char *out)
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

static int brk_rng_range(int min, int max)
{
    static uint32_t seed = 98765;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));
}

static void brk_update_texts(void)
{
    brk_itoa(s_brk.score, s_brk.buf_score);
    brk_itoa(s_brk.lives, s_brk.buf_lives);
    brk_itoa(s_brk.level, s_brk.buf_level);
}

/*=============================================================================
 *  Brick Pixel Position
 *=============================================================================*/

/* Get pixel rect for a brick cell */
static ui_rect_t brk_brick_rect(int row, int col)
{
    ui_rect_t r = {
        BRK_GRID_X + col * (BRK_CELL_W + BRK_GAP),
        BRK_GRID_Y + row * (BRK_CELL_H + BRK_GAP),
        BRK_CELL_W, BRK_CELL_H
    };
    return r;
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

static void brk_reset_ball(void)
{
    /* Place ball on top of paddle */
    s_brk.ball_x = BRK_INT_TO_FIX(s_brk.paddle_x);
    s_brk.ball_y = BRK_INT_TO_FIX(BRK_PADDLE_Y - BRK_BALL_R - 1);
    s_brk.ball_vx = 0;
    s_brk.ball_vy = 0;
    s_brk.state = BRK_STATE_LAUNCH;
}

static void brk_launch_ball(void)
{
    if (s_brk.state != BRK_STATE_LAUNCH) return;

    int speed = BRK_BALL_SPEED_INIT + s_brk.level * 2;
    /* Random angle between -45 and +45 degrees from vertical */
    int angle = brk_rng_range(-30, 30);
    /* Approximate: vx = speed * sin(angle), vy = -speed * cos(angle) */
    /* Use simple lookup for sin/cos * 256 */
    static const int8_t sin_tbl[61] = {
         0,  3,  5,  8, 10, 13, 15, 18, 20, 22, 25,
        27, 29, 31, 34, 36, 38, 40, 42, 44, 46,
        48, 50, 52, 53, 55, 57, 58, 60, 62, 63,
        65, 66, 67, 69, 70, 71, 72, 73, 74, 75,
        76, 77, 78, 79, 80, 81, 81, 82, 83, 83,
        84, 84, 85, 85, 86, 86, 86, 87, 87, 87
    };
    int idx = angle < 0 ? -angle : angle;
    if (idx > 30) idx = 30;
    int sv = sin_tbl[idx];
    int cv = sin_tbl[30 - idx];  /* cos ≈ sin(90-θ) */

    s_brk.ball_vx = BRK_INT_TO_FIX(speed) * (angle < 0 ? -sv : sv) / 256;
    s_brk.ball_vy = -BRK_INT_TO_FIX(speed) * cv / 256;

    /* Ensure minimum vertical speed */
    if (BRK_FIX_TO_INT(-s_brk.ball_vy) < 2) {
        s_brk.ball_vy = -BRK_INT_TO_FIX(2);
    }

    s_brk.state = BRK_STATE_PLAYING;
}

static void brk_init_level(void)
{
    /* Fill bricks */
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            s_brk.bricks[r][c] = 1;
        }
    }

    s_brk.paddle_x = BRK_PLAY_X + BRK_PLAY_W / 2;
    brk_reset_ball();
    s_brk.need_full_redraw = true;
}

static void brk_start_game(void)
{
    memset(&s_brk, 0, sizeof(s_brk));
    s_brk.lives = 3;
    s_brk.level = 0;
    s_brk.score = 0;
    brk_init_level();
    brk_update_texts();
    s_brk.need_full_redraw = true;
    s_brk.last_tick_ms = ui_get_real_ms();
}

static int brk_bricks_remaining(void)
{
    int count = 0;
    for (int r = 0; r < BRK_ROWS; r++)
        for (int c = 0; c < BRK_COLS; c++)
            if (s_brk.bricks[r][c]) count++;
    return count;
}

/* Check if ball overlaps a brick, destroy it, return true if hit */
static bool brk_check_brick_collision(int16_t bx, int16_t by, int16_t br,
                                      int16_t *hit_row, int16_t *hit_col)
{
    /* Check which brick cell the ball center is in or near */
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            if (!s_brk.bricks[r][c]) continue;
            ui_rect_t brect = brk_brick_rect(r, c);
            /* AABB circle collision */
            int16_t closest_x = bx < brect.x ? brect.x :
                                (bx > brect.x + brect.w ? brect.x + brect.w : bx);
            int16_t closest_y = by < brect.y ? brect.y :
                                (by > brect.y + brect.h ? brect.y + brect.h : by);
            int dx = bx - closest_x;
            int dy = by - closest_y;
            if (dx * dx + dy * dy <= br * br) {
                s_brk.bricks[r][c] = 0;
                *hit_row = r;
                *hit_col = c;
                return true;
            }
        }
    }
    return false;
}

static void brk_game_tick(void)
{
    if (s_brk.state != BRK_STATE_PLAYING) return;

    uint32_t now = ui_get_real_ms();
    /* Run at ~60fps: tick every 16ms */
    if (now - s_brk.last_tick_ms < 16) return;
    s_brk.last_tick_ms = now;

    /* Move ball */
    s_brk.ball_x += s_brk.ball_vx;
    s_brk.ball_y += s_brk.ball_vy;

    int16_t bx = BRK_FIX_TO_INT(s_brk.ball_x);
    int16_t by = BRK_FIX_TO_INT(s_brk.ball_y);
    int16_t br = BRK_BALL_R;

    /* Wall collisions */
    /* Left wall */
    if (bx - br <= BRK_PLAY_X) {
        s_brk.ball_x = BRK_INT_TO_FIX(BRK_PLAY_X + br);
        s_brk.ball_vx = -s_brk.ball_vx;
        bx = BRK_FIX_TO_INT(s_brk.ball_x);
    }
    /* Right wall */
    if (bx + br >= BRK_PLAY_X + BRK_PLAY_W) {
        s_brk.ball_x = BRK_INT_TO_FIX(BRK_PLAY_X + BRK_PLAY_W - br);
        s_brk.ball_vx = -s_brk.ball_vx;
        bx = BRK_FIX_TO_INT(s_brk.ball_x);
    }
    /* Top wall */
    if (by - br <= APP_TITLE_BAR_H) {
        s_brk.ball_y = BRK_INT_TO_FIX(APP_TITLE_BAR_H + br);
        s_brk.ball_vy = -s_brk.ball_vy;
        by = BRK_FIX_TO_INT(s_brk.ball_y);
    }

    /* Paddle collision */
    int16_t pad_left = s_brk.paddle_x - BRK_PADDLE_W / 2;
    int16_t pad_right = s_brk.paddle_x + BRK_PADDLE_W / 2;
    int16_t pad_top = BRK_PADDLE_Y;

    if (s_brk.ball_vy > 0 &&  /* Ball moving down */
        by + br >= pad_top && by + br <= pad_top + BRK_PADDLE_H + 4 &&
        bx >= pad_left - br && bx <= pad_right + br) {
        /* Reflect with angle based on where ball hits paddle */
        int16_t offset = bx - s_brk.paddle_x;  /* -PADDLE_W/2 .. +PADDLE_W/2 */
        int speed = BRK_BALL_SPEED_INIT + s_brk.level * 2;
        /* Map offset to angle: -60..+60 degrees */
        int angle = offset * 60 / (BRK_PADDLE_W / 2);
        if (angle > 55) angle = 55;
        if (angle < -55) angle = -55;

        /* sin/cos lookup */
        static const int8_t sin_tbl2[61] = {
             0,  3,  5,  8, 10, 13, 15, 18, 20, 22, 25,
            27, 29, 31, 34, 36, 38, 40, 42, 44, 46,
            48, 50, 52, 53, 55, 57, 58, 60, 62, 63,
            65, 66, 67, 69, 70, 71, 72, 73, 74, 75,
            76, 77, 78, 79, 80, 81, 81, 82, 83, 83,
            84, 84, 85, 85, 86, 86, 86, 87, 87, 87
        };
        int idx = angle < 0 ? -angle : angle;
        if (idx > 55) idx = 55;
        int sv = sin_tbl2[idx];
        int cv = sin_tbl2[60 - idx];

        s_brk.ball_vx = BRK_INT_TO_FIX(speed) * (angle < 0 ? -sv : sv) / 256;
        s_brk.ball_vy = -BRK_INT_TO_FIX(speed) * cv / 256;
        if (BRK_FIX_TO_INT(-s_brk.ball_vy) < 2) {
            s_brk.ball_vy = -BRK_INT_TO_FIX(2);
        }

        s_brk.ball_y = BRK_INT_TO_FIX(pad_top - br - 1);
        by = BRK_FIX_TO_INT(s_brk.ball_y);
    }

    /* Brick collision */
    int16_t hit_row, hit_col;
    if (brk_check_brick_collision(bx, by, br, &hit_row, &hit_col)) {
        /* Determine reflection direction based on which side of the brick was hit */
        ui_rect_t brect = brk_brick_rect(hit_row, hit_col);

        /* Calculate overlap on each side */
        int16_t overlap_left   = (bx + br) - brect.x;              /* ball right edge - brick left */
        int16_t overlap_right  = (brect.x + brect.w) - (bx - br); /* brick right - ball left edge */
        int16_t overlap_top    = (by + br) - brect.y;              /* ball bottom edge - brick top */
        int16_t overlap_bottom = (brect.y + brect.h) - (by - br);  /* brick bottom - ball top edge */

        /* The smallest overlap indicates which side was hit */
        int16_t min_overlap = overlap_left;
        int side = 0;  /* 0=left, 1=right, 2=top, 3=bottom */
        if (overlap_right < min_overlap) { min_overlap = overlap_right; side = 1; }
        if (overlap_top < min_overlap)   { min_overlap = overlap_top;   side = 2; }
        if (overlap_bottom < min_overlap){ min_overlap = overlap_bottom; side = 3; }

        /* Reflect and push ball out of brick */
        switch (side) {
            case 0:  /* Hit left side of brick */
                s_brk.ball_vx = -BRK_INT_TO_FIX(abs(BRK_FIX_TO_INT(s_brk.ball_vx)));
                s_brk.ball_x = BRK_INT_TO_FIX(brect.x - br - 1);
                break;
            case 1:  /* Hit right side of brick */
                s_brk.ball_vx = BRK_INT_TO_FIX(abs(BRK_FIX_TO_INT(s_brk.ball_vx)));
                s_brk.ball_x = BRK_INT_TO_FIX(brect.x + brect.w + br + 1);
                break;
            case 2:  /* Hit top side of brick */
                s_brk.ball_vy = -BRK_INT_TO_FIX(abs(BRK_FIX_TO_INT(s_brk.ball_vy)));
                s_brk.ball_y = BRK_INT_TO_FIX(brect.y - br - 1);
                break;
            case 3:  /* Hit bottom side of brick */
                s_brk.ball_vy = BRK_INT_TO_FIX(abs(BRK_FIX_TO_INT(s_brk.ball_vy)));
                s_brk.ball_y = BRK_INT_TO_FIX(brect.y + brect.h + br + 1);
                break;
        }

        /* Score */
        s_brk.score += s_brick_scores[hit_row] * (s_brk.level + 1);
        brk_update_texts();

        /* Invalidate the destroyed brick */
        ui_rect_t inv = brk_brick_rect(hit_row, hit_col);
        ui_page_invalidate(&inv);
        /* Invalidate score */
        ui_rect_t sr = {BRK_PANEL_X, BRK_SCORE_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
        ui_page_invalidate(&sr);

        /* Check win */
        if (brk_bricks_remaining() == 0) {
            s_brk.level++;
            brk_init_level();
            brk_update_texts();
            return;
        }
    }

    /* Ball fell below paddle */
    if (by - br > BRK_PLAY_BOTTOM) {
        s_brk.lives--;
        brk_update_texts();

        /* Invalidate lives */
        ui_rect_t lr = {BRK_PANEL_X, BRK_LIVES_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
        ui_page_invalidate(&lr);

        if (s_brk.lives <= 0) {
            s_brk.state = BRK_STATE_GAMEOVER;
            s_brk.need_full_redraw = true;
        } else {
            brk_reset_ball();
        }
        return;
    }

    /* Invalidate old and new ball positions */
    int16_t old_bx = BRK_FIX_TO_INT(s_brk.ball_x - s_brk.ball_vx);
    int16_t old_by = BRK_FIX_TO_INT(s_brk.ball_y - s_brk.ball_vy);
    ui_rect_t old_ball = {old_bx - br - 2, old_by - br - 2,
                          2 * br + 4, 2 * br + 4};
    ui_rect_t new_ball = {BRK_FIX_TO_INT(s_brk.ball_x) - br - 2,
                          BRK_FIX_TO_INT(s_brk.ball_y) - br - 2,
                          2 * br + 4, 2 * br + 4};
    ui_page_invalidate(&old_ball);
    ui_page_invalidate(&new_ball);

    /* Invalidate old and new paddle positions */
    ui_rect_t old_pad = {s_brk.paddle_x - BRK_PADDLE_W / 2 - 2, BRK_PADDLE_Y - 2,
                         BRK_PADDLE_W + 4, BRK_PADDLE_H + 4};
    ui_page_invalidate(&old_pad);
}

/* Move paddle to an absolute x position (for DRAG events) */
static void brk_move_paddle_to(int16_t target_x)
{
    if (s_brk.state != BRK_STATE_PLAYING && s_brk.state != BRK_STATE_LAUNCH) return;

    int16_t old_x = s_brk.paddle_x;
    int16_t half_w = BRK_PADDLE_W / 2;
    int16_t new_x = target_x;

    /* Clamp to play area */
    if (new_x - half_w < BRK_PLAY_X) new_x = BRK_PLAY_X + half_w;
    if (new_x + half_w > BRK_PLAY_X + BRK_PLAY_W) new_x = BRK_PLAY_X + BRK_PLAY_W - half_w;

    if (new_x == old_x) return;  /* No movement */

    s_brk.paddle_x = new_x;

    /* If in launch state, ball follows paddle */
    if (s_brk.state == BRK_STATE_LAUNCH) {
        s_brk.ball_x = BRK_INT_TO_FIX(s_brk.paddle_x);
        s_brk.ball_y = BRK_INT_TO_FIX(BRK_PADDLE_Y - BRK_BALL_R - 1);
    }

    /* Invalidate the union of old and new paddle positions */
    int16_t left  = (old_x < new_x ? old_x : new_x) - half_w - 2;
    int16_t right = (old_x > new_x ? old_x : new_x) + half_w + 2;
    ui_rect_t pad_union = {left, BRK_PADDLE_Y - 2,
                           right - left, BRK_PADDLE_H + 4};
    ui_page_invalidate(&pad_union);

    if (s_brk.state == BRK_STATE_LAUNCH) {
        int16_t br = BRK_BALL_R;
        int16_t ball_y = BRK_PADDLE_Y - BRK_BALL_R - 1;
        int16_t b_left  = (old_x < new_x ? old_x : new_x) - br - 2;
        int16_t b_right = (old_x > new_x ? old_x : new_x) + br + 2;
        ui_rect_t ball_union = {b_left, ball_y - br - 2,
                                b_right - b_left, 2 * br + 4};
        ui_page_invalidate(&ball_union);
    }
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void brk_draw_brick(int row, int col)
{
    if (!s_brk.bricks[row][col]) return;

    ui_rect_t r = brk_brick_rect(row, col);
    ui_color_t fill = s_brick_colors[row];
    ui_color_t border = ui_color_darken(fill, 30);
    ui_color_t highlight = ui_color_lighten(fill, 40);

    ui_draw_fill_rect(&r, fill);
    /* 3D effect: top/left highlight, bottom/right shadow */
    ui_draw_hline(r.x, r.y, r.w, highlight);
    ui_draw_vline(r.x, r.y, r.h, highlight);
    ui_draw_hline(r.x, r.y + r.h - 1, r.w, border);
    ui_draw_vline(r.x + r.w - 1, r.y, r.h, border);
}

static void brk_draw_all_bricks(void)
{
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            brk_draw_brick(r, c);
        }
    }
}

static void brk_draw_paddle(void)
{
    int16_t x = s_brk.paddle_x - BRK_PADDLE_W / 2;
    ui_rect_t r = {x, BRK_PADDLE_Y, BRK_PADDLE_W, BRK_PADDLE_H};
    ui_color_t fill = UI_HEX(0xE0E0E0);
    ui_color_t highlight = UI_COLOR_WHITE;

    ui_draw_fill_round_rect(&r, 4, fill);
    /* Highlight on top */
    ui_draw_hline(x + 2, BRK_PADDLE_Y, BRK_PADDLE_W - 4, highlight);
}

static void brk_draw_ball(void)
{
    int16_t bx = BRK_FIX_TO_INT(s_brk.ball_x);
    int16_t by = BRK_FIX_TO_INT(s_brk.ball_y);
    ui_draw_fill_circle(bx, by, BRK_BALL_R, UI_COLOR_WHITE);
}

/* Erase a region with play area background */
static void brk_erase_rect(const ui_rect_t *r)
{
    ui_draw_fill_rect(r, UI_HEX(0x1A1A1A));
}

static void brk_draw_panel(void)
{
    /* Score */
    ui_draw_text(BRK_PANEL_X, BRK_SCORE_LABEL_Y, "SCORE", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t score_bg = {BRK_PANEL_X, BRK_SCORE_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
    ui_draw_fill_round_rect(&score_bg, BRK_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&score_bg, s_brk.buf_score, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Lives */
    ui_draw_text(BRK_PANEL_X, BRK_LIVES_LABEL_Y, "LIVES", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t lives_bg = {BRK_PANEL_X, BRK_LIVES_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
    ui_draw_fill_round_rect(&lives_bg, BRK_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&lives_bg, s_brk.buf_lives, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Level */
    ui_draw_text(BRK_PANEL_X, BRK_LEVEL_LABEL_Y, "LEVEL", &font_montserrat_12, UI_HEX(0xAAAAAA));
    ui_rect_t level_bg = {BRK_PANEL_X, BRK_LEVEL_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
    ui_draw_fill_round_rect(&level_bg, BRK_INFO_BOX_R, UI_HEX(0x3C3C3C));
    ui_draw_text_in_rect(&level_bg, s_brk.buf_level, &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Control buttons */
    brk_draw_ctrl_btns();
    brk_draw_launch_btn();
}

static void brk_draw_ctrl_btns(void)
{
    ui_color_t bg = UI_HEX(0x3C3C3C);
    ui_color_t fg = UI_COLOR_WHITE;

    ui_rect_t lt_r = {BRK_CTRL_LEFT_X, BRK_CTRL_LEFT_Y, BRK_CTRL_BTN_W, BRK_CTRL_BTN_H};
    ui_draw_fill_round_rect(&lt_r, 6, bg);
    ui_draw_text_in_rect(&lt_r, "<", &font_montserrat_16, fg, 1);

    ui_rect_t rt_r = {BRK_CTRL_RIGHT_X, BRK_CTRL_RIGHT_Y, BRK_CTRL_BTN_W, BRK_CTRL_BTN_H};
    ui_draw_fill_round_rect(&rt_r, 6, bg);
    ui_draw_text_in_rect(&rt_r, ">", &font_montserrat_16, fg, 1);
}

static void brk_draw_launch_btn(void)
{
    ui_rect_t r = {BRK_LAUNCH_X, BRK_LAUNCH_Y, BRK_LAUNCH_W, BRK_LAUNCH_H};
    ui_color_t color = (s_brk.state == BRK_STATE_LAUNCH) ? UI_HEX(0x4CAF50) : UI_HEX(0x3C3C3C);
    ui_draw_fill_round_rect(&r, 6, color);
    const char *text = (s_brk.state == BRK_STATE_LAUNCH) ? "LAUNCH" : "START";
    ui_draw_text_in_rect(&r, text, &font_montserrat_12, UI_COLOR_WHITE, 1);
}

/* Draw idle/gameover/win overlay on play area */
static void brk_draw_overlay(const char *title, ui_color_t title_color, const char *subtitle)
{
    /* Semi-transparent overlay on play area */
    ui_rect_t overlay = {BRK_PLAY_X, APP_TITLE_BAR_H,
                         BRK_PLAY_W, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_color_t overlay_bg = ui_color_blend(UI_HEX(0x000000), UI_HEX(0x1A1A1A), 160);
    ui_draw_fill_rect(&overlay, overlay_bg);

    ui_rect_t title_r = {BRK_PLAY_X, APP_TITLE_BAR_H + 160, BRK_PLAY_W, 30};
    ui_draw_text_in_rect(&title_r, title, &font_montserrat_16, title_color, 1);

    ui_rect_t sub_r = {BRK_PLAY_X, APP_TITLE_BAR_H + 200, BRK_PLAY_W, 24};
    ui_draw_text_in_rect(&sub_r, subtitle, &font_montserrat_12, UI_HEX(0xAAAAAA), 1);
}

/*=============================================================================
 *  Input Handling
 *=============================================================================*/

static void brk_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    if (e->type == UI_EVENT_CLICK) {
        if (s_brk.state == BRK_STATE_IDLE || s_brk.state == BRK_STATE_GAMEOVER || s_brk.state == BRK_STATE_WIN) {
            brk_start_game();
        } else if (s_brk.state == BRK_STATE_LAUNCH) {
            /* Only launch when tapping on play area */
            int16_t tx = e->pos.x;
            int16_t ty = e->pos.y;
            if (tx >= BRK_PLAY_X && tx < BRK_PLAY_X + BRK_PLAY_W &&
                ty >= APP_TITLE_BAR_H && ty < UI_SCREEN_HEIGHT) {
                brk_launch_ball();
            }
        }
    } else if (e->type == UI_EVENT_MOVE) {
        if (s_brk.state == BRK_STATE_PLAYING || s_brk.state == BRK_STATE_LAUNCH) {
            /* Move paddle to follow finger x position */
            brk_move_paddle_to(e->pos.x);
        }
    } else if (e->type == UI_EVENT_SWIPE_UP) {
        if (s_brk.state == BRK_STATE_LAUNCH) {
            brk_launch_ball();
        }
    }
}

static void brk_ctrl_event(ui_widget_t *w, ui_event_t *e)
{
    if (e->type != UI_EVENT_DOWN && e->type != UI_EVENT_LONG_PRESS &&
        e->type != UI_EVENT_LONG_PRESS_REPEAT) return;
    if (s_brk.state != BRK_STATE_PLAYING && s_brk.state != BRK_STATE_LAUNCH) return;

    int speed = (e->type == UI_EVENT_LONG_PRESS || e->type == UI_EVENT_LONG_PRESS_REPEAT)
                ? BRK_PADDLE_HOLD_SPEED : BRK_PADDLE_SPEED;
    int dir = 0;
    if (w == &s_btn_left) dir = -1;
    else if (w == &s_btn_right) dir = 1;
    if (dir) {
        int16_t old_x = s_brk.paddle_x;
        int16_t new_x = s_brk.paddle_x + dir * speed;
        int16_t half_w = BRK_PADDLE_W / 2;
        if (new_x - half_w < BRK_PLAY_X) new_x = BRK_PLAY_X + half_w;
        if (new_x + half_w > BRK_PLAY_X + BRK_PLAY_W) new_x = BRK_PLAY_X + BRK_PLAY_W - half_w;
        s_brk.paddle_x = new_x;

        if (s_brk.state == BRK_STATE_LAUNCH) {
            s_brk.ball_x = BRK_INT_TO_FIX(s_brk.paddle_x);
            s_brk.ball_y = BRK_INT_TO_FIX(BRK_PADDLE_Y - BRK_BALL_R - 1);
        }

        /* Invalidate the union of old and new paddle positions */
        int16_t left  = (old_x < new_x ? old_x : new_x) - half_w - 2;
        int16_t right = (old_x > new_x ? old_x : new_x) + half_w + 2;
        ui_rect_t pad_union = {left, BRK_PADDLE_Y - 2,
                               right - left, BRK_PADDLE_H + 4};
        ui_page_invalidate(&pad_union);

        if (s_brk.state == BRK_STATE_LAUNCH) {
            int16_t br = BRK_BALL_R;
            int16_t ball_y = BRK_PADDLE_Y - BRK_BALL_R - 1;
            int16_t b_left  = (old_x < new_x ? old_x : new_x) - br - 2;
            int16_t b_right = (old_x > new_x ? old_x : new_x) + br + 2;
            ui_rect_t ball_union = {b_left, ball_y - br - 2,
                                    b_right - b_left, 2 * br + 4};
            ui_page_invalidate(&ball_union);
        }
    }
}

static void brk_launch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type != UI_EVENT_CLICK) return;

    if (s_brk.state == BRK_STATE_IDLE || s_brk.state == BRK_STATE_GAMEOVER || s_brk.state == BRK_STATE_WIN) {
        brk_start_game();
    } else if (s_brk.state == BRK_STATE_LAUNCH) {
        brk_launch_ball();
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void brk_game_enter(ui_page_t *page)
{
    (void)page;
    memset(&s_brk, 0, sizeof(s_brk));
    s_brk.state = BRK_STATE_IDLE;
    brk_update_texts();
    s_brk.need_full_redraw = true;
    s_brk.paddle_x = BRK_PLAY_X + BRK_PLAY_W / 2;
}

static void brk_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)dirty;

    /* Game tick */
    brk_game_tick();

    /* Full redraw */
    if (s_brk.need_full_redraw) {
        s_brk.need_full_redraw = false;

        /* Title bar */
        ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
        ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

        ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
        for (uint16_t j = 0; j < page->widget_count; j++) {
            if (page->widgets[j]) {
                ui_widget_draw(page->widgets[j], &full);
            }
        }

        /* Game area background - fill entire screen below title bar */
        ui_rect_t full_bg = {0, APP_TITLE_BAR_H,
                             UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
        ui_draw_fill_rect(&full_bg, UI_HEX(0x1A1A1A));

        if (s_brk.state == BRK_STATE_IDLE) {
            brk_draw_all_bricks();
            brk_draw_panel();
            brk_draw_overlay("BREAKOUT", UI_COLOR_WHITE, "Swipe or press to start");
        } else {
            brk_draw_all_bricks();
            brk_draw_paddle();
            brk_draw_ball();
            brk_draw_panel();

            if (s_brk.state == BRK_STATE_GAMEOVER) {
                brk_draw_overlay("GAME OVER", UI_HEX(0xF44336), "Press to restart");
            } else if (s_brk.state == BRK_STATE_WIN) {
                brk_draw_overlay("YOU WIN!", UI_HEX(0x4CAF50), "Press to play again");
            }
        }

        /* Update drawn positions after full redraw */
        s_brk.drawn_paddle_x = s_brk.paddle_x;
        s_brk.drawn_ball_x = BRK_FIX_TO_INT(s_brk.ball_x);
        s_brk.drawn_ball_y = BRK_FIX_TO_INT(s_brk.ball_y);

        ui_page_clear_dirty();
        return;
    }

    /* Partial redraw */
    const ui_dirty_list_t *dl = ui_page_get_dirty_list();
    if (dl->count == 0) return;

    if (s_brk.state == BRK_STATE_IDLE || s_brk.state == BRK_STATE_GAMEOVER || s_brk.state == BRK_STATE_WIN) {
        ui_page_clear_dirty();
        return;
    }

    ui_rect_t game_area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                           UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};

    /* Panel element rects */
    ui_rect_t score_rect = {BRK_PANEL_X, BRK_SCORE_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
    ui_rect_t lives_rect = {BRK_PANEL_X, BRK_LIVES_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
    ui_rect_t level_rect = {BRK_PANEL_X, BRK_LEVEL_BOX_Y, BRK_PANEL_W, BRK_INFO_BOX_H};
    ui_rect_t launch_rect = {BRK_LAUNCH_X, BRK_LAUNCH_Y, BRK_LAUNCH_W, BRK_LAUNCH_H};
    bool score_needs = false, lives_needs = false, level_needs = false;
    bool launch_needs = false;

    /* Track which bricks need redraw */
    bool brick_dirty[BRK_ROWS][BRK_COLS];
    memset(brick_dirty, 0, sizeof(brick_dirty));

    /* Track ball and paddle redraw */
    bool ball_needs = false, paddle_needs = false;

    /* Current paddle rect */
    int16_t pad_x = s_brk.paddle_x - BRK_PADDLE_W / 2;
    ui_rect_t paddle_rect = {pad_x, BRK_PADDLE_Y, BRK_PADDLE_W, BRK_PADDLE_H};
    /* Drawn paddle rect (where paddle was last drawn) */
    int16_t drawn_pad_x = s_brk.drawn_paddle_x - BRK_PADDLE_W / 2;
    ui_rect_t drawn_paddle_rect = {drawn_pad_x, BRK_PADDLE_Y, BRK_PADDLE_W, BRK_PADDLE_H};

    /* Current ball rect */
    int16_t bx = BRK_FIX_TO_INT(s_brk.ball_x);
    int16_t by = BRK_FIX_TO_INT(s_brk.ball_y);
    ui_rect_t ball_rect = {bx - BRK_BALL_R, by - BRK_BALL_R,
                           2 * BRK_BALL_R, 2 * BRK_BALL_R};
    /* Drawn ball rect */
    ui_rect_t drawn_ball_rect = {s_brk.drawn_ball_x - BRK_BALL_R, s_brk.drawn_ball_y - BRK_BALL_R,
                                 2 * BRK_BALL_R, 2 * BRK_BALL_R};

    for (uint16_t i = 0; i < dl->count; i++) {
        const ui_rect_t *d = &dl->regions[i];
        if (d->y + d->h <= APP_TITLE_BAR_H) continue;

        /* Check panel overlaps */
        if (d->x < score_rect.x + score_rect.w &&
            d->x + d->w > score_rect.x &&
            d->y < score_rect.y + score_rect.h &&
            d->y + d->h > score_rect.y)
            score_needs = true;
        if (d->x < lives_rect.x + lives_rect.w &&
            d->x + d->w > lives_rect.x &&
            d->y < lives_rect.y + lives_rect.h &&
            d->y + d->h > lives_rect.y)
            lives_needs = true;
        if (d->x < level_rect.x + level_rect.w &&
            d->x + d->w > level_rect.x &&
            d->y < level_rect.y + level_rect.h &&
            d->y + d->h > level_rect.y)
            level_needs = true;
        if (d->x < launch_rect.x + launch_rect.w &&
            d->x + d->w > launch_rect.x &&
            d->y < launch_rect.y + launch_rect.h &&
            d->y + d->h > launch_rect.y)
            launch_needs = true;

        /* Check brick overlaps */
        if (d->x < BRK_GRID_X + BRK_GRID_W &&
            d->x + d->w > BRK_GRID_X &&
            d->y < BRK_GRID_Y + BRK_GRID_H &&
            d->y + d->h > BRK_GRID_Y) {
            int c_start = (d->x > BRK_GRID_X) ?
                          (d->x - BRK_GRID_X) / (BRK_CELL_W + BRK_GAP) : 0;
            int c_end = (d->x + d->w < BRK_GRID_X + BRK_GRID_W) ?
                        (d->x + d->w - BRK_GRID_X + BRK_CELL_W + BRK_GAP - 1) / (BRK_CELL_W + BRK_GAP) : BRK_COLS;
            int r_start = (d->y > BRK_GRID_Y) ?
                          (d->y - BRK_GRID_Y) / (BRK_CELL_H + BRK_GAP) : 0;
            int r_end = (d->y + d->h < BRK_GRID_Y + BRK_GRID_H) ?
                        (d->y + d->h - BRK_GRID_Y + BRK_CELL_H + BRK_GAP - 1) / (BRK_CELL_H + BRK_GAP) : BRK_ROWS;

            if (c_start < 0) c_start = 0;
            if (c_end > BRK_COLS) c_end = BRK_COLS;
            if (r_start < 0) r_start = 0;
            if (r_end > BRK_ROWS) r_end = BRK_ROWS;

            for (int r = r_start; r < r_end; r++)
                for (int c = c_start; c < c_end; c++)
                    brick_dirty[r][c] = true;
        }

        /* Check ball/paddle overlap (both current and last-drawn positions) */
        if ((d->x < ball_rect.x + ball_rect.w &&
             d->x + d->w > ball_rect.x &&
             d->y < ball_rect.y + ball_rect.h &&
             d->y + d->h > ball_rect.y) ||
            (d->x < drawn_ball_rect.x + drawn_ball_rect.w &&
             d->x + d->w > drawn_ball_rect.x &&
             d->y < drawn_ball_rect.y + drawn_ball_rect.h &&
             d->y + d->h > drawn_ball_rect.y))
            ball_needs = true;
        if ((d->x < paddle_rect.x + paddle_rect.w &&
             d->x + d->w > paddle_rect.x &&
             d->y < paddle_rect.y + paddle_rect.h &&
             d->y + d->h > paddle_rect.y) ||
            (d->x < drawn_paddle_rect.x + drawn_paddle_rect.w &&
             d->x + d->w > drawn_paddle_rect.x &&
             d->y < drawn_paddle_rect.y + drawn_paddle_rect.h &&
             d->y + d->h > drawn_paddle_rect.y))
            paddle_needs = true;
    }

    ui_render_set_clip(&game_area);

    /* Redraw dirty bricks */
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            if (!brick_dirty[r][c]) continue;
            ui_rect_t br = brk_brick_rect(r, c);
            /* Erase brick area with background */
            brk_erase_rect(&br);
            /* Redraw if still present */
            if (s_brk.bricks[r][c]) {
                brk_draw_brick(r, c);
            }
        }
    }

    /* Redraw ball */
    if (ball_needs) {
        /* Erase the union of drawn and current ball positions */
        int16_t er = BRK_BALL_R + 2;
        int16_t min_x = s_brk.drawn_ball_x < bx ? s_brk.drawn_ball_x : bx;
        int16_t min_y = s_brk.drawn_ball_y < by ? s_brk.drawn_ball_y : by;
        int16_t max_x = s_brk.drawn_ball_x > bx ? s_brk.drawn_ball_x : bx;
        int16_t max_y = s_brk.drawn_ball_y > by ? s_brk.drawn_ball_y : by;
        ui_rect_t erase_r = {min_x - er, min_y - er,
                             max_x - min_x + 2 * er, max_y - min_y + 2 * er};
        brk_erase_rect(&erase_r);
        brk_draw_ball();
        s_brk.drawn_ball_x = bx;
        s_brk.drawn_ball_y = by;
    }

    /* Redraw paddle */
    if (paddle_needs) {
        /* Erase the union of drawn and current paddle positions */
        int16_t min_x = drawn_pad_x < pad_x ? drawn_pad_x : pad_x;
        int16_t max_x = drawn_pad_x > pad_x ? drawn_pad_x : pad_x;
        ui_rect_t erase_r = {min_x - 2, BRK_PADDLE_Y - 2,
                             max_x - min_x + BRK_PADDLE_W + 4, BRK_PADDLE_H + 4};
        brk_erase_rect(&erase_r);
        brk_draw_paddle();
        s_brk.drawn_paddle_x = s_brk.paddle_x;
    }

    /* Redraw panel elements */
    if (score_needs) {
        ui_render_set_clip(&score_rect);
        ui_draw_fill_round_rect(&score_rect, BRK_INFO_BOX_R, UI_HEX(0x3C3C3C));
        ui_render_set_clip(&game_area);
        ui_draw_text_in_rect(&score_rect, s_brk.buf_score, &font_montserrat_16,
                             UI_COLOR_WHITE, 1);
    }
    if (lives_needs) {
        ui_render_set_clip(&lives_rect);
        ui_draw_fill_round_rect(&lives_rect, BRK_INFO_BOX_R, UI_HEX(0x3C3C3C));
        ui_render_set_clip(&game_area);
        ui_draw_text_in_rect(&lives_rect, s_brk.buf_lives, &font_montserrat_16,
                             UI_COLOR_WHITE, 1);
    }
    if (level_needs) {
        ui_render_set_clip(&level_rect);
        ui_draw_fill_round_rect(&level_rect, BRK_INFO_BOX_R, UI_HEX(0x3C3C3C));
        ui_render_set_clip(&game_area);
        ui_draw_text_in_rect(&level_rect, s_brk.buf_level, &font_montserrat_16,
                             UI_COLOR_WHITE, 1);
    }
    if (launch_needs) {
        ui_render_set_clip(&launch_rect);
        brk_draw_launch_btn();
    }

    ui_render_reset_clip();
    ui_page_clear_dirty();
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_breakout_init(void)
{
    ui_app_page_init(&s_game_breakout, "Breakout", 0x203);
    s_game_breakout.page.flags |= UI_PAGE_FLAG_GAME;

    /* Touch area for swipe/click */
    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = brk_touch_event;

    /* Left button */
    ui_rect_t lt_r = {BRK_CTRL_LEFT_X, BRK_CTRL_LEFT_Y, BRK_CTRL_BTN_W, BRK_CTRL_BTN_H};
    ui_widget_init(&s_btn_left, &lt_r);
    s_btn_left.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_left.event_cb = brk_ctrl_event;

    /* Right button */
    ui_rect_t rt_r = {BRK_CTRL_RIGHT_X, BRK_CTRL_RIGHT_Y, BRK_CTRL_BTN_W, BRK_CTRL_BTN_H};
    ui_widget_init(&s_btn_right, &rt_r);
    s_btn_right.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_right.event_cb = brk_ctrl_event;

    /* Launch button */
    ui_rect_t launch_r = {BRK_LAUNCH_X, BRK_LAUNCH_Y, BRK_LAUNCH_W, BRK_LAUNCH_H};
    ui_widget_init(&s_btn_launch, &launch_r);
    s_btn_launch.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_launch.event_cb = brk_launch_event;

    /* Widget order: high index = high priority for events */
    s_brk_widgets[0] = &s_touch_area;
    s_brk_widgets[1] = &s_btn_left;
    s_brk_widgets[2] = &s_btn_right;
    s_brk_widgets[3] = &s_btn_launch;
    s_brk_widgets[4] = (ui_widget_t *)&s_game_breakout.lbl_title;
    s_brk_widgets[5] = (ui_widget_t *)&s_game_breakout.btn_back;

    ui_page_set_widgets(&s_game_breakout.page, s_brk_widgets, 6);
    ui_page_set_callbacks(&s_game_breakout.page, brk_game_enter, NULL,
                          brk_game_draw, NULL);
    ui_page_register(&s_game_breakout.page);
}

ui_page_t *game_breakout_get_page(void)
{
    return &s_game_breakout.page;
}
