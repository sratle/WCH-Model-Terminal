/********************************** (C) COPYRIGHT *******************************
* File Name          : game_touchball.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/19
* Description        : Touch Ball game.
*                      Tap flying balls to score. Missed balls reduce HP.
********************************************************************************/
#include "game_touchball.h"
#include "../UI/ui_app_common.h"
#include <string.h>

/*=============================================================================
 *  Game Configuration
 *=============================================================================*/

#define TB_MAX_BALLS        6       /* Max balls on screen */
#define TB_SPAWN_INTERVAL   35      /* Frames between spawns (~0.35s @ 100fps) */
#define TB_INIT_HP          10
#define TB_BALL_MIN_R       20
#define TB_BALL_MAX_R       36
#define TB_BALL_MIN_SPEED   3
#define TB_BALL_MAX_SPEED   5
#define TB_SCORE_PER_HIT    10
#define TB_HP_LOSS_MISS     1

#define TB_AREA_X           0
#define TB_AREA_Y           APP_TITLE_BAR_H
#define TB_AREA_W           UI_SCREEN_WIDTH
#define TB_AREA_H           (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H)

/* Ball colors - macaron palette */
static const ui_color_t s_ball_colors[] = {
    UI_HEX(0xF5B7B1),  /* Soft pink */
    UI_HEX(0x7EC8C8),  /* Mint */
    UI_HEX(0xFAD7A0),  /* Soft yellow */
    UI_HEX(0xB8E0D2),  /* Light mint */
    UI_HEX(0xC5A3E0),  /* Soft purple */
    UI_HEX(0xA3D5E0),  /* Soft blue */
    UI_HEX(0xFFB6C1),  /* Light pink */
    UI_HEX(0x90EE90),  /* Light green */
};
#define TB_NUM_COLORS (sizeof(s_ball_colors) / sizeof(s_ball_colors[0]))

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    TB_STATE_IDLE,      /* Show "Tap to Start" */
    TB_STATE_PLAYING,   /* Game in progress */
    TB_STATE_GAMEOVER,  /* Show score, tap to restart */
} tb_state_enum_t;

typedef struct {
    int16_t x, y;       /* Fixed-point: actual = x >> 4 */
    int16_t vx, vy;     /* Fixed-point: actual = vx >> 4 */
    int16_t r;
    bool active;
    ui_color_t color;
    int16_t prev_x, prev_y;  /* Previous frame position (fixed-point) for dirty tracking */
} tb_ball_t;

typedef struct {
    tb_state_enum_t state;
    int score;
    int hp;
    int frame_count;
    int spawn_timer;
    tb_ball_t balls[TB_MAX_BALLS];

    /* Real FPS measurement */
    uint32_t fps_last_ms;
    int fps_frame_cnt;
    int fps_value;

    /* Dirty tracking: which areas need redraw */
    bool need_full_redraw;    /* State change: redraw everything */
    bool hud_dirty;           /* Score/HP/FPS text changed */
    bool fps_dirty;           /* FPS text only changed */

    /* Text buffers for HUD */
    char buf_score[16];
    char buf_hp[8];
    char buf_fps[12];
    char buf_gameover[24];
} tb_state_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_touchball;
static ui_widget_t s_touch_area;
static ui_widget_t *s_tb_widgets[3];
static tb_state_t s_tb;

/*=============================================================================
 *  Dirty Region Helpers
 *=============================================================================*/

/* Mark a ball's bounding box as dirty (with margin for highlight circle) */
static void tb_invalidate_ball(int16_t bx, int16_t by, int16_t r)
{
    int16_t margin = r + r / 3 + 2;  /* ball + highlight radius + padding */
    ui_rect_t rect = {
        TB_AREA_X + bx - margin,
        TB_AREA_Y + by - margin,
        margin * 2,
        margin * 2
    };
    ui_page_invalidate(&rect);
}

/* Mark HUD bar as dirty */
static void tb_invalidate_hud(void)
{
    ui_rect_t hud = {TB_AREA_X, TB_AREA_Y + TB_AREA_H - 32, TB_AREA_W, 32};
    ui_page_invalidate(&hud);
}

/* Check if two rectangles overlap */
static bool tb_rects_overlap(const ui_rect_t *a, const ui_rect_t *b)
{
    return (a->x < b->x + b->w && a->x + a->w > b->x &&
            a->y < b->y + b->h && a->y + a->h > b->y);
}

/*=============================================================================
 *  Simple Integer to ASCII (no snprintf dependency)
 *=============================================================================*/

static void tb_itoa(int val, char *out)
{
    char tmp[12];
    int i = 0;
    bool neg = val < 0;
    if (neg) val = -val;
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    int j = 0;
    if (neg) out[j++] = '-';
    while (i > 0) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
}

static void tb_strcpy(char *dst, const char *src)
{
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void tb_strcat(char *dst, const char *src)
{
    while (*dst) dst++;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

/*=============================================================================
 *  Simple RNG (xorshift32, no stdlib dependency)
 *=============================================================================*/

static uint32_t s_rng = 0xACE1u;

static uint32_t tb_rng(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int32_t tb_rng_range(int32_t min, int32_t max)
{
    if (max <= min) return min;
    return min + (int32_t)(tb_rng() % (uint32_t)(max - min + 1));
}

/*=============================================================================
 *  HUD Text Update
 *=============================================================================*/

static void tb_update_score_text(void)
{
    tb_strcpy(s_tb.buf_score, "Score: ");
    char num[8];
    tb_itoa(s_tb.score, num);
    tb_strcat(s_tb.buf_score, num);
}

static void tb_update_hp_text(void)
{
    tb_strcpy(s_tb.buf_hp, "HP: ");
    char num[4];
    tb_itoa(s_tb.hp, num);
    tb_strcat(s_tb.buf_hp, num);
}

static void tb_update_fps_text(void)
{
    tb_strcpy(s_tb.buf_fps, "FPS: ");
    char num[6];
    tb_itoa(s_tb.fps_value, num);
    tb_strcat(s_tb.buf_fps, num);
}

static void tb_update_gameover_text(void)
{
    tb_strcpy(s_tb.buf_gameover, "Score: ");
    char num[8];
    tb_itoa(s_tb.score, num);
    tb_strcat(s_tb.buf_gameover, num);
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

static void tb_reset(void)
{
    memset(&s_tb, 0, sizeof(s_tb));
    s_tb.state = TB_STATE_IDLE;
    s_tb.hp = TB_INIT_HP;
    s_tb.spawn_timer = TB_SPAWN_INTERVAL;
    s_tb.fps_value = 0;
    s_tb.fps_last_ms = ui_get_real_ms();
    s_tb.fps_frame_cnt = 0;
    tb_strcpy(s_tb.buf_fps, "FPS: --");
    for (int i = 0; i < TB_MAX_BALLS; i++) {
        s_tb.balls[i].active = false;
    }
    tb_update_score_text();
    tb_update_hp_text();
}

static void tb_start_game(void)
{
    s_tb.state = TB_STATE_PLAYING;
    s_tb.score = 0;
    s_tb.hp = TB_INIT_HP;
    s_tb.frame_count = 0;
    s_tb.spawn_timer = TB_SPAWN_INTERVAL;
    s_tb.fps_last_ms = ui_get_real_ms();
    s_tb.fps_frame_cnt = 0;
    for (int i = 0; i < TB_MAX_BALLS; i++) {
        s_tb.balls[i].active = false;
    }
    tb_update_score_text();
    tb_update_hp_text();
}

static void tb_spawn_ball(void)
{
    /* Find inactive slot */
    int slot = -1;
    for (int i = 0; i < TB_MAX_BALLS; i++) {
        if (!s_tb.balls[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return; /* All slots full */

    tb_ball_t *b = &s_tb.balls[slot];

    int16_t r = tb_rng_range(TB_BALL_MIN_R, TB_BALL_MAX_R);
    int16_t speed = tb_rng_range(TB_BALL_MIN_SPEED, TB_BALL_MAX_SPEED);

    /* Spawn from random edge, fly toward center area */
    int edge = tb_rng_range(0, 3); /* 0=top, 1=right, 2=bottom, 3=left */

    switch (edge) {
        case 0: /* top */
            b->x = (tb_rng_range(r, TB_AREA_W - r) << 4);
            b->y = (-r << 4);
            b->vx = (tb_rng_range(-speed, speed) << 4);
            b->vy = (speed << 4);
            break;
        case 1: /* right */
            b->x = ((TB_AREA_W + r) << 4);
            b->y = (tb_rng_range(r, TB_AREA_H - r) << 4);
            b->vx = (-speed << 4);
            b->vy = (tb_rng_range(-speed, speed) << 4);
            break;
        case 2: /* bottom */
            b->x = (tb_rng_range(r, TB_AREA_W - r) << 4);
            b->y = ((TB_AREA_H + r) << 4);
            b->vx = (tb_rng_range(-speed, speed) << 4);
            b->vy = (-speed << 4);
            break;
        default: /* left */
            b->x = (-r << 4);
            b->y = (tb_rng_range(r, TB_AREA_H - r) << 4);
            b->vx = (speed << 4);
            b->vy = (tb_rng_range(-speed, speed) << 4);
            break;
    }

    b->r = r;
    b->active = true;
    b->color = s_ball_colors[tb_rng_range(0, TB_NUM_COLORS - 1)];
    b->prev_x = b->x;
    b->prev_y = b->y;

    /* Mark spawn position as dirty */
    tb_invalidate_ball(b->x >> 4, b->y >> 4, r);
}

static void tb_update(void)
{
    if (s_tb.state != TB_STATE_PLAYING) return;

    s_tb.frame_count++;
    s_tb.fps_frame_cnt++;

    /* Update real FPS every 2 seconds */
    uint32_t now = ui_get_real_ms();
    uint32_t elapsed = now - s_tb.fps_last_ms;
    if (elapsed >= 2000) {
        if (elapsed > 0) {
            s_tb.fps_value = (int)(s_tb.fps_frame_cnt * 1000 / elapsed);
        }
        s_tb.fps_frame_cnt = 0;
        s_tb.fps_last_ms = now;
        tb_update_fps_text();
        s_tb.fps_dirty = true;
    }

    /* Spawn timer */
    s_tb.spawn_timer--;
    if (s_tb.spawn_timer <= 0) {
        tb_spawn_ball();
        s_tb.spawn_timer = TB_SPAWN_INTERVAL;
    }

    /* Update balls and mark dirty regions */
    for (int i = 0; i < TB_MAX_BALLS; i++) {
        tb_ball_t *b = &s_tb.balls[i];
        if (!b->active) continue;

        /* Save old position for dirty tracking */
        b->prev_x = b->x;
        b->prev_y = b->y;

        b->x += b->vx;
        b->y += b->vy;

        int16_t bx = b->x >> 4;
        int16_t by = b->y >> 4;

        /* Mark old position dirty (to erase) */
        tb_invalidate_ball(b->prev_x >> 4, b->prev_y >> 4, b->r);
        /* Mark new position dirty (to draw) */
        tb_invalidate_ball(bx, by, b->r);

        /* Check if ball flew off screen */
        if (bx < -b->r * 3 || bx > TB_AREA_W + b->r * 3 ||
            by < -b->r * 3 || by > TB_AREA_H + b->r * 3) {
            b->active = false;
            s_tb.hp -= TB_HP_LOSS_MISS;
            s_tb.hud_dirty = true;
            if (s_tb.hp <= 0) {
                s_tb.hp = 0;
                s_tb.state = TB_STATE_GAMEOVER;
                tb_update_gameover_text();
                s_tb.need_full_redraw = true;
            }
            tb_update_hp_text();
        }
    }

    /* Mark HUD dirty if FPS or score/HP changed */
    if (s_tb.fps_dirty || s_tb.hud_dirty) {
        tb_invalidate_hud();
        s_tb.fps_dirty = false;
        s_tb.hud_dirty = false;
    }
}

static void tb_on_press(int16_t x, int16_t y)
{
    (void)x;
    (void)y;

    if (s_tb.state == TB_STATE_IDLE) {
        tb_start_game();
        s_tb.need_full_redraw = true;
        return;
    }

    if (s_tb.state == TB_STATE_GAMEOVER) {
        tb_start_game();
        s_tb.need_full_redraw = true;
        return;
    }

    /* TB_STATE_PLAYING: check ball hits */
    int16_t gx = x - TB_AREA_X;
    int16_t gy = y - TB_AREA_Y;

    for (int i = 0; i < TB_MAX_BALLS; i++) {
        tb_ball_t *b = &s_tb.balls[i];
        if (!b->active) continue;

        int16_t bx = b->x >> 4;
        int16_t by = b->y >> 4;
        int16_t dx = gx - bx;
        int16_t dy = gy - by;
        int32_t dist_sq = (int32_t)dx * dx + (int32_t)dy * dy;
        int32_t r_sq = (int32_t)b->r * b->r;

        if (dist_sq <= r_sq) {
            /* Hit! Mark ball area dirty before deactivating */
            tb_invalidate_ball(bx, by, b->r);
            b->active = false;
            s_tb.score += TB_SCORE_PER_HIT;
            tb_update_score_text();
            s_tb.hud_dirty = true;
            break; /* Only hit one ball per tap */
        }
    }
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void tb_draw_ball_at(int16_t bx, int16_t by, int16_t r, ui_color_t color)
{
    ui_draw_fill_circle(bx, by, r, color);
    ui_draw_fill_circle(bx - r / 4, by - r / 4, r / 3, UI_COLOR_WHITE);
}

static void tb_draw_balls_in_rect(const ui_rect_t *clip)
{
    for (int i = 0; i < TB_MAX_BALLS; i++) {
        tb_ball_t *b = &s_tb.balls[i];
        if (!b->active) continue;
        int16_t bx = TB_AREA_X + (b->x >> 4);
        int16_t by = TB_AREA_Y + (b->y >> 4);
        int16_t margin = b->r + b->r / 3 + 2;
        ui_rect_t ball_rect = {bx - margin, by - margin, margin * 2, margin * 2};
        if (tb_rects_overlap(&ball_rect, clip)) {
            tb_draw_ball_at(bx, by, b->r, b->color);
        }
    }
}

static void tb_draw_hud(void)
{
    /* HUD background bar */
    ui_rect_t hud = {TB_AREA_X, TB_AREA_Y + TB_AREA_H - 32, TB_AREA_W, 32};
    ui_draw_fill_rect(&hud, UI_HEX(0xE8E8E8));
    ui_draw_hline(TB_AREA_X, TB_AREA_Y + TB_AREA_H - 32, TB_AREA_W, UI_HEX(0xD0D0D0));

    /* Score (left) */
    ui_draw_text(TB_AREA_X + 12, TB_AREA_Y + TB_AREA_H - 26,
                 s_tb.buf_score, &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);

    /* HP (center) */
    int16_t hp_w = ui_text_width(s_tb.buf_hp, &font_montserrat_12);
    ui_draw_text(TB_AREA_X + TB_AREA_W / 2 - hp_w / 2, TB_AREA_Y + TB_AREA_H - 26,
                 s_tb.buf_hp, &font_montserrat_12,
                 s_tb.hp <= 3 ? UI_COLOR_DANGER : UI_COLOR_TEXT_PRIMARY);

    /* FPS (right) */
    int16_t fps_w = ui_text_width(s_tb.buf_fps, &font_montserrat_12);
    ui_draw_text(TB_AREA_X + TB_AREA_W - fps_w - 12, TB_AREA_Y + TB_AREA_H - 26,
                 s_tb.buf_fps, &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
}

static void tb_draw_idle_screen(void)
{
    /* Decorative background circles */
    ui_draw_fill_circle(TB_AREA_W / 4, TB_AREA_Y + TB_AREA_H / 3, 30, UI_HEX(0xE8F6F3));
    ui_draw_fill_circle(TB_AREA_W * 3 / 4, TB_AREA_Y + TB_AREA_H * 2 / 3, 40, UI_HEX(0xF5E8F0));
    ui_draw_fill_circle(TB_AREA_W / 2, TB_AREA_Y + TB_AREA_H / 2 + 40, 25, UI_HEX(0xF0F5E8));

    /* Title */
    ui_rect_t title_rect = {TB_AREA_X, TB_AREA_Y + TB_AREA_H / 4 - 20, TB_AREA_W, 36};
    ui_draw_text_in_rect(&title_rect, "Touch Ball", &font_montserrat_16,
                         UI_COLOR_PRIMARY, 1);

    /* Instruction */
    ui_rect_t inst_rect = {TB_AREA_X, TB_AREA_Y + TB_AREA_H / 4 + 24, TB_AREA_W, 24};
    ui_draw_text_in_rect(&inst_rect, "Tap the flying balls before they escape!",
                         &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    /* Start hint */
    ui_rect_t hint_rect = {TB_AREA_X, TB_AREA_Y + TB_AREA_H / 2 + 20, TB_AREA_W, 24};
    ui_draw_text_in_rect(&hint_rect, "Tap anywhere to start",
                         &font_montserrat_12, UI_COLOR_ACCENT, 1);
}

static void tb_draw_gameover(void)
{
    /* Dark overlay */
    ui_rect_t overlay = {TB_AREA_X, TB_AREA_Y, TB_AREA_W, TB_AREA_H};
    ui_draw_fill_rect(&overlay, UI_HEX(0x333333));

    /* "GAME OVER" */
    ui_rect_t go_rect = {TB_AREA_X, TB_AREA_Y + TB_AREA_H / 2 - 50, TB_AREA_W, 30};
    ui_draw_text_in_rect(&go_rect, "GAME OVER", &font_montserrat_16,
                         UI_COLOR_ACCENT, 1);

    /* Score */
    ui_rect_t score_rect = {TB_AREA_X, TB_AREA_Y + TB_AREA_H / 2 - 12, TB_AREA_W, 24};
    ui_draw_text_in_rect(&score_rect, s_tb.buf_gameover, &font_montserrat_12,
                         UI_COLOR_WHITE, 1);

    /* Hint */
    ui_rect_t hint_rect = {TB_AREA_X, TB_AREA_Y + TB_AREA_H / 2 + 20, TB_AREA_W, 24};
    ui_draw_text_in_rect(&hint_rect, "Tap to restart", &font_montserrat_12,
                         UI_COLOR_SECONDARY, 1);
}

static void tb_draw_game_area(const ui_rect_t *clip)
{
    switch (s_tb.state) {
        case TB_STATE_IDLE:
            tb_draw_idle_screen();
            break;

        case TB_STATE_PLAYING:
            /* Erase dirty area with background, then draw balls on top */
            ui_draw_fill_rect(clip, UI_COLOR_BG_MAIN);
            tb_draw_balls_in_rect(clip);
            tb_draw_hud();
            break;

        case TB_STATE_GAMEOVER:
            ui_draw_fill_rect(clip, UI_COLOR_BG_MAIN);
            tb_draw_balls_in_rect(clip);
            tb_draw_hud();
            tb_draw_gameover();
            break;
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void tb_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_CLICK) {
        tb_on_press(e->pos.x, e->pos.y);
    }
}

static void tb_game_enter(ui_page_t *page)
{
    (void)page;
    tb_reset();
    s_tb.need_full_redraw = true;
}

static void tb_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Game logic update (marks dirty regions via ui_page_invalidate) */
    tb_update();

    /* Full redraw on state change */
    if (s_tb.need_full_redraw) {
        s_tb.need_full_redraw = false;

        /* Title bar */
        ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
        ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

        /* Draw widgets (back button, title label) on title bar */
        ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
        for (uint16_t j = 0; j < page->widget_count; j++) {
            if (page->widgets[j]) {
                ui_widget_draw(page->widgets[j], &full);
            }
        }

        /* Game area background */
        ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                          UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
        ui_draw_fill_rect(&area, UI_COLOR_BG_MAIN);

        tb_draw_game_area(&area);
        ui_page_clear_dirty();
        return;
    }

    /* No dirty regions? Nothing to redraw */
    const ui_dirty_list_t *dl = ui_page_get_dirty_list();
    if (dl->count == 0) return;

    /* For IDLE state, no animation needed - skip partial redraw */
    if (s_tb.state == TB_STATE_IDLE) {
        ui_page_clear_dirty();
        return;
    }

    /* Partial redraw: process each dirty region completely (erase + redraw)
     * in one pass to minimize the gap between erase and redraw.
     * This prevents the LCD from scanning a region after erase but
     * before the ball is redrawn, which causes flickering. */
    ui_rect_t game_area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                           UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_rect_t hud = {TB_AREA_X, TB_AREA_Y + TB_AREA_H - 32, TB_AREA_W, 32};
    bool hud_needs_redraw = false;

    for (uint16_t i = 0; i < dl->count; i++) {
        const ui_rect_t *d = &dl->regions[i];
        if (d->y + d->h <= APP_TITLE_BAR_H) continue;

        ui_rect_t clip = *d;
        if (clip.y < APP_TITLE_BAR_H) {
            clip.h -= (APP_TITLE_BAR_H - clip.y);
            clip.y = APP_TITLE_BAR_H;
        }
        if (clip.w <= 0 || clip.h <= 0) continue;

        /* Erase background within dirty region */
        ui_render_set_clip(&clip);
        ui_draw_fill_rect(&clip, UI_COLOR_BG_MAIN);

        /* Redraw ball with game-area clip (so ball draws completely,
         * not clipped to the dirty region edge) */
        ui_render_set_clip(&game_area);
        tb_draw_balls_in_rect(&clip);

        /* Check if this dirty region overlaps HUD - defer HUD redraw
         * to after the loop to avoid erasing+redrawing HUD multiple
         * times per frame (which causes flickering) */
        if (tb_rects_overlap(&clip, &hud)) {
            hud_needs_redraw = true;
        }

        /* Redraw gameover overlay if needed */
        if (s_tb.state == TB_STATE_GAMEOVER) {
            tb_draw_gameover();
        }
    }

    /* Single HUD redraw at the end: erase + redraw once per frame */
    if (hud_needs_redraw) {
        ui_render_set_clip(&hud);
        ui_draw_fill_rect(&hud, UI_HEX(0xE8E8E8));
        ui_render_set_clip(&game_area);
        tb_draw_hud();
    }

    /* Clear dirty list after processing */
    ui_page_clear_dirty();
    ui_render_reset_clip();
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_touchball_init(void)
{
    ui_app_page_init(&s_game_touchball, "Touch Ball", 0x205);

    /* UI_PAGE_FLAG_GAME: enables continuous frame updates for game pages.
     * Our draw callback manages dirty regions internally - it only redraws
     * what changed, not the full screen. */
    s_game_touchball.page.flags |= UI_PAGE_FLAG_GAME;

    /* Create full-screen touch area widget to capture game-area presses */
    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = tb_touch_event;

    /* Widget order: touch_area first (lowest z), then title, then back button.
     * UI_Tick iterates widgets from highest index downward for hit-testing,
     * so back button (index 2) and title (index 1) are checked before
     * touch_area (index 0). This ensures the back button works. */
    s_tb_widgets[0] = &s_touch_area;
    s_tb_widgets[1] = (ui_widget_t *)&s_game_touchball.lbl_title;
    s_tb_widgets[2] = (ui_widget_t *)&s_game_touchball.btn_back;

    ui_page_set_widgets(&s_game_touchball.page, s_tb_widgets, 3);
    ui_page_set_callbacks(&s_game_touchball.page, tb_game_enter, NULL,
                          tb_game_draw, NULL);
    ui_page_register(&s_game_touchball.page);
}

ui_page_t *game_touchball_get_page(void)
{
    return &s_game_touchball.page;
}
