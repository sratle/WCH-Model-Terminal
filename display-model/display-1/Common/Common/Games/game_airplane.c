/********************************** (C) COPYRIGHT *******************************
* File Name          : game_airplane.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/19
* Description        : Airplane shooting game.
*                      Touch to move player plane. Shoot down enemy planes.
*                      3-minute time limit. Dodge enemy bullets for points.
********************************************************************************/
#include "game_airplane.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Game Configuration
 *=============================================================================*/

#define AP_MAX_ENEMIES      5
#define AP_MAX_P_BULLETS    20
#define AP_MAX_E_BULLETS    50

#define AP_PLAYER_HP        100
#define AP_ENEMY_HP         50
#define AP_ENEMY_HP_PHASE2  60
#define AP_BULLET_DAMAGE    10
#define AP_SCORE_DODGE      1
#define AP_SCORE_KILL       50
#define AP_SCORE_ESCAPE     -100  /* Enemy escaped off screen */
#define AP_TIME_LIMIT_SEC   180

#define AP_SPAWN_INTERVAL   80
#define AP_PLAYER_FIRE_INTERVAL  5
#define AP_ENEMY_FIRE_INTERVAL   60
#define AP_ENEMY_FIRE_INTERVAL_P2 45  /* Phase 2: faster fire */

#define AP_PLAYER_SPEED     16
#define AP_P_BULLET_SPEED   8
#define AP_E_BULLET_SPEED   4
#define AP_E_BULLET_SPEED_P2 6  /* Phase 2: faster bullets */
#define AP_ENEMY_SPEED      1

#define AP_PHASE2_SCORE     1000  /* Score threshold for phase 2 */

#define AP_AREA_X           0
#define AP_AREA_Y           APP_TITLE_BAR_H
#define AP_AREA_W           UI_SCREEN_WIDTH
#define AP_AREA_H           (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H)
#define AP_HUD_H            28

/* Plane sizes */
#define AP_PLAYER_W         35
#define AP_PLAYER_H         35
#define AP_ENEMY_W          26
#define AP_ENEMY_H          26
#define AP_BULLET_W         4
#define AP_BULLET_H         8
#define AP_HP_BAR_W         22
#define AP_HP_BAR_H         3

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    AP_STATE_IDLE,
    AP_STATE_PLAYING,
    AP_STATE_GAMEOVER,
} ap_state_t;

typedef struct {
    int16_t x, y;           /* Center position (game-area relative) */
    int16_t hp;
    int16_t fire_timer;
    bool active;
    int16_t prev_x, prev_y; /* Previous frame position for dirty tracking */
} ap_enemy_t;

typedef struct {
    int16_t x, y;           /* Center position (game-area relative) */
    int16_t dx;             /* Horizontal speed (0 = straight, +/- for diagonal) */
    int16_t speed;          /* Vertical speed */
    bool active;
    int16_t prev_x, prev_y;
} ap_bullet_t;

typedef struct {
    ap_state_t state;
    int score;
    int hp;
    int frame_count;
    int spawn_timer;
    int player_fire_timer;
    uint32_t game_start_ms;
    int time_remaining;
    int frames_remaining;  /* Frame-based timer to avoid UCYCLE overflow */

    /* Player */
    int16_t player_x, player_y;
    int16_t player_prev_x, player_prev_y;
    int16_t target_x, target_y;

    /* Entities */
    ap_enemy_t enemies[AP_MAX_ENEMIES];
    ap_bullet_t p_bullets[AP_MAX_P_BULLETS];
    ap_bullet_t e_bullets[AP_MAX_E_BULLETS];

    /* State */
    bool hud_dirty;
    int16_t dmg_show_timer;  /* Frames remaining to show -10 */
    int16_t dmg_show_x, dmg_show_y; /* Position of -10 text */

    /* Text buffers */
    char buf_time[16];
    char buf_hp[12];
    char buf_score[16];
    char buf_gameover[32];
    char buf_best[16];
    int best;
} ap_game_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_airplane;
static ui_widget_t s_touch_area;
static ui_widget_t *s_ap_widgets[3];
static ap_game_t s_ap;
static bool s_ap_touch_active = false;  /* 触摸优先标志：触摸按下时为true */

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void ap_itoa(int val, char *out)
{
    char tmp[12];
    int i = 0;
    bool neg = val < 0;
    if (neg) val = -val;
    if (val == 0) { tmp[i++] = '0'; }
    else { while (val > 0) { tmp[i++] = '0' + (val % 10); val /= 10; } }
    int j = 0;
    if (neg) out[j++] = '-';
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
}

static void ap_strcpy(char *dst, const char *src)
{
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void ap_strcat(char *dst, const char *src)
{
    while (*dst) dst++;
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static uint32_t s_rng = 0xBEEFu;
static uint32_t ap_rng(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int32_t ap_rng_range(int32_t min, int32_t max)
{
    if (max <= min) return min;
    return min + (int32_t)(ap_rng() % (uint32_t)(max - min + 1));
}

static bool ap_rects_overlap(const ui_rect_t *a, const ui_rect_t *b)
{
    return (a->x < b->x + b->w && a->x + a->w > b->x &&
            a->y < b->y + b->h && a->y + a->h > b->y);
}

/* Check if two center-based rects overlap */
static bool ap_centers_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                               int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return (ax - aw/2 < bx + bw/2 && ax + aw/2 > bx - bw/2 &&
            ay - ah/2 < by + bh/2 && ay + ah/2 > by - bh/2);
}

/*=============================================================================
 *  Dirty Region Helpers
 *=============================================================================*/

/* Invalidate a rect in screen coordinates */
static void ap_inv(int16_t x, int16_t y, int16_t w, int16_t h)
{
    ui_rect_t r = {x, y, w, h};
    ui_page_invalidate(&r);
}

/* Invalidate player area (old + new position) */
static void ap_inv_player(void)
{
    int16_t m = 2; /* margin */
    /* Previous position */
    ap_inv(AP_AREA_X + s_ap.player_prev_x - AP_PLAYER_W/2 - m,
           AP_AREA_Y + s_ap.player_prev_y - AP_PLAYER_H/2 - m,
           AP_PLAYER_W + m*2, AP_PLAYER_H + m*2);
    /* Current position */
    ap_inv(AP_AREA_X + s_ap.player_x - AP_PLAYER_W/2 - m,
           AP_AREA_Y + s_ap.player_y - AP_PLAYER_H/2 - m,
           AP_PLAYER_W + m*2, AP_PLAYER_H + m*2);
}

/* Invalidate enemy area including HP bar (old + new position) */
static void ap_inv_enemy(const ap_enemy_t *e)
{
    int16_t m = 2;
    int16_t total_h = AP_ENEMY_H + AP_HP_BAR_H + 3; /* body + gap + hp bar */
    /* Previous */
    ap_inv(AP_AREA_X + e->prev_x - AP_ENEMY_W/2 - m,
           AP_AREA_Y + e->prev_y - AP_ENEMY_H/2 - AP_HP_BAR_H - 3 - m,
           AP_ENEMY_W + m*2, total_h + m*2);
    /* Current */
    ap_inv(AP_AREA_X + e->x - AP_ENEMY_W/2 - m,
           AP_AREA_Y + e->y - AP_ENEMY_H/2 - AP_HP_BAR_H - 3 - m,
           AP_ENEMY_W + m*2, total_h + m*2);
}

/* Invalidate bullet area (old + new position) */
static void ap_inv_bullet(int16_t cx, int16_t cy, int16_t prev_cx, int16_t prev_cy)
{
    int16_t m = 1;
    ap_inv(AP_AREA_X + prev_cx - AP_BULLET_W/2 - m,
           AP_AREA_Y + prev_cy - AP_BULLET_H/2 - m,
           AP_BULLET_W + m*2, AP_BULLET_H + m*2);
    ap_inv(AP_AREA_X + cx - AP_BULLET_W/2 - m,
           AP_AREA_Y + cy - AP_BULLET_H/2 - m,
           AP_BULLET_W + m*2, AP_BULLET_H + m*2);
}

/* Invalidate HUD bar */
static void ap_inv_hud(void)
{
    ap_inv(AP_AREA_X, AP_AREA_Y, AP_AREA_W, AP_HUD_H);
}

/*=============================================================================
 *  HUD Text
 *=============================================================================*/

static void ap_update_time_text(void)
{
    int min = s_ap.time_remaining / 60;
    int sec = s_ap.time_remaining % 60;
    ap_strcpy(s_ap.buf_time, "");
    char num[4];
    ap_itoa(min, num);
    if (min < 10) ap_strcat(s_ap.buf_time, "0");
    ap_strcat(s_ap.buf_time, num);
    ap_strcat(s_ap.buf_time, ":");
    ap_itoa(sec, num);
    if (sec < 10) ap_strcat(s_ap.buf_time, "0");
    ap_strcat(s_ap.buf_time, num);
}

static void ap_update_hp_text(void)
{
    ap_strcpy(s_ap.buf_hp, "HP:");
    char num[6];
    ap_itoa(s_ap.hp, num);
    ap_strcat(s_ap.buf_hp, num);
}

static void ap_update_score_text(void)
{
    ap_strcpy(s_ap.buf_score, "Score:");
    char num[8];
    ap_itoa(s_ap.score, num);
    ap_strcat(s_ap.buf_score, num);
}

static void ap_update_gameover_text(void)
{
    ap_strcpy(s_ap.buf_gameover, "Score: ");
    char num[8];
    ap_itoa(s_ap.score, num);
    ap_strcat(s_ap.buf_gameover, num);
    /* Update best text buffer */
    ap_strcpy(s_ap.buf_best, "Best: ");
    ap_itoa(s_ap.best, num);
    ap_strcat(s_ap.buf_best, num);
}

/*=============================================================================
 *  Best Score Persistence (via CLI passthrough to Core appcfg)
 *=============================================================================*/

static void ap_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    if (!tag || strcmp(tag, "appcfg") != 0) return;
    if (buf[0] < '0' || buf[0] > '9') return;
    s_ap.best = atoi(buf);
    ui_page_invalidate_all();
}

static const uart_cli_cb_t s_ap_cli_cb = {
    .on_cli_complete = ap_on_cli_complete,
};

static void ap_load_best(void)
{
    UART_SetCLICallbacks(&s_ap_cli_cb);
    UART_SendCLI("appcfg get game_airplane highscore");
}

static void ap_save_best(void)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "appcfg set game_airplane highscore %d", s_ap.best);
    UART_SendCLI(cmd);
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

static void ap_reset(void)
{
    int saved_best = s_ap.best;
    memset(&s_ap, 0, sizeof(s_ap));
    s_ap.best = saved_best;
    s_ap.state = AP_STATE_IDLE;
    s_ap.hp = AP_PLAYER_HP;
    s_ap.player_x = AP_AREA_W / 2;
    s_ap.player_y = AP_AREA_H - 60;
    s_ap.player_prev_x = s_ap.player_x;
    s_ap.player_prev_y = s_ap.player_y;
    s_ap.target_x = s_ap.player_x;
    s_ap.target_y = s_ap.player_y;
    s_ap.time_remaining = AP_TIME_LIMIT_SEC;
    ap_update_time_text();
    ap_update_hp_text();
    ap_update_score_text();
}

static void ap_start_game(void)
{
    ap_game_t *g = &s_ap;
    g->state = AP_STATE_PLAYING;
    g->score = 0;
    g->hp = AP_PLAYER_HP;
    g->frame_count = 0;
    g->spawn_timer = AP_SPAWN_INTERVAL / 2;
    g->player_fire_timer = 0;
    g->game_start_ms = ui_get_real_ms();
    g->time_remaining = AP_TIME_LIMIT_SEC;
    g->frames_remaining = AP_TIME_LIMIT_SEC * 25; /* 25 FPS target */
    g->player_x = AP_AREA_W / 2;
    g->player_y = AP_AREA_H - 60;
    g->player_prev_x = g->player_x;
    g->player_prev_y = g->player_y;
    g->target_x = g->player_x;
    g->target_y = g->player_y;
    for (int i = 0; i < AP_MAX_ENEMIES; i++) g->enemies[i].active = false;
    for (int i = 0; i < AP_MAX_P_BULLETS; i++) g->p_bullets[i].active = false;
    for (int i = 0; i < AP_MAX_E_BULLETS; i++) g->e_bullets[i].active = false;
    ap_update_time_text();
    ap_update_hp_text();
    ap_update_score_text();
}

static void ap_spawn_enemy(void)
{
    int slot = -1;
    for (int i = 0; i < AP_MAX_ENEMIES; i++) {
        if (!s_ap.enemies[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    ap_enemy_t *e = &s_ap.enemies[slot];
    int16_t ex = ap_rng_range(AP_ENEMY_W, AP_AREA_W - AP_ENEMY_W);

    for (int tries = 0; tries < 10; tries++) {
        bool overlap = false;
        for (int i = 0; i < AP_MAX_ENEMIES; i++) {
            if (!s_ap.enemies[i].active) continue;
            if (ap_centers_overlap(ex, AP_ENEMY_H, AP_ENEMY_W, AP_ENEMY_H,
                                   s_ap.enemies[i].x, s_ap.enemies[i].y,
                                   AP_ENEMY_W, AP_ENEMY_H)) {
                overlap = true;
                break;
            }
        }
        if (!overlap) break;
        ex = ap_rng_range(AP_ENEMY_W, AP_AREA_W - AP_ENEMY_W);
    }

    e->x = ex;
    e->y = AP_HUD_H + AP_ENEMY_H;  /* Spawn below HUD */
    e->prev_x = e->x;
    e->prev_y = e->y;
    e->hp = (s_ap.score >= AP_PHASE2_SCORE) ? AP_ENEMY_HP_PHASE2 : AP_ENEMY_HP;
    e->fire_timer = ap_rng_range(20, (s_ap.score >= AP_PHASE2_SCORE)
                                       ? AP_ENEMY_FIRE_INTERVAL_P2
                                       : AP_ENEMY_FIRE_INTERVAL);
    e->active = true;
}

static void ap_fire_player(void)
{
    int slot = -1;
    for (int i = 0; i < AP_MAX_P_BULLETS; i++) {
        if (!s_ap.p_bullets[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    ap_bullet_t *b = &s_ap.p_bullets[slot];
    b->x = s_ap.player_x;
    b->y = s_ap.player_y - AP_PLAYER_H / 2;
    b->dx = 0;
    b->speed = AP_P_BULLET_SPEED;
    b->prev_x = b->x;
    b->prev_y = b->y;
    b->active = true;
}

static void ap_fire_enemy(ap_enemy_t *e)
{
    bool phase2 = s_ap.score >= AP_PHASE2_SCORE;
    int16_t e_speed = phase2 ? AP_E_BULLET_SPEED_P2 : AP_E_BULLET_SPEED;

    /* Straight bullet */
    int slot = -1;
    for (int i = 0; i < AP_MAX_E_BULLETS; i++) {
        if (!s_ap.e_bullets[i].active) { slot = i; break; }
    }
    if (slot < 0) return;
    ap_bullet_t *b = &s_ap.e_bullets[slot];
    b->x = e->x;
    b->y = e->y + AP_ENEMY_H / 2;
    b->dx = 0;
    b->speed = e_speed;
    b->prev_x = b->x;
    b->prev_y = b->y;
    b->active = true;

    if (!phase2) return;

    /* Phase 2: additional bullets at ±30 degrees */
    /* At 30°, dx/dy ≈ tan(30°) ≈ 0.577, so dx ≈ e_speed * 0.577 */
    int16_t diag_dx = (int16_t)(e_speed * 3 / 5);  /* ≈ 0.6 * speed */

    /* Left bullet */
    slot = -1;
    for (int i = 0; i < AP_MAX_E_BULLETS; i++) {
        if (!s_ap.e_bullets[i].active) { slot = i; break; }
    }
    if (slot >= 0) {
        b = &s_ap.e_bullets[slot];
        b->x = e->x;
        b->y = e->y + AP_ENEMY_H / 2;
        b->dx = -diag_dx;
        b->speed = e_speed;
        b->prev_x = b->x;
        b->prev_y = b->y;
        b->active = true;
    }

    /* Right bullet */
    slot = -1;
    for (int i = 0; i < AP_MAX_E_BULLETS; i++) {
        if (!s_ap.e_bullets[i].active) { slot = i; break; }
    }
    if (slot >= 0) {
        b = &s_ap.e_bullets[slot];
        b->x = e->x;
        b->y = e->y + AP_ENEMY_H / 2;
        b->dx = diag_dx;
        b->speed = e_speed;
        b->prev_x = b->x;
        b->prev_y = b->y;
        b->active = true;
    }
}

static void ap_end_game(void)
{
    s_ap.state = AP_STATE_GAMEOVER;
    if (s_ap.score > s_ap.best) {
        s_ap.best = s_ap.score;
        ap_save_best();
    }
    ap_update_gameover_text();
    ui_page_invalidate_all();
}

static void ap_update(void)
{
    if (s_ap.state != AP_STATE_PLAYING) return;

    s_ap.frame_count++;

    /* Update timer using frame counter (avoids UCYCLE 32-bit overflow at ~43s) */
    s_ap.frames_remaining--;
    if (s_ap.frames_remaining < 0) s_ap.frames_remaining = 0;
    int new_time = s_ap.frames_remaining / 25; /* Convert frames to seconds */
    if (new_time != s_ap.time_remaining) {
        s_ap.time_remaining = new_time;
        ap_update_time_text();
        s_ap.hud_dirty = true;
    }
    if (s_ap.frames_remaining <= 0) {
        ap_end_game();
        return;
    }

    /* Move player toward target */
    s_ap.player_prev_x = s_ap.player_x;
    s_ap.player_prev_y = s_ap.player_y;
    {
        int16_t dx = s_ap.target_x - s_ap.player_x;
        int16_t dy = s_ap.target_y - s_ap.player_y;
        int16_t dist_sq = dx * dx + dy * dy;
        if (dist_sq > 4) {
            if (dist_sq > (int16_t)(AP_PLAYER_SPEED * AP_PLAYER_SPEED)) {
                int32_t adx = dx > 0 ? dx : -dx;
                int32_t ady = dy > 0 ? dy : -dy;
                int32_t idist = (adx > ady) ? adx + (ady * 3 / 8) : ady + (adx * 3 / 8);
                if (idist > 0) {
                    s_ap.player_x += (int16_t)((int32_t)dx * AP_PLAYER_SPEED / idist);
                    s_ap.player_y += (int16_t)((int32_t)dy * AP_PLAYER_SPEED / idist);
                }
            } else {
                s_ap.player_x = s_ap.target_x;
                s_ap.player_y = s_ap.target_y;
            }
        }
    }
    if (s_ap.player_x < AP_PLAYER_W/2) s_ap.player_x = AP_PLAYER_W/2;
    if (s_ap.player_x > AP_AREA_W - AP_PLAYER_W/2) s_ap.player_x = AP_AREA_W - AP_PLAYER_W/2;
    if (s_ap.player_y < AP_HUD_H + AP_PLAYER_H/2) s_ap.player_y = AP_HUD_H + AP_PLAYER_H/2;
    if (s_ap.player_y > AP_AREA_H - AP_PLAYER_H/2) s_ap.player_y = AP_AREA_H - AP_PLAYER_H/2;

    ap_inv_player();

    /* Player auto-fire */
    s_ap.player_fire_timer--;
    if (s_ap.player_fire_timer <= 0) {
        ap_fire_player();
        s_ap.player_fire_timer = AP_PLAYER_FIRE_INTERVAL;
    }

    /* Spawn enemies */
    s_ap.spawn_timer--;
    if (s_ap.spawn_timer <= 0) {
        ap_spawn_enemy();
        s_ap.spawn_timer = AP_SPAWN_INTERVAL;
    }

    /* Update enemies */
    for (int i = 0; i < AP_MAX_ENEMIES; i++) {
        ap_enemy_t *e = &s_ap.enemies[i];
        if (!e->active) continue;

        e->prev_x = e->x;
        e->prev_y = e->y;
        e->y += AP_ENEMY_SPEED;

        e->fire_timer--;
        if (e->fire_timer <= 0) {
            ap_fire_enemy(e);
            e->fire_timer = (s_ap.score >= AP_PHASE2_SCORE)
                            ? AP_ENEMY_FIRE_INTERVAL_P2
                            : AP_ENEMY_FIRE_INTERVAL;
        }

        if (e->y > AP_AREA_H + AP_ENEMY_H) {
            e->active = false;
            s_ap.score += AP_SCORE_ESCAPE;
            if (s_ap.score < 0) s_ap.score = 0;
            ap_update_score_text();
            s_ap.hud_dirty = true;
        }

        ap_inv_enemy(e);
    }

    /* Update player bullets */
    for (int i = 0; i < AP_MAX_P_BULLETS; i++) {
        ap_bullet_t *b = &s_ap.p_bullets[i];
        if (!b->active) continue;
        b->prev_x = b->x;
        b->prev_y = b->y;
        b->y -= b->speed;
        if (b->y < AP_HUD_H - AP_BULLET_H) { /* Deactivate when entering HUD */
            b->active = false;
            ap_inv_bullet(b->prev_x, b->prev_y, b->prev_x, b->prev_y);
            continue;
        }
        ap_inv_bullet(b->x, b->y, b->prev_x, b->prev_y);

        for (int j = 0; j < AP_MAX_ENEMIES; j++) {
            ap_enemy_t *e = &s_ap.enemies[j];
            if (!e->active) continue;
            if (ap_centers_overlap(b->x, b->y, AP_BULLET_W, AP_BULLET_H,
                                   e->x, e->y, AP_ENEMY_W, AP_ENEMY_H)) {
                e->hp -= AP_BULLET_DAMAGE;
                b->active = false;
                if (e->hp <= 0) {
                    e->active = false;
                    s_ap.score += AP_SCORE_KILL;
                    ap_update_score_text();
                    s_ap.hud_dirty = true;
                }
                ap_inv_enemy(e);
                break;
            }
        }
    }

    /* Update enemy bullets */
    for (int i = 0; i < AP_MAX_E_BULLETS; i++) {
        ap_bullet_t *b = &s_ap.e_bullets[i];
        if (!b->active) continue;
        b->prev_x = b->x;
        b->prev_y = b->y;
        b->y += b->speed;
        b->x += b->dx;

        if (b->y > AP_AREA_H + AP_BULLET_H ||
            b->x < -AP_BULLET_W || b->x > AP_AREA_W + AP_BULLET_W) {
            b->active = false;
            s_ap.score += AP_SCORE_DODGE;
            ap_update_score_text();
            s_ap.hud_dirty = true;
            ap_inv_bullet(b->prev_x, b->prev_y, b->prev_x, b->prev_y);
            continue;
        }
        ap_inv_bullet(b->x, b->y, b->prev_x, b->prev_y);

        if (ap_centers_overlap(b->x, b->y, AP_BULLET_W, AP_BULLET_H,
                               s_ap.player_x, s_ap.player_y,
                               AP_PLAYER_W, AP_PLAYER_H)) {
            b->active = false;
            s_ap.hp -= AP_BULLET_DAMAGE;
            ap_update_hp_text();
            s_ap.hud_dirty = true;
            /* Show -10 damage indicator */
            s_ap.dmg_show_timer = 30; /* ~1 second at 30fps */
            s_ap.dmg_show_x = s_ap.player_x;
            s_ap.dmg_show_y = s_ap.player_y;
            if (s_ap.hp <= 0) {
                s_ap.hp = 0;
                ap_end_game();
                return;
            }
        }
    }

    if (s_ap.hud_dirty) {
        ap_inv_hud();
        s_ap.hud_dirty = false;
    }

    /* Damage indicator timer */
    if (s_ap.dmg_show_timer > 0) {
        s_ap.dmg_show_timer--;
        /* Invalidate area around the -10 text (generous size) */
        ap_inv(AP_AREA_X + s_ap.dmg_show_x - 24,
               AP_AREA_Y + s_ap.dmg_show_y - AP_PLAYER_H/2 - 24,
               48, 24);
    }
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void ap_draw_player(int16_t cx, int16_t cy)
{
    int16_t x = AP_AREA_X + cx;
    int16_t y = AP_AREA_Y + cy;
    ui_color_t body = UI_HEX(0x4A90D9);
    ui_color_t wing = UI_HEX(0x357ABD);
    ui_draw_fill_circle(x, y - 7, 6, body);
    ui_draw_fill_circle(x, y, 8, body);
    ui_draw_fill_circle(x - 9, y + 5, 5, wing);
    ui_draw_fill_circle(x + 9, y + 5, 5, wing);
    ui_draw_fill_circle(x, y - 15, 4, UI_HEX(0x5BA3E6));
    ui_draw_fill_circle(x, y + 12, 4, wing);
}

static void ap_draw_enemy(int16_t cx, int16_t cy, int16_t hp, int16_t max_hp)
{
    int16_t x = AP_AREA_X + cx;
    int16_t y = AP_AREA_Y + cy;
    ui_color_t body = UI_HEX(0xE74C3C);
    ui_color_t wing = UI_HEX(0xC0392B);
    ui_draw_fill_circle(x, y + 6, 5, body);
    ui_draw_fill_circle(x, y, 6, body);
    ui_draw_fill_circle(x - 7, y - 4, 4, wing);
    ui_draw_fill_circle(x + 7, y - 4, 4, wing);
    ui_draw_fill_circle(x, y + 12, 3, UI_HEX(0xF5B7B1));
    /* HP bar */
    int16_t bar_x = x - AP_HP_BAR_W / 2;
    int16_t bar_y = y - AP_ENEMY_H / 2 - AP_HP_BAR_H - 3;
    ui_rect_t bg = {bar_x, bar_y, AP_HP_BAR_W, AP_HP_BAR_H};
    ui_draw_fill_rect(&bg, UI_HEX(0xCCCCCC));
    int16_t fill_w = (int16_t)((int32_t)AP_HP_BAR_W * hp / max_hp);
    if (fill_w > 0) {
        ui_rect_t fill = {bar_x, bar_y, fill_w, AP_HP_BAR_H};
        ui_color_t hp_color = hp > max_hp / 2 ? UI_HEX(0x2ECC71) :
                              hp > max_hp / 4 ? UI_HEX(0xF39C12) : UI_HEX(0xE74C3C);
        ui_draw_fill_rect(&fill, hp_color);
    }
}

static void ap_draw_bullet(int16_t cx, int16_t cy, bool is_player)
{
    int16_t x = AP_AREA_X + cx;
    int16_t y = AP_AREA_Y + cy;
    ui_color_t color = is_player ? UI_HEX(0x5DADE2) : UI_HEX(0xF5B041);
    ui_rect_t r = {x - AP_BULLET_W/2, y - AP_BULLET_H/2, AP_BULLET_W, AP_BULLET_H};
    ui_draw_fill_rect(&r, color);
}

static void ap_draw_hud(void)
{
    ui_rect_t hud = {AP_AREA_X, AP_AREA_Y, AP_AREA_W, AP_HUD_H};
    ui_draw_fill_rect(&hud, UI_HEX(0x2C3E50));

    ui_draw_text(AP_AREA_X + 12, AP_AREA_Y + 7,
                 s_ap.buf_time, &font_montserrat_12, UI_COLOR_WHITE);

    int16_t hp_w = ui_text_width(s_ap.buf_hp, &font_montserrat_12);
    ui_draw_text(AP_AREA_X + AP_AREA_W/2 - hp_w/2, AP_AREA_Y + 7,
                 s_ap.buf_hp, &font_montserrat_12,
                 s_ap.hp <= 30 ? UI_HEX(0xE74C3C) : UI_COLOR_WHITE);

    /* Score + phase indicator */
    const char *phase = (s_ap.score >= AP_PHASE2_SCORE) ? " P2" : "";
    char buf[24];
    ap_strcpy(buf, s_ap.buf_score);
    ap_strcat(buf, phase);
    int16_t score_w = ui_text_width(buf, &font_montserrat_12);
    ui_draw_text(AP_AREA_X + AP_AREA_W - score_w - 12, AP_AREA_Y + 7,
                 buf, &font_montserrat_12,
                 s_ap.score >= AP_PHASE2_SCORE ? UI_HEX(0xE74C3C) : UI_HEX(0xF1C40F));
}

static void ap_draw_idle_screen(void)
{
    ui_draw_fill_circle(AP_AREA_W / 3, AP_AREA_Y + AP_AREA_H / 3, 35, UI_HEX(0xD6EAF8));
    ui_draw_fill_circle(AP_AREA_W * 2 / 3, AP_AREA_Y + AP_AREA_H * 2 / 3, 45, UI_HEX(0xFADBD8));

    ui_rect_t title_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 4 - 20, AP_AREA_W, 36};
    ui_draw_text_in_rect(&title_rect, "Airplane", &font_montserrat_16,
                         UI_COLOR_PRIMARY, 1);

    ui_rect_t inst_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 4 + 24, AP_AREA_W, 24};
    ui_draw_text_in_rect(&inst_rect, "Shoot down enemy planes! Dodge their bullets!",
                         &font_montserrat_12, UI_COLOR_TEXT_SECONDARY, 1);

    ui_rect_t hint_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 2 + 20, AP_AREA_W, 24};
    ui_draw_text_in_rect(&hint_rect, "Touch and drag to move, tap to start",
                         &font_montserrat_12, UI_COLOR_ACCENT, 1);
}

static void ap_draw_gameover(void)
{
    ui_rect_t overlay = {AP_AREA_X, AP_AREA_Y, AP_AREA_W, AP_AREA_H};
    ui_draw_fill_rect(&overlay, UI_HEX(0x333333));

    ui_rect_t go_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 2 - 60, AP_AREA_W, 30};
    ui_draw_text_in_rect(&go_rect, "GAME OVER", &font_montserrat_16,
                         UI_COLOR_ACCENT, 1);

    ui_rect_t score_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 2 - 22, AP_AREA_W, 24};
    ui_draw_text_in_rect(&score_rect, s_ap.buf_gameover, &font_montserrat_12,
                         UI_COLOR_WHITE, 1);

    /* Show best score */
    ui_rect_t best_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 2 - 4, AP_AREA_W, 24};
    ui_draw_text_in_rect(&best_rect, s_ap.buf_best, &font_montserrat_12,
                         UI_HEX(0xF1C40F), 1);

    /* Show remaining HP and time */
    char buf_info[32];
    ap_strcpy(buf_info, "HP: ");
    char num_hp[8];
    ap_itoa(s_ap.hp, num_hp);
    ap_strcat(buf_info, num_hp);
    ap_strcat(buf_info, "  Time: ");
    ap_strcat(buf_info, s_ap.buf_time);
    ui_rect_t info_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 2 + 8, AP_AREA_W, 24};
    ui_draw_text_in_rect(&info_rect, buf_info, &font_montserrat_12,
                         UI_COLOR_SECONDARY, 1);

    ui_rect_t hint_rect = {AP_AREA_X, AP_AREA_Y + AP_AREA_H / 2 + 38, AP_AREA_W, 24};
    ui_draw_text_in_rect(&hint_rect, "Tap to restart", &font_montserrat_12,
                         UI_COLOR_SECONDARY, 1);
}

/* Draw entities that overlap with the given clip rect */
static void ap_draw_entities_in_clip(const ui_rect_t *clip)
{
    /* Player bullets (skip if in HUD area) */
    for (int i = 0; i < AP_MAX_P_BULLETS; i++) {
        ap_bullet_t *b = &s_ap.p_bullets[i];
        if (!b->active) continue;
        if (b->y - AP_BULLET_H/2 < AP_HUD_H) continue; /* Don't draw in HUD */
        ui_rect_t br = {AP_AREA_X + b->x - AP_BULLET_W/2,
                        AP_AREA_Y + b->y - AP_BULLET_H/2,
                        AP_BULLET_W, AP_BULLET_H};
        if (ap_rects_overlap(&br, clip))
            ap_draw_bullet(b->x, b->y, true);
    }

    /* Enemy bullets (skip if in HUD area) */
    for (int i = 0; i < AP_MAX_E_BULLETS; i++) {
        ap_bullet_t *b = &s_ap.e_bullets[i];
        if (!b->active) continue;
        if (b->y - AP_BULLET_H/2 < AP_HUD_H) continue; /* Don't draw in HUD */
        ui_rect_t br = {AP_AREA_X + b->x - AP_BULLET_W/2,
                        AP_AREA_Y + b->y - AP_BULLET_H/2,
                        AP_BULLET_W, AP_BULLET_H};
        if (ap_rects_overlap(&br, clip))
            ap_draw_bullet(b->x, b->y, false);
    }

    /* Enemies */
    for (int i = 0; i < AP_MAX_ENEMIES; i++) {
        ap_enemy_t *e = &s_ap.enemies[i];
        if (!e->active) continue;
        ui_rect_t er = {AP_AREA_X + e->x - AP_ENEMY_W/2,
                        AP_AREA_Y + e->y - AP_ENEMY_H/2 - AP_HP_BAR_H - 3,
                        AP_ENEMY_W, AP_ENEMY_H + AP_HP_BAR_H + 3};
        if (ap_rects_overlap(&er, clip))
            ap_draw_enemy(e->x, e->y, e->hp,
                          e->hp > AP_ENEMY_HP ? AP_ENEMY_HP_PHASE2 : AP_ENEMY_HP);
    }

    /* Player */
    {
        ui_rect_t pr = {AP_AREA_X + s_ap.player_x - AP_PLAYER_W/2,
                        AP_AREA_Y + s_ap.player_y - AP_PLAYER_H/2,
                        AP_PLAYER_W, AP_PLAYER_H};
        if (ap_rects_overlap(&pr, clip))
            ap_draw_player(s_ap.player_x, s_ap.player_y);
    }

    /* Damage indicator (-10) */
    if (s_ap.dmg_show_timer > 0) {
        int16_t dx = AP_AREA_X + s_ap.dmg_show_x;
        int16_t dy = AP_AREA_Y + s_ap.dmg_show_y - AP_PLAYER_H/2 - 14;
        ui_rect_t dr = {dx - 24, dy - 10, 48, 24};
        if (ap_rects_overlap(&dr, clip))
            ui_draw_text(dx - 10, dy, "-10", &font_montserrat_12, UI_HEX(0xE74C3C));
    }
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void ap_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (s_ap.state == AP_STATE_IDLE) {
        if (e->type == UI_EVENT_CLICK) {
            ap_start_game();
            ui_page_invalidate_all();
        }
        return;
    }
    if (s_ap.state == AP_STATE_GAMEOVER) {
        if (e->type == UI_EVENT_CLICK) {
            ap_start_game();
            ui_page_invalidate_all();
        }
        return;
    }

    /* 触摸事件：优先级最高 */
    if (e->source == UI_INPUT_TOUCH) {
        if (e->type == UI_EVENT_DOWN || e->type == UI_EVENT_MOVE) {
            s_ap_touch_active = true;
            s_ap.target_x = e->pos.x - AP_AREA_X;
            s_ap.target_y = e->pos.y - AP_AREA_Y;
        } else if (e->type == UI_EVENT_UP) {
            s_ap_touch_active = false;
        }
        return;
    }

    /* 鼠标事件：仅当触摸未激活时，鼠标移动直接设置目标位置（无需按下左键） */
    if (e->source == UI_INPUT_MOUSE && !s_ap_touch_active) {
        if (e->type == UI_EVENT_MOVE) {
            s_ap.target_x = e->pos.x - AP_AREA_X;
            s_ap.target_y = e->pos.y - AP_AREA_Y;
        }
        return;
    }

    /* 键盘事件：方向键移动飞机，OK键开始/重新开始 */
    if (e->type == UI_EVENT_KEY_LEFT_ARROW) {
        s_ap.target_x -= 20;
        if (s_ap.target_x < 0) s_ap.target_x = 0;
        return;
    } else if (e->type == UI_EVENT_KEY_RIGHT_ARROW) {
        s_ap.target_x += 20;
        if (s_ap.target_x > AP_AREA_W) s_ap.target_x = AP_AREA_W;
        return;
    } else if (e->type == UI_EVENT_KEY_UP_ARROW) {
        s_ap.target_y -= 20;
        if (s_ap.target_y < 0) s_ap.target_y = 0;
        return;
    } else if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        s_ap.target_y += 20;
        if (s_ap.target_y > AP_AREA_H) s_ap.target_y = AP_AREA_H;
        return;
    } else if (e->type == UI_EVENT_KEY_OK) {
        if (s_ap.state == AP_STATE_IDLE || s_ap.state == AP_STATE_GAMEOVER) {
            ap_start_game();
            ui_page_invalidate_all();
        }
        return;
    }
}

static void ap_game_enter(ui_page_t *page)
{
    (void)page;
    s_ap_touch_active = false;
    ap_reset();
    ap_load_best();
    ui_page_invalidate_all();
}

/* Per-frame game logic update */
static void ap_game_update(ui_page_t *page)
{
    (void)page;
    ap_update();
}

/* Per-row game rendering (compositing renderer auto-clips to target row) */
static void ap_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;
    (void)dirty;

    /* Title bar background */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    /* Game area background */
    ui_rect_t area = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                      UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_draw_fill_rect(&area, UI_COLOR_BG_MAIN);

    if (s_ap.state == AP_STATE_IDLE) {
        ap_draw_idle_screen();
    } else {
        /* Draw all entities (primitives auto-clip to target row) */
        ap_draw_entities_in_clip(&area);
        ap_draw_hud();

        if (s_ap.state == AP_STATE_GAMEOVER) {
            ap_draw_gameover();
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_airplane_init(void)
{
    ui_app_page_init(&s_game_airplane, "Airplane", 0x201);

    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, UI_SCREEN_WIDTH,
                            UI_SCREEN_HEIGHT - APP_TITLE_BAR_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = ap_touch_event;

    s_ap_widgets[0] = &s_touch_area;
    s_ap_widgets[1] = (ui_widget_t *)&s_game_airplane.lbl_title;
    s_ap_widgets[2] = (ui_widget_t *)&s_game_airplane.btn_back;

    ui_page_set_widgets(&s_game_airplane.page, s_ap_widgets, 3);
    ui_page_set_callbacks(&s_game_airplane.page, ap_game_enter, NULL,
                          ap_game_draw, NULL);
    ui_page_set_update_cb(&s_game_airplane.page, ap_game_update);
    ui_page_register(&s_game_airplane.page);
}

ui_page_t *game_airplane_get_page(void)
{
    return &s_game_airplane.page;
}
