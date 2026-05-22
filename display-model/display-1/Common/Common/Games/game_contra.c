/********************************** (C) COPYRIGHT *******************************
* File Name          : game_contra.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/05/21
* Description        : Contra-like side-scrolling shooter game.
*                      Left D-pad for movement, right buttons for shoot/jump.
*                      One level, 5 lives.
********************************************************************************/
#include "game_contra.h"
#include "../UI/ui_app_common.h"
#include <string.h>

/*=============================================================================
 *  Game Configuration
 *=============================================================================*/

#define CT_MAX_BULLETS      30
#define CT_MAX_ENEMIES      20
#define CT_MAX_PLATFORMS    30
#define CT_MAX_PARTICLES    30

#define CT_LIVES            5
#define CT_PLAYER_W         14
#define CT_PLAYER_H         24
#define CT_PLAYER_CROUCH_H  14
#define CT_PLAYER_SPEED     3
#define CT_JUMP_VY          -12
#define CT_GRAVITY          1
#define CT_BULLET_SPEED     8
#define CT_E_BULLET_SPEED   3
#define CT_BULLET_W         6
#define CT_BULLET_H         3
#define CT_E_BULLET_W       4
#define CT_E_BULLET_H       4
#define CT_FIRE_INTERVAL    6       /* Frames between shots */
#define CT_INVINCIBLE_TIME  75      /* Frames of invincibility after hit */
#define CT_ENEMY_W          14
#define CT_ENEMY_H          20
#define CT_BOSS_W           40
#define CT_BOSS_H           40
#define CT_BOSS_HP          30

/* Level dimensions */
#define CT_LEVEL_WIDTH      5000
#define CT_GROUND_Y         380     /* Ground surface Y (game-area relative) */
#define CT_CAMERA_MARGIN    200     /* Camera follows player within this margin */

/* Game area */
#define CT_AREA_X           0
#define CT_AREA_Y           APP_TITLE_BAR_H
#define CT_AREA_W           UI_SCREEN_WIDTH
#define CT_AREA_H           (UI_SCREEN_HEIGHT - APP_TITLE_BAR_H)
#define CT_HUD_H            24

/* D-pad layout (bottom-left) */
#define CT_DPAD_BTN         52
#define CT_DPAD_GAP         4
#define CT_DPAD_LEFT_X      20
#define CT_DPAD_BOT_Y       (UI_SCREEN_HEIGHT - 20)
#define CT_DPAD_UP_Y        (CT_DPAD_BOT_Y - 2 * CT_DPAD_BTN - CT_DPAD_GAP)
#define CT_DPAD_MID_Y       (CT_DPAD_UP_Y + CT_DPAD_BTN + CT_DPAD_GAP)
#define CT_DPAD_UP_X        (CT_DPAD_LEFT_X + CT_DPAD_BTN + CT_DPAD_GAP)
#define CT_DPAD_LEFT_Y      CT_DPAD_MID_Y
#define CT_DPAD_DOWN_X      CT_DPAD_UP_X
#define CT_DPAD_DOWN_Y      CT_DPAD_MID_Y
#define CT_DPAD_RIGHT_X     (CT_DPAD_UP_X + CT_DPAD_BTN + CT_DPAD_GAP)
#define CT_DPAD_RIGHT_Y     CT_DPAD_MID_Y

/* Action buttons (bottom-right) */
#define CT_ACT_BTN_W        70
#define CT_ACT_BTN_H        70
#define CT_ACT_GAP          12
#define CT_ACT_RIGHT_X      (UI_SCREEN_WIDTH - CT_ACT_BTN_W - 20)
#define CT_ACT_JUMP_Y       (UI_SCREEN_HEIGHT - 2 * CT_ACT_BTN_H - CT_ACT_GAP - 20)
#define CT_ACT_SHOOT_Y      (CT_ACT_JUMP_Y + CT_ACT_BTN_H + CT_ACT_GAP)

/*=============================================================================
 *  Colors
 *=============================================================================*/

#define CT_BG_SKY           UI_HEX(0x1B2838)   /* Dark sky */
#define CT_BG_GROUND        UI_HEX(0x4A3728)   /* Ground */
#define CT_BG_GROUND_TOP    UI_HEX(0x5D8A3C)   /* Grass line */
#define CT_PLATFORM_COLOR   UI_HEX(0x6B5B3A)   /* Platform */
#define CT_PLATFORM_TOP     UI_HEX(0x7DA844)   /* Platform grass */
#define CT_PLAYER_COLOR     UI_HEX(0x2196F3)   /* Player body */
#define CT_PLAYER_HEAD      UI_HEX(0xFFCC80)   /* Player head */
#define CT_PLAYER_GUN       UI_HEX(0x757575)   /* Gun */
#define CT_ENEMY_COLOR      UI_HEX(0xF44336)   /* Enemy body */
#define CT_ENEMY_HEAD       UI_HEX(0xFFCC80)   /* Enemy head */
#define CT_BOSS_COLOR       UI_HEX(0x9C27B0)   /* Boss body */
#define CT_BOSS_HEAD        UI_HEX(0xFFCC80)   /* Boss head */
#define CT_BULLET_COLOR     UI_HEX(0xFFEB3B)   /* Player bullet */
#define CT_E_BULLET_COLOR   UI_HEX(0xFF5722)   /* Enemy bullet */
#define CT_HUD_BG           UI_HEX(0x000000)   /* HUD background */
#define CT_CTRL_BG          UI_HEX(0x444444)   /* Control button bg */
#define CT_CTRL_BG_PRESSED  UI_HEX(0x666666)   /* Control button pressed */
#define CT_CTRL_ALPHA       120                 /* Control overlay alpha */
#define CT_PARTICLE_COLOR   UI_HEX(0xFF9800)   /* Explosion particle */

/*=============================================================================
 *  Types
 *=============================================================================*/

typedef enum {
    CT_STATE_IDLE,
    CT_STATE_PLAYING,
    CT_STATE_DEAD,      /* Player death animation */
    CT_STATE_GAMEOVER,
    CT_STATE_WIN,
} ct_state_t;

typedef enum {
    CT_ENEMY_WALKER,    /* Walks on platform, shoots occasionally */
    CT_ENEMY_TURRET,    /* Stationary, shoots at player */
    CT_ENEMY_BOSS,      /* Boss at end of level */
} ct_enemy_type_t;

typedef struct {
    int16_t x, y;       /* World position (top-left) */
    int16_t w, h;       /* Size */
} ct_platform_t;

typedef struct {
    int16_t x, y;       /* World position (top-left) */
    int16_t vx, vy;     /* Velocity */
    bool active;
    bool from_player;   /* True = player bullet, False = enemy bullet */
    int16_t prev_x, prev_y; /* Previous frame position for dirty tracking */
} ct_bullet_t;

typedef struct {
    int16_t x, y;       /* World position (top-left) */
    int16_t vx;         /* Horizontal speed */
    ct_enemy_type_t type;
    int hp;
    int fire_timer;
    bool active;
    bool facing_right;  /* Direction enemy faces */
    int16_t patrol_min_x, patrol_max_x; /* Walker patrol range */
    int16_t prev_x, prev_y; /* Previous frame position for dirty tracking */
    int prev_hp;            /* Previous HP for boss bar dirty tracking */
} ct_enemy_t;

typedef struct {
    int16_t x, y;
    int16_t vx, vy;
    int life;           /* Frames remaining */
    bool active;
    int16_t prev_x, prev_y;
} ct_particle_t;

typedef struct {
    ct_state_t state;
    int frame_count;
    int lives;

    /* Player */
    int16_t px, py;     /* World position (top-left) */
    int16_t pvx, pvy;   /* Velocity */
    int16_t prev_px, prev_py; /* Previous frame position */
    bool on_ground;
    bool crouching;
    bool prev_crouching;
    bool facing_right;
    int invincible;      /* Frames of invincibility remaining */
    int fire_timer;
    bool shooting;       /* True while shoot button held */

    /* Input state */
    bool input_left, input_right, input_up, input_down;
    bool input_jump, input_shoot;
    bool prev_input_left, prev_input_right, prev_input_up, prev_input_down;
    bool prev_input_jump, prev_input_shoot;

    /* Camera */
    int16_t camera_x;
    int16_t prev_camera_x;

    /* Entities */
    ct_bullet_t bullets[CT_MAX_BULLETS];
    ct_enemy_t enemies[CT_MAX_ENEMIES];
    ct_particle_t particles[CT_MAX_PARTICLES];

    /* Level */
    ct_platform_t platforms[CT_MAX_PLATFORMS];
    int platform_count;

    /* Score & HUD */
    int score;
    bool need_full_redraw;
    bool hud_dirty;
    char buf_lives[16];
    char buf_score[16];
} ct_game_t;

/*=============================================================================
 *  Static Data
 *=============================================================================*/

static ui_app_page_t s_game_ct;
static ui_widget_t s_touch_area;
static ui_widget_t s_btn_up, s_btn_down, s_btn_left, s_btn_right;
static ui_widget_t s_btn_jump, s_btn_shoot;
static ui_widget_t *s_ct_widgets[9];
static ct_game_t s_ct;

/*=============================================================================
 *  Helpers
 *=============================================================================*/

static void ct_itoa(int val, char *out)
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

static uint32_t s_rng = 0xCAFEu;
static uint32_t ct_rng(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int32_t ct_rng_range(int32_t min, int32_t max)
{
    if (max <= min) return min;
    return min + (int32_t)(ct_rng() % (uint32_t)(max - min + 1));
}

static bool ct_rects_overlap(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                             int16_t bx, int16_t by, int16_t bw, int16_t bh)
{
    return (ax < bx + bw && ax + aw > bx &&
            ay < by + bh && ay + ah > by);
}

static void ct_update_hud_texts(void)
{
    ct_itoa(s_ct.lives, s_ct.buf_lives);
    ct_itoa(s_ct.score, s_ct.buf_score);
}

/*=============================================================================
 *  Dirty Region Invalidation
 *=============================================================================*/

/* Invalidate a world-coordinate rect (converts to screen coords) */
static void ct_inv_world_rect(int16_t wx, int16_t wy, int16_t w, int16_t h)
{
    int16_t sx = wx - s_ct.camera_x;
    int16_t sy = wy + CT_AREA_Y;
    /* Clip to game area */
    if (sx + w <= 0 || sx >= CT_AREA_W) return;
    if (sy + h <= CT_AREA_Y || sy >= UI_SCREEN_HEIGHT) return;
    if (sx < 0) { w += sx; sx = 0; }
    if (sy < CT_AREA_Y) { h -= (CT_AREA_Y - sy); sy = CT_AREA_Y; }
    ui_rect_t r = {sx, sy, w, h};
    ui_page_invalidate(&r);
}

/* Invalidate a world-coordinate rect using a specific camera offset */
static void ct_inv_world_rect_at(int16_t wx, int16_t wy, int16_t w, int16_t h, int16_t cam_x)
{
    int16_t sx = wx - cam_x;
    int16_t sy = wy + CT_AREA_Y;
    /* Clip to game area horizontally */
    if (sx + w <= 0 || sx >= CT_AREA_W) return;
    /* Clip to game area vertically (don't invalidate title bar) */
    if (sy + h <= CT_AREA_Y || sy >= UI_SCREEN_HEIGHT) return;
    if (sx < 0) { w += sx; sx = 0; }
    if (sy < CT_AREA_Y) { h -= (CT_AREA_Y - sy); sy = CT_AREA_Y; }
    ui_rect_t r = {sx, sy, w, h};
    ui_page_invalidate(&r);
}

/* Invalidate player region (with padding for gun) */
static void ct_inv_player(int16_t px, int16_t py, bool crouching, int16_t cam_x)
{
    int16_t ph = crouching ? CT_PLAYER_CROUCH_H : CT_PLAYER_H;
    /* Extra padding for gun (8px) and up-aim (8px) */
    ct_inv_world_rect_at(px - 8, py - 8, CT_PLAYER_W + 16, ph + 8, cam_x);
}

/* Invalidate enemy region (with padding for gun and boss HP bar) */
static void ct_inv_enemy(ct_enemy_t *e, int16_t cam_x)
{
    int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
    int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
    /* Padding for gun (6px) and boss HP bar (8px above) */
    ct_inv_world_rect_at(e->x - 6, e->y - 8, ew + 12, eh + 8, cam_x);
}

/* Invalidate bullet region */
static void ct_inv_bullet(ct_bullet_t *b, int16_t cam_x)
{
    int16_t bw = b->from_player ? CT_BULLET_W : CT_E_BULLET_W;
    int16_t bh = b->from_player ? CT_BULLET_H : CT_E_BULLET_H;
    ct_inv_world_rect_at(b->x, b->y, bw, bh, cam_x);
}

/* Invalidate particle region */
static void ct_inv_particle(ct_particle_t *p, int16_t cam_x)
{
    ct_inv_world_rect_at(p->x, p->y, 5, 5, cam_x);
}

/* Invalidate HUD area */
static void ct_inv_hud(void)
{
    ui_rect_t r = {0, CT_AREA_Y, CT_AREA_W, CT_HUD_H};
    ui_page_invalidate(&r);
}

/* Invalidate entire game area */
static void ct_inv_game_area(void)
{
    ui_rect_t r = {0, CT_AREA_Y, CT_AREA_W, CT_AREA_H};
    ui_page_invalidate(&r);
}

/* Invalidate a control button area */
static void ct_inv_rect(int16_t x, int16_t y, int16_t w, int16_t h)
{
    ui_rect_t r = {x, y, w, h};
    ui_page_invalidate(&r);
}

/*=============================================================================
 *  Level Definition
 *=============================================================================*/

static void ct_build_level(void)
{
    s_ct.platform_count = 0;
    ct_platform_t *p = s_ct.platforms;

    /* Ground segments (with gaps) */
    /* Section 1: Start area (0-1200) */
    p[s_ct.platform_count++] = (ct_platform_t){0, CT_GROUND_Y, 1200, 60};

    /* Section 2: After first gap (1300-2500) */
    p[s_ct.platform_count++] = (ct_platform_t){1300, CT_GROUND_Y, 1200, 60};

    /* Section 3: After second gap (2600-3800) */
    p[s_ct.platform_count++] = (ct_platform_t){2600, CT_GROUND_Y, 1200, 60};

    /* Section 4: Boss area (3900-5000) */
    p[s_ct.platform_count++] = (ct_platform_t){3900, CT_GROUND_Y, 1100, 60};

    /* Elevated platforms */
    p[s_ct.platform_count++] = (ct_platform_t){200, 300, 120, 16};
    p[s_ct.platform_count++] = (ct_platform_t){450, 260, 100, 16};
    p[s_ct.platform_count++] = (ct_platform_t){700, 300, 120, 16};
    p[s_ct.platform_count++] = (ct_platform_t){950, 240, 100, 16};

    p[s_ct.platform_count++] = (ct_platform_t){1400, 300, 120, 16};
    p[s_ct.platform_count++] = (ct_platform_t){1700, 260, 100, 16};
    p[s_ct.platform_count++] = (ct_platform_t){2000, 300, 120, 16};
    p[s_ct.platform_count++] = (ct_platform_t){2300, 220, 100, 16};

    p[s_ct.platform_count++] = (ct_platform_t){2800, 300, 120, 16};
    p[s_ct.platform_count++] = (ct_platform_t){3100, 260, 100, 16};
    p[s_ct.platform_count++] = (ct_platform_t){3400, 300, 120, 16};
    p[s_ct.platform_count++] = (ct_platform_t){3700, 220, 100, 16};

    /* Boss area platforms */
    p[s_ct.platform_count++] = (ct_platform_t){4100, 280, 100, 16};
    p[s_ct.platform_count++] = (ct_platform_t){4400, 240, 100, 16};
    p[s_ct.platform_count++] = (ct_platform_t){4700, 280, 100, 16};

    /* Floating platforms over gaps */
    p[s_ct.platform_count++] = (ct_platform_t){1200, 320, 100, 16};
    p[s_ct.platform_count++] = (ct_platform_t){2500, 320, 100, 16};
}

static void ct_spawn_enemies(void)
{
    memset(s_ct.enemies, 0, sizeof(s_ct.enemies));

    /* Walkers on ground sections */
    struct { int x; int y; ct_enemy_type_t type; int hp; int min_x; int max_x; } spawns[] = {
        /* Section 1 walkers */
        {400,  CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 300,  800},
        {800,  CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 700,  1100},
        /* Section 1 turrets on platforms */
        {470,  260 - CT_ENEMY_H,          CT_ENEMY_TURRET, 3, 0, 0},
        {970,  240 - CT_ENEMY_H,          CT_ENEMY_TURRET, 3, 0, 0},

        /* Section 2 walkers */
        {1500, CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 1400, 1800},
        {1900, CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 1800, 2200},
        {2300, CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 2200, 2450},
        /* Section 2 turrets */
        {1720, 260 - CT_ENEMY_H,          CT_ENEMY_TURRET, 3, 0, 0},
        {2320, 220 - CT_ENEMY_H,          CT_ENEMY_TURRET, 3, 0, 0},

        /* Section 3 walkers */
        {2900, CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 2800, 3200},
        {3300, CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 3200, 3600},
        {3600, CT_GROUND_Y - CT_ENEMY_H, CT_ENEMY_WALKER, 2, 3500, 3750},
        /* Section 3 turrets */
        {3120, 260 - CT_ENEMY_H,          CT_ENEMY_TURRET, 3, 0, 0},
        {3720, 220 - CT_ENEMY_H,          CT_ENEMY_TURRET, 3, 0, 0},

        /* Boss */
        {4600, CT_GROUND_Y - CT_BOSS_H,  CT_ENEMY_BOSS, CT_BOSS_HP, 4200, 4800},
    };

    int count = sizeof(spawns) / sizeof(spawns[0]);
    if (count > CT_MAX_ENEMIES) count = CT_MAX_ENEMIES;

    for (int i = 0; i < count; i++) {
        ct_enemy_t *e = &s_ct.enemies[i];
        e->x = spawns[i].x;
        e->y = spawns[i].y;
        e->prev_x = spawns[i].x;
        e->prev_y = spawns[i].y;
        e->prev_hp = spawns[i].hp;
        e->type = spawns[i].type;
        e->hp = spawns[i].hp;
        e->active = true;
        e->facing_right = false;
        e->vx = (spawns[i].type == CT_ENEMY_WALKER) ? -1 : 0;
        e->patrol_min_x = spawns[i].min_x;
        e->patrol_max_x = spawns[i].max_x;
        e->fire_timer = ct_rng_range(30, 80);
    }
}

/*=============================================================================
 *  Game Logic
 *=============================================================================*/

static void ct_start_game(void)
{
    memset(&s_ct, 0, sizeof(s_ct));
    s_ct.state = CT_STATE_PLAYING;
    s_ct.lives = CT_LIVES;
    s_ct.px = 50;
    s_ct.py = CT_GROUND_Y - CT_PLAYER_H;
    s_ct.pvx = 0;
    s_ct.pvy = 0;
    s_ct.facing_right = true;
    s_ct.on_ground = true;
    s_ct.invincible = 0;
    s_ct.fire_timer = 0;
    s_ct.camera_x = 0;
    s_ct.score = 0;
    s_ct.need_full_redraw = true;
    s_ct.hud_dirty = true;

    ct_build_level();
    ct_spawn_enemies();
    ct_update_hud_texts();
}

static void ct_player_die(void)
{
    /* Mark old player position as dirty before resetting */
    ct_inv_player(s_ct.px, s_ct.py, s_ct.crouching, s_ct.camera_x);

    s_ct.lives--;
    ct_update_hud_texts();
    s_ct.hud_dirty = true;

    if (s_ct.lives <= 0) {
        s_ct.state = CT_STATE_GAMEOVER;
        s_ct.need_full_redraw = true;
        return;
    }

    /* Respawn at current camera position */
    s_ct.px = s_ct.camera_x + 50;
    s_ct.py = CT_GROUND_Y - CT_PLAYER_H;
    s_ct.pvx = 0;
    s_ct.pvy = 0;
    s_ct.on_ground = true;
    s_ct.crouching = false;
    s_ct.invincible = CT_INVINCIBLE_TIME;

    /* Mark new respawn position as dirty */
    ct_inv_player(s_ct.px, s_ct.py, s_ct.crouching, s_ct.camera_x);
    ct_inv_hud();
}

static void ct_fire_bullet(int16_t wx, int16_t wy, int16_t vx, int16_t vy, bool from_player)
{
    for (int i = 0; i < CT_MAX_BULLETS; i++) {
        ct_bullet_t *b = &s_ct.bullets[i];
        if (!b->active) {
            b->x = wx;
            b->y = wy;
            b->prev_x = wx;
            b->prev_y = wy;
            b->vx = vx;
            b->vy = vy;
            b->active = true;
            b->from_player = from_player;
            return;
        }
    }
}

static void ct_player_shoot(void)
{
    if (s_ct.fire_timer > 0) return;
    s_ct.fire_timer = CT_FIRE_INTERVAL;

    int16_t bx, by, bvx, bvy;
    by = s_ct.py + (s_ct.crouching ? CT_PLAYER_CROUCH_H / 2 : CT_PLAYER_H / 2);

    if (s_ct.input_up && !s_ct.crouching) {
        /* Shoot upward */
        bx = s_ct.px + CT_PLAYER_W / 2 - CT_BULLET_W / 2;
        bvx = 0;
        bvy = -CT_BULLET_SPEED;
    } else if (s_ct.facing_right) {
        bx = s_ct.px + CT_PLAYER_W;
        bvx = CT_BULLET_SPEED;
        bvy = 0;
    } else {
        bx = s_ct.px - CT_BULLET_W;
        bvx = -CT_BULLET_SPEED;
        bvy = 0;
    }

    ct_fire_bullet(bx, by, bvx, bvy, true);
}

static void ct_enemy_shoot(ct_enemy_t *e)
{
    if (!e->active) return;

    int16_t ex, ey, evx, evy;
    int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
    int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;

    ex = e->x + ew / 2;
    ey = e->y + eh / 2;

    /* Aim at player */
    int16_t dx = (s_ct.px + CT_PLAYER_W / 2) - ex;
    int16_t dy = (s_ct.py + CT_PLAYER_H / 2) - ey;

    if (e->type == CT_ENEMY_BOSS) {
        /* Boss shoots spread */
        for (int i = -1; i <= 1; i++) {
            int16_t bvx = (dx > 0) ? CT_E_BULLET_SPEED : -CT_E_BULLET_SPEED;
            int16_t bvy = dy / (dx != 0 ? (dx > 0 ? 4 : -4) : 1) + i * 2;
            ct_fire_bullet(ex, ey, bvx, bvy, false);
        }
    } else {
        /* Simple aimed shot */
        if (dx != 0) {
            evx = (dx > 0) ? CT_E_BULLET_SPEED : -CT_E_BULLET_SPEED;
            evy = (int16_t)((int32_t)dy * CT_E_BULLET_SPEED / (dx > 0 ? dx : -dx));
        } else {
            evx = 0;
            evy = (dy > 0) ? CT_E_BULLET_SPEED : -CT_E_BULLET_SPEED;
        }
        ct_fire_bullet(ex, ey, evx, evy, false);
    }
}

static void ct_spawn_particle(int16_t x, int16_t y)
{
    for (int i = 0; i < CT_MAX_PARTICLES; i++) {
        ct_particle_t *p = &s_ct.particles[i];
        if (!p->active) {
            p->x = x;
            p->y = y;
            p->prev_x = x;
            p->prev_y = y;
            p->vx = ct_rng_range(-3, 3);
            p->vy = ct_rng_range(-4, 0);
            p->life = ct_rng_range(8, 20);
            p->active = true;
            return;
        }
    }
}

static bool ct_check_platform_collision(int16_t x, int16_t y, int16_t w, int16_t h)
{
    for (int i = 0; i < s_ct.platform_count; i++) {
        ct_platform_t *p = &s_ct.platforms[i];
        if (ct_rects_overlap(x, y, w, h, p->x, p->y, p->w, p->h))
            return true;
    }
    return false;
}

/* Get the ground Y at a given world X position */
static int16_t ct_get_ground_y(int16_t wx)
{
    /* Check platforms for ground-level ones */
    for (int i = 0; i < s_ct.platform_count; i++) {
        ct_platform_t *p = &s_ct.platforms[i];
        if (p->h >= 40 && wx >= p->x && wx < p->x + p->w)
            return p->y;
    }
    return CT_AREA_H; /* No ground = pit */
}

/*=============================================================================
 *  Update
 *=============================================================================*/

static void ct_update(void)
{
    if (s_ct.state != CT_STATE_PLAYING) return;

    s_ct.frame_count++;

    /* Save previous frame state for dirty tracking */
    s_ct.prev_px = s_ct.px;
    s_ct.prev_py = s_ct.py;
    s_ct.prev_crouching = s_ct.crouching;
    s_ct.prev_camera_x = s_ct.camera_x;
    s_ct.prev_input_left = s_ct.input_left;
    s_ct.prev_input_right = s_ct.input_right;
    s_ct.prev_input_up = s_ct.input_up;
    s_ct.prev_input_down = s_ct.input_down;
    s_ct.prev_input_jump = s_ct.input_jump;
    s_ct.prev_input_shoot = s_ct.input_shoot;

    for (int i = 0; i < CT_MAX_ENEMIES; i++) {
        ct_enemy_t *e = &s_ct.enemies[i];
        if (!e->active) continue;
        e->prev_x = e->x;
        e->prev_y = e->y;
        e->prev_hp = e->hp;
    }
    for (int i = 0; i < CT_MAX_BULLETS; i++) {
        ct_bullet_t *b = &s_ct.bullets[i];
        if (!b->active) continue;
        b->prev_x = b->x;
        b->prev_y = b->y;
    }
    for (int i = 0; i < CT_MAX_PARTICLES; i++) {
        ct_particle_t *p = &s_ct.particles[i];
        if (!p->active) continue;
        p->prev_x = p->x;
        p->prev_y = p->y;
    }

    /* --- Player movement --- */
    s_ct.pvx = 0;
    if (!s_ct.crouching) {
        if (s_ct.input_left)  s_ct.pvx = -CT_PLAYER_SPEED;
        if (s_ct.input_right) s_ct.pvx = CT_PLAYER_SPEED;
    }
    if (s_ct.input_left)  s_ct.facing_right = false;
    if (s_ct.input_right) s_ct.facing_right = true;

    /* Crouch */
    if (s_ct.input_down && s_ct.on_ground) {
        s_ct.crouching = true;
        s_ct.pvx = 0;
    } else {
        s_ct.crouching = false;
    }

    /* Jump */
    if (s_ct.input_jump && s_ct.on_ground) {
        s_ct.pvy = CT_JUMP_VY;
        s_ct.on_ground = false;
    }

    /* Shoot */
    if (s_ct.input_shoot) {
        ct_player_shoot();
    }
    if (s_ct.fire_timer > 0) s_ct.fire_timer--;

    /* Gravity */
    s_ct.pvy += CT_GRAVITY;

    /* Move player horizontally */
    int16_t new_px = s_ct.px + s_ct.pvx;
    int16_t ph = s_ct.crouching ? CT_PLAYER_CROUCH_H : CT_PLAYER_H;

    /* Horizontal collision */
    if (s_ct.pvx != 0) {
        if (ct_check_platform_collision(new_px, s_ct.py, CT_PLAYER_W, ph)) {
            /* Try to slide up one pixel (step over small edges) */
            if (!ct_check_platform_collision(new_px, s_ct.py - 2, CT_PLAYER_W, ph)) {
                new_px = s_ct.px + s_ct.pvx;
                s_ct.py -= 2;
            } else {
                new_px = s_ct.px;
            }
        }
    }

    /* World bounds */
    if (new_px < 0) new_px = 0;
    if (new_px > CT_LEVEL_WIDTH - CT_PLAYER_W) new_px = CT_LEVEL_WIDTH - CT_PLAYER_W;
    s_ct.px = new_px;

    /* Move player vertically */
    int16_t new_py = s_ct.py + s_ct.pvy;
    s_ct.on_ground = false;

    if (s_ct.pvy >= 0) {
        /* Falling: check landing on platforms */
        bool landed = false;
        for (int i = 0; i < s_ct.platform_count; i++) {
            ct_platform_t *p = &s_ct.platforms[i];
            /* Check if player's feet cross the platform top */
            if (s_ct.px + CT_PLAYER_W > p->x && s_ct.px < p->x + p->w) {
                if (s_ct.py + ph <= p->y && new_py + ph >= p->y) {
                    new_py = p->y - ph;
                    s_ct.pvy = 0;
                    s_ct.on_ground = true;
                    landed = true;
                    break;
                }
            }
        }
        if (!landed && new_py + ph > CT_AREA_H) {
            /* Fell into pit */
            ct_player_die();
            return;
        }
    } else {
        /* Rising: check head collision */
        if (ct_check_platform_collision(s_ct.px, new_py, CT_PLAYER_W, ph)) {
            s_ct.pvy = 0;
            new_py = s_ct.py;
        }
    }

    s_ct.py = new_py;

    /* Invincibility countdown */
    if (s_ct.invincible > 0) s_ct.invincible--;

    /* --- Camera --- */
    int16_t target_cx = s_ct.px - CT_CAMERA_MARGIN;
    if (target_cx < 0) target_cx = 0;
    if (target_cx > CT_LEVEL_WIDTH - CT_AREA_W)
        target_cx = CT_LEVEL_WIDTH - CT_AREA_W;
    /* Smooth camera follow */
    s_ct.camera_x += (target_cx - s_ct.camera_x) / 4;

    /* --- Update enemies --- */
    for (int i = 0; i < CT_MAX_ENEMIES; i++) {
        ct_enemy_t *e = &s_ct.enemies[i];
        if (!e->active) continue;

        /* Only update enemies near camera */
        int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
        if (e->x + ew < s_ct.camera_x - 100 || e->x > s_ct.camera_x + CT_AREA_W + 100)
            continue;

        /* Walker movement */
        if (e->type == CT_ENEMY_WALKER) {
            e->x += e->vx;
            if (e->x <= e->patrol_min_x || e->x + ew >= e->patrol_max_x) {
                e->vx = -e->vx;
            }
            e->facing_right = (e->vx > 0);
        }

        /* Face player */
        if (e->type != CT_ENEMY_WALKER) {
            e->facing_right = (s_ct.px > e->x);
        }

        /* Fire timer */
        e->fire_timer--;
        if (e->fire_timer <= 0) {
            /* Only shoot if player is roughly in range */
            int16_t dist_x = s_ct.px - e->x;
            if (dist_x < 0) dist_x = -dist_x;
            if (dist_x < CT_AREA_W) {
                ct_enemy_shoot(e);
            }
            if (e->type == CT_ENEMY_BOSS) {
                e->fire_timer = ct_rng_range(25, 50);
            } else if (e->type == CT_ENEMY_TURRET) {
                e->fire_timer = ct_rng_range(50, 90);
            } else {
                e->fire_timer = ct_rng_range(60, 120);
            }
        }
    }

    /* --- Update bullets --- */
    for (int i = 0; i < CT_MAX_BULLETS; i++) {
        ct_bullet_t *b = &s_ct.bullets[i];
        if (!b->active) continue;

        b->x += b->vx;
        b->y += b->vy;

        /* Out of bounds */
        if (b->y < -20 || b->y > CT_AREA_H + 20 ||
            b->x < s_ct.camera_x - 50 || b->x > s_ct.camera_x + CT_AREA_W + 50) {
            b->active = false;
            continue;
        }

        /* Platform collision for all bullets */
        if (ct_check_platform_collision(b->x, b->y,
                b->from_player ? CT_BULLET_W : CT_E_BULLET_W,
                b->from_player ? CT_BULLET_H : CT_E_BULLET_H)) {
            ct_spawn_particle(b->x, b->y);
            b->active = false;
            continue;
        }

        if (b->from_player) {
            /* Player bullet vs enemies */
            for (int j = 0; j < CT_MAX_ENEMIES; j++) {
                ct_enemy_t *e = &s_ct.enemies[j];
                if (!e->active) continue;
                int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
                int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
                if (ct_rects_overlap(b->x, b->y, CT_BULLET_W, CT_BULLET_H,
                                     e->x, e->y, ew, eh)) {
                    e->hp--;
                    b->active = false;
                    ct_spawn_particle(b->x, b->y);
                    if (e->hp <= 0) {
                        e->active = false;
                        s_ct.score += (e->type == CT_ENEMY_BOSS) ? 500 :
                                      (e->type == CT_ENEMY_TURRET) ? 100 : 50;
                        ct_update_hud_texts();
                        s_ct.hud_dirty = true;
                        /* Boss death = win */
                        if (e->type == CT_ENEMY_BOSS) {
                            s_ct.state = CT_STATE_WIN;
                            s_ct.need_full_redraw = true;
                            return;
                        }
                        /* Explosion particles */
                        for (int k = 0; k < 5; k++)
                            ct_spawn_particle(e->x + ew / 2, e->y + eh / 2);
                    }
                    break;
                }
            }
        } else {
            /* Enemy bullet vs player */
            if (s_ct.invincible <= 0) {
                int16_t ph = s_ct.crouching ? CT_PLAYER_CROUCH_H : CT_PLAYER_H;
                if (ct_rects_overlap(b->x, b->y, CT_E_BULLET_W, CT_E_BULLET_H,
                                     s_ct.px, s_ct.py, CT_PLAYER_W, ph)) {
                    /* Mark bullet position before deactivating */
                    ct_inv_bullet(b, s_ct.camera_x);
                    b->active = false;
                    s_ct.invincible = CT_INVINCIBLE_TIME;
                    ct_player_die();
                    return;
                }
            }
        }
    }

    /* --- Update particles --- */
    for (int i = 0; i < CT_MAX_PARTICLES; i++) {
        ct_particle_t *p = &s_ct.particles[i];
        if (!p->active) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vy += 1;
        p->life--;
        if (p->life <= 0) p->active = false;
    }

    /* --- Enemy vs player collision --- */
    if (s_ct.invincible <= 0) {
        int16_t ph = s_ct.crouching ? CT_PLAYER_CROUCH_H : CT_PLAYER_H;
        for (int i = 0; i < CT_MAX_ENEMIES; i++) {
            ct_enemy_t *e = &s_ct.enemies[i];
            if (!e->active) continue;
            int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
            int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
            if (ct_rects_overlap(s_ct.px, s_ct.py, CT_PLAYER_W, ph,
                                 e->x, e->y, ew, eh)) {
                s_ct.invincible = CT_INVINCIBLE_TIME;
                ct_player_die();
                return;
            }
        }
    }

    /* --- Mark dirty regions for partial refresh --- */
    /* Camera scroll: mark old and new positions of all visible elements.
       Background (sky) is uniform so erasing dirty regions with sky color is correct. */
    if (s_ct.camera_x != s_ct.prev_camera_x) {
        /* Mark the left/right edge strip revealed by scrolling */
        int16_t dx = s_ct.camera_x - s_ct.prev_camera_x;
        if (dx > 0) {
            /* Scrolled right: new area on the right edge */
            ui_rect_t edge = {CT_AREA_W - dx, CT_AREA_Y, dx, CT_AREA_H};
            ui_page_invalidate(&edge);
        } else {
            /* Scrolled left: new area on the left edge */
            ui_rect_t edge = {0, CT_AREA_Y, -dx, CT_AREA_H};
            ui_page_invalidate(&edge);
        }

        /* Mark platforms at old and new camera offsets */
        for (int i = 0; i < s_ct.platform_count; i++) {
            ct_platform_t *p = &s_ct.platforms[i];
            ct_inv_world_rect_at(p->x, p->y, p->w, p->h, s_ct.prev_camera_x);
            ct_inv_world_rect_at(p->x, p->y, p->w, p->h, s_ct.camera_x);
        }

        /* Mark mountains (parallax) at old and new offsets */
        {
            int16_t old_m_offset = s_ct.prev_camera_x / 3;
            int16_t new_m_offset = s_ct.camera_x / 3;
            for (int i = 0; i < 8; i++) {
                int16_t old_mx = i * 200 - (old_m_offset % 200);
                int16_t new_mx = i * 200 - (new_m_offset % 200);
                int16_t mh = 40 + (i * 37) % 60;
                ui_rect_t old_mr = {old_mx, CT_AREA_Y + CT_GROUND_Y - mh - 20, 120, mh};
                ui_rect_t new_mr = {new_mx, CT_AREA_Y + CT_GROUND_Y - mh - 20, 120, mh};
                ui_page_invalidate(&old_mr);
                ui_page_invalidate(&new_mr);
            }
        }

        /* Mark old positions of all entities at old camera offset */
        ct_inv_player(s_ct.prev_px, s_ct.prev_py, s_ct.prev_crouching, s_ct.prev_camera_x);
        for (int i = 0; i < CT_MAX_ENEMIES; i++) {
            ct_enemy_t *e = &s_ct.enemies[i];
            if (!e->active && e->prev_x == 0 && e->prev_y == 0) continue;
            int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
            int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
            if (e->prev_x != e->x || e->prev_y != e->y || !e->active) {
                ct_inv_world_rect_at(e->prev_x - 6, e->prev_y - 8, ew + 12, eh + 8, s_ct.prev_camera_x);
            }
            if (!e->active && (e->prev_x != 0 || e->prev_y != 0)) {
                ct_inv_world_rect_at(e->x - 6, e->y - 8, ew + 12, eh + 8, s_ct.camera_x);
            }
        }
        for (int i = 0; i < CT_MAX_BULLETS; i++) {
            ct_bullet_t *b = &s_ct.bullets[i];
            int16_t bw = b->from_player ? CT_BULLET_W : CT_E_BULLET_W;
            int16_t bh = b->from_player ? CT_BULLET_H : CT_E_BULLET_H;
            if (b->prev_x != b->x || b->prev_y != b->y || !b->active) {
                ct_inv_world_rect_at(b->prev_x, b->prev_y, bw, bh, s_ct.prev_camera_x);
            }
            if (!b->active && (b->prev_x != 0 || b->prev_y != 0)) {
                ct_inv_world_rect_at(b->x, b->y, bw, bh, s_ct.camera_x);
            }
        }
        for (int i = 0; i < CT_MAX_PARTICLES; i++) {
            ct_particle_t *p = &s_ct.particles[i];
            if (p->prev_x != p->x || p->prev_y != p->y || !p->active) {
                ct_inv_world_rect_at(p->prev_x, p->prev_y, 5, 5, s_ct.prev_camera_x);
            }
        }

        /* Mark new positions of all entities at new camera offset */
        ct_inv_player(s_ct.px, s_ct.py, s_ct.crouching, s_ct.camera_x);
        for (int i = 0; i < CT_MAX_ENEMIES; i++) {
            ct_enemy_t *e = &s_ct.enemies[i];
            if (e->active) {
                ct_inv_enemy(e, s_ct.camera_x);
            }
        }
        for (int i = 0; i < CT_MAX_BULLETS; i++) {
            ct_bullet_t *b = &s_ct.bullets[i];
            if (b->active) {
                ct_inv_bullet(b, s_ct.camera_x);
            }
        }
        for (int i = 0; i < CT_MAX_PARTICLES; i++) {
            ct_particle_t *p = &s_ct.particles[i];
            if (p->active) {
                ct_inv_particle(p, s_ct.camera_x);
            }
        }

        /* HUD only if dirty (score/lives changed) */
        if (s_ct.hud_dirty) {
            ct_inv_hud();
        }

        return;
    }

    /* Player old and new position */
    ct_inv_player(s_ct.prev_px, s_ct.prev_py, s_ct.prev_crouching, s_ct.prev_camera_x);
    ct_inv_player(s_ct.px, s_ct.py, s_ct.crouching, s_ct.camera_x);

    /* When invincible, player blinks so current position needs redraw every frame */
    if (s_ct.invincible > 0) {
        ct_inv_player(s_ct.px, s_ct.py, s_ct.crouching, s_ct.camera_x);
    }

    /* Enemies */
    for (int i = 0; i < CT_MAX_ENEMIES; i++) {
        ct_enemy_t *e = &s_ct.enemies[i];
        if (!e->active && e->prev_x == 0 && e->prev_y == 0) continue;
        int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
        int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
        /* Invalidate old position if enemy moved or died */
        if (e->prev_x != e->x || e->prev_y != e->y || !e->active) {
            ct_inv_world_rect_at(e->prev_x - 6, e->prev_y - 8, ew + 12, eh + 8, s_ct.prev_camera_x);
        }
        if (e->active) {
            ct_inv_enemy(e, s_ct.camera_x);
            /* Boss HP bar changed */
            if (e->type == CT_ENEMY_BOSS && e->hp != e->prev_hp) {
                ct_inv_enemy(e, s_ct.camera_x);
            }
        } else if (e->prev_x != 0 || e->prev_y != 0) {
            /* Enemy just died: also invalidate its last known position */
            ct_inv_world_rect_at(e->x - 6, e->y - 8, ew + 12, eh + 8, s_ct.camera_x);
        }
    }

    /* Bullets */
    for (int i = 0; i < CT_MAX_BULLETS; i++) {
        ct_bullet_t *b = &s_ct.bullets[i];
        int16_t bw = b->from_player ? CT_BULLET_W : CT_E_BULLET_W;
        int16_t bh = b->from_player ? CT_BULLET_H : CT_E_BULLET_H;
        /* Invalidate old position (prev frame) */
        if (b->prev_x != b->x || b->prev_y != b->y || !b->active) {
            ct_inv_world_rect_at(b->prev_x, b->prev_y, bw, bh, s_ct.prev_camera_x);
        }
        if (b->active) {
            ct_inv_bullet(b, s_ct.camera_x);
        } else if (b->prev_x != 0 || b->prev_y != 0) {
            /* Bullet just destroyed: also invalidate its last known position
             * (b->x/b->y may be the collision point, not prev position) */
            ct_inv_world_rect_at(b->x, b->y, bw, bh, s_ct.camera_x);
        }
    }

    /* Particles */
    for (int i = 0; i < CT_MAX_PARTICLES; i++) {
        ct_particle_t *p = &s_ct.particles[i];
        if (p->prev_x != p->x || p->prev_y != p->y || !p->active) {
            ct_inv_world_rect_at(p->prev_x, p->prev_y, 5, 5, s_ct.prev_camera_x);
        }
        if (p->active) {
            ct_inv_particle(p, s_ct.camera_x);
        }
    }

    /* HUD */
    if (s_ct.hud_dirty) {
        ct_inv_hud();
        s_ct.hud_dirty = false;
    }

    /* Control buttons */
    if (s_ct.input_up != s_ct.prev_input_up)
        ct_inv_rect(CT_DPAD_UP_X, CT_DPAD_UP_Y, CT_DPAD_BTN, CT_DPAD_BTN);
    if (s_ct.input_down != s_ct.prev_input_down)
        ct_inv_rect(CT_DPAD_DOWN_X, CT_DPAD_DOWN_Y, CT_DPAD_BTN, CT_DPAD_BTN);
    if (s_ct.input_left != s_ct.prev_input_left)
        ct_inv_rect(CT_DPAD_LEFT_X, CT_DPAD_LEFT_Y, CT_DPAD_BTN, CT_DPAD_BTN);
    if (s_ct.input_right != s_ct.prev_input_right)
        ct_inv_rect(CT_DPAD_RIGHT_X, CT_DPAD_RIGHT_Y, CT_DPAD_BTN, CT_DPAD_BTN);
    if (s_ct.input_jump != s_ct.prev_input_jump)
        ct_inv_rect(CT_ACT_RIGHT_X, CT_ACT_JUMP_Y, CT_ACT_BTN_W, CT_ACT_BTN_H);
    if (s_ct.input_shoot != s_ct.prev_input_shoot)
        ct_inv_rect(CT_ACT_RIGHT_X, CT_ACT_SHOOT_Y, CT_ACT_BTN_W, CT_ACT_BTN_H);
}

/*=============================================================================
 *  Drawing
 *=============================================================================*/

/* Draw a filled rect in screen coordinates (world x - camera_x) */
static void ct_draw_world_rect(int16_t wx, int16_t wy, int16_t w, int16_t h, ui_color_t color)
{
    int16_t sx = wx - s_ct.camera_x;
    int16_t sy = wy + CT_AREA_Y;
    /* Clip to screen */
    if (sx + w <= 0 || sx >= CT_AREA_W) return;
    if (sy + h <= CT_AREA_Y || sy >= UI_SCREEN_HEIGHT) return;
    ui_rect_t r = {sx, sy, w, h};
    ui_draw_fill_rect(&r, color);
}

static void ct_draw_hline(int16_t wx, int16_t wy, int16_t w, ui_color_t color)
{
    int16_t sx = wx - s_ct.camera_x;
    int16_t sy = wy + CT_AREA_Y;
    if (sx + w <= 0 || sx >= CT_AREA_W) return;
    ui_draw_hline(sx, sy, w, color);
}

static void ct_draw_game(void)
{
    ui_rect_t area = {CT_AREA_X, CT_AREA_Y, CT_AREA_W, CT_AREA_H};

    /* Sky background */
    ui_draw_fill_rect(&area, CT_BG_SKY);

    /* Stars / decoration */
    for (int i = 0; i < 30; i++) {
        int16_t sx = (i * 137 + 50) % CT_AREA_W;
        int16_t sy = (i * 89 + 30) % (CT_AREA_H - 100) + CT_AREA_Y;
        ui_draw_pixel(sx, sy, UI_HEX(0x555577));
    }

    /* Mountains in background (parallax) */
    int16_t m_offset = s_ct.camera_x / 3;
    for (int i = 0; i < 8; i++) {
        int16_t mx = i * 200 - (m_offset % 200);
        int16_t mh = 40 + (i * 37) % 60;
        ui_rect_t mr = {mx, CT_AREA_Y + CT_GROUND_Y - mh - 20, 120, mh};
        ui_draw_fill_rect(&mr, UI_HEX(0x1F3044));
    }

    /* Platforms */
    for (int i = 0; i < s_ct.platform_count; i++) {
        ct_platform_t *p = &s_ct.platforms[i];
        ct_draw_world_rect(p->x, p->y, p->w, p->h, CT_PLATFORM_COLOR);
        /* Grass top */
        ct_draw_hline(p->x, p->y, p->w, CT_PLATFORM_TOP);
    }

    /* Enemies */
    for (int i = 0; i < CT_MAX_ENEMIES; i++) {
        ct_enemy_t *e = &s_ct.enemies[i];
        if (!e->active) continue;

        int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
        int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
        ui_color_t body_color = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_COLOR : CT_ENEMY_COLOR;
        ui_color_t head_color = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_HEAD : CT_ENEMY_HEAD;

        ct_draw_world_rect(e->x, e->y, ew, eh, body_color);

        /* Head */
        int16_t head_w = ew * 2 / 3;
        int16_t head_h = eh / 4;
        int16_t head_x = e->x + (ew - head_w) / 2;
        ct_draw_world_rect(head_x, e->y, head_w, head_h, head_color);

        /* Boss HP bar */
        if (e->type == CT_ENEMY_BOSS) {
            int16_t bar_w = ew;
            int16_t bar_h = 4;
            ct_draw_world_rect(e->x, e->y - 8, bar_w, bar_h, UI_HEX(0x333333));
            int16_t fill_w = (int16_t)((int32_t)bar_w * e->hp / CT_BOSS_HP);
            ct_draw_world_rect(e->x, e->y - 8, fill_w, bar_h, UI_HEX(0xF44336));
        }

        /* Gun */
        int16_t gun_y = e->y + eh / 3;
        if (e->facing_right) {
            ct_draw_world_rect(e->x + ew, gun_y, 6, 3, UI_HEX(0x757575));
        } else {
            ct_draw_world_rect(e->x - 6, gun_y, 6, 3, UI_HEX(0x757575));
        }
    }

    /* Player */
    if (s_ct.state == CT_STATE_PLAYING || s_ct.state == CT_STATE_DEAD) {
        /* Blink when invincible */
        bool visible = (s_ct.invincible <= 0) || ((s_ct.invincible / 4) % 2 == 0);
        if (visible) {
            int16_t ph = s_ct.crouching ? CT_PLAYER_CROUCH_H : CT_PLAYER_H;
            ct_draw_world_rect(s_ct.px, s_ct.py, CT_PLAYER_W, ph, CT_PLAYER_COLOR);

            /* Head */
            int16_t head_w = CT_PLAYER_W * 2 / 3;
            int16_t head_h = CT_PLAYER_H / 5;
            int16_t head_x = s_ct.px + (CT_PLAYER_W - head_w) / 2;
            ct_draw_world_rect(head_x, s_ct.py, head_w, head_h, CT_PLAYER_HEAD);

            /* Gun */
            int16_t gun_y = s_ct.py + ph / 3;
            if (s_ct.facing_right) {
                ct_draw_world_rect(s_ct.px + CT_PLAYER_W, gun_y, 8, 3, CT_PLAYER_GUN);
            } else {
                ct_draw_world_rect(s_ct.px - 8, gun_y, 8, 3, CT_PLAYER_GUN);
            }

            /* Up-aim indicator */
            if (s_ct.input_up && !s_ct.crouching) {
                int16_t gun_x = s_ct.px + CT_PLAYER_W / 2 - 1;
                ct_draw_world_rect(gun_x, s_ct.py - 8, 3, 8, CT_PLAYER_GUN);
            }
        }
    }

    /* Bullets */
    for (int i = 0; i < CT_MAX_BULLETS; i++) {
        ct_bullet_t *b = &s_ct.bullets[i];
        if (!b->active) continue;
        if (b->from_player) {
            ct_draw_world_rect(b->x, b->y, CT_BULLET_W, CT_BULLET_H, CT_BULLET_COLOR);
        } else {
            ct_draw_world_rect(b->x, b->y, CT_E_BULLET_W, CT_E_BULLET_H, CT_E_BULLET_COLOR);
        }
    }

    /* Particles */
    for (int i = 0; i < CT_MAX_PARTICLES; i++) {
        ct_particle_t *p = &s_ct.particles[i];
        if (!p->active) continue;
        ct_draw_world_rect(p->x, p->y, 3, 3, CT_PARTICLE_COLOR);
    }

    /* HUD */
    ui_rect_t hud_bg = {0, CT_AREA_Y, CT_AREA_W, CT_HUD_H};
    ui_draw_fill_rect(&hud_bg, CT_HUD_BG);

    ui_rect_t lives_r = {10, CT_AREA_Y + 4, 100, CT_HUD_H - 8};
    char lives_buf[32];
    int li = 0;
    const char *lp = "LIVES:";
    while (*lp && li < 25) lives_buf[li++] = *lp++;
    const char *lv = s_ct.buf_lives;
    while (*lv && li < 31) lives_buf[li++] = *lv++;
    lives_buf[li] = '\0';
    ui_draw_text_in_rect(&lives_r, lives_buf, &font_montserrat_12, UI_COLOR_WHITE, 0);

    ui_rect_t score_r = {200, CT_AREA_Y + 4, 120, CT_HUD_H - 8};
    char score_buf[32];
    int si = 0;
    const char *sp = "SCORE:";
    while (*sp && si < 25) score_buf[si++] = *sp++;
    const char *sv = s_ct.buf_score;
    while (*sv && si < 31) score_buf[si++] = *sv++;
    score_buf[si] = '\0';
    ui_draw_text_in_rect(&score_r, score_buf, &font_montserrat_12, UI_COLOR_WHITE, 0);

    /* Control overlays (semi-transparent) */
    /* D-pad */
    ui_color_t dpad_c = ui_color_blend(CT_CTRL_BG, CT_BG_SKY, CT_CTRL_ALPHA);
    ui_color_t dpad_up_c = s_ct.input_up ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t dpad_dn_c = s_ct.input_down ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t dpad_lt_c = s_ct.input_left ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t dpad_rt_c = s_ct.input_right ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;

    ui_rect_t ur = {CT_DPAD_UP_X, CT_DPAD_UP_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_draw_fill_round_rect(&ur, 6, dpad_up_c);
    ui_draw_text_in_rect(&ur, "^", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t dr = {CT_DPAD_DOWN_X, CT_DPAD_DOWN_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_draw_fill_round_rect(&dr, 6, dpad_dn_c);
    ui_draw_text_in_rect(&dr, "v", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t lr = {CT_DPAD_LEFT_X, CT_DPAD_LEFT_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_draw_fill_round_rect(&lr, 6, dpad_lt_c);
    ui_draw_text_in_rect(&lr, "<", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t rr = {CT_DPAD_RIGHT_X, CT_DPAD_RIGHT_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_draw_fill_round_rect(&rr, 6, dpad_rt_c);
    ui_draw_text_in_rect(&rr, ">", &font_montserrat_16, UI_COLOR_WHITE, 1);

    /* Action buttons */
    ui_color_t jump_c = s_ct.input_jump ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t shoot_c = s_ct.input_shoot ? ui_color_blend(UI_HEX(0xFF5722), CT_BG_SKY, CT_CTRL_ALPHA) : ui_color_blend(UI_HEX(0xF44336), CT_BG_SKY, CT_CTRL_ALPHA);

    ui_rect_t jr = {CT_ACT_RIGHT_X, CT_ACT_JUMP_Y, CT_ACT_BTN_W, CT_ACT_BTN_H};
    ui_draw_fill_round_rect(&jr, 10, jump_c);
    ui_draw_text_in_rect(&jr, "JUMP", &font_montserrat_12, UI_COLOR_WHITE, 1);

    ui_rect_t sr = {CT_ACT_RIGHT_X, CT_ACT_SHOOT_Y, CT_ACT_BTN_W, CT_ACT_BTN_H};
    ui_draw_fill_round_rect(&sr, 10, shoot_c);
    ui_draw_text_in_rect(&sr, "FIRE", &font_montserrat_12, UI_COLOR_WHITE, 1);
}

static void ct_draw_idle_overlay(void)
{
    ui_rect_t overlay = {0, CT_AREA_Y, CT_AREA_W, CT_AREA_H};
    ui_color_t obg = ui_color_blend(UI_HEX(0x000000), CT_BG_SKY, 160);
    ui_draw_fill_rect(&overlay, obg);

    ui_rect_t title = {0, CT_AREA_Y + CT_AREA_H / 2 - 40, CT_AREA_W, 30};
    ui_draw_text_in_rect(&title, "CONTRA", &font_montserrat_16, UI_COLOR_WHITE, 1);

    ui_rect_t sub = {0, CT_AREA_Y + CT_AREA_H / 2, CT_AREA_W, 24};
    ui_draw_text_in_rect(&sub, "Tap screen to start", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

static void ct_draw_gameover_overlay(void)
{
    ui_rect_t overlay = {0, CT_AREA_Y, CT_AREA_W, CT_AREA_H};
    ui_color_t obg = ui_color_blend(UI_HEX(0x000000), CT_BG_SKY, 160);
    ui_draw_fill_rect(&overlay, obg);

    ui_rect_t title = {0, CT_AREA_Y + CT_AREA_H / 2 - 40, CT_AREA_W, 30};
    ui_draw_text_in_rect(&title, "GAME OVER", &font_montserrat_16,
                         UI_HEX(0xF44336), 1);

    char buf[32];
    int bi = 0;
    const char *sp = "Score: ";
    while (*sp && bi < 25) buf[bi++] = *sp++;
    const char *sv = s_ct.buf_score;
    while (*sv && bi < 31) buf[bi++] = *sv++;
    buf[bi] = '\0';

    ui_rect_t sr = {0, CT_AREA_Y + CT_AREA_H / 2, CT_AREA_W, 24};
    ui_draw_text_in_rect(&sr, buf, &font_montserrat_12, UI_COLOR_WHITE, 1);

    ui_rect_t hr = {0, CT_AREA_Y + CT_AREA_H / 2 + 30, CT_AREA_W, 24};
    ui_draw_text_in_rect(&hr, "Tap to restart", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

static void ct_draw_win_overlay(void)
{
    ui_rect_t overlay = {0, CT_AREA_Y, CT_AREA_W, CT_AREA_H};
    ui_color_t obg = ui_color_blend(UI_HEX(0x000000), CT_BG_SKY, 160);
    ui_draw_fill_rect(&overlay, obg);

    ui_rect_t title = {0, CT_AREA_Y + CT_AREA_H / 2 - 40, CT_AREA_W, 30};
    ui_draw_text_in_rect(&title, "VICTORY!", &font_montserrat_16,
                         UI_HEX(0x4CAF50), 1);

    char buf[32];
    int bi = 0;
    const char *sp = "Score: ";
    while (*sp && bi < 25) buf[bi++] = *sp++;
    const char *sv = s_ct.buf_score;
    while (*sv && bi < 31) buf[bi++] = *sv++;
    buf[bi] = '\0';

    ui_rect_t sr = {0, CT_AREA_Y + CT_AREA_H / 2, CT_AREA_W, 24};
    ui_draw_text_in_rect(&sr, buf, &font_montserrat_12, UI_COLOR_WHITE, 1);

    ui_rect_t hr = {0, CT_AREA_Y + CT_AREA_H / 2 + 30, CT_AREA_W, 24};
    ui_draw_text_in_rect(&hr, "Tap to restart", &font_montserrat_12,
                         UI_HEX(0xAAAAAA), 1);
}

/*=============================================================================
 *  Input Handling
 *=============================================================================*/

/* Check which button a screen position is in. Returns a bitmask. */
#define CT_BTN_UP       (1 << 0)
#define CT_BTN_DOWN     (1 << 1)
#define CT_BTN_LEFT     (1 << 2)
#define CT_BTN_RIGHT    (1 << 3)
#define CT_BTN_JUMP     (1 << 4)
#define CT_BTN_SHOOT    (1 << 5)

static uint8_t ct_hit_test(int16_t x, int16_t y)
{
    uint8_t mask = 0;
    if (x >= CT_DPAD_UP_X && x < CT_DPAD_UP_X + CT_DPAD_BTN &&
        y >= CT_DPAD_UP_Y && y < CT_DPAD_UP_Y + CT_DPAD_BTN)
        mask |= CT_BTN_UP;
    if (x >= CT_DPAD_DOWN_X && x < CT_DPAD_DOWN_X + CT_DPAD_BTN &&
        y >= CT_DPAD_DOWN_Y && y < CT_DPAD_DOWN_Y + CT_DPAD_BTN)
        mask |= CT_BTN_DOWN;
    if (x >= CT_DPAD_LEFT_X && x < CT_DPAD_LEFT_X + CT_DPAD_BTN &&
        y >= CT_DPAD_LEFT_Y && y < CT_DPAD_LEFT_Y + CT_DPAD_BTN)
        mask |= CT_BTN_LEFT;
    if (x >= CT_DPAD_RIGHT_X && x < CT_DPAD_RIGHT_X + CT_DPAD_BTN &&
        y >= CT_DPAD_RIGHT_Y && y < CT_DPAD_RIGHT_Y + CT_DPAD_BTN)
        mask |= CT_BTN_RIGHT;
    if (x >= CT_ACT_RIGHT_X && x < CT_ACT_RIGHT_X + CT_ACT_BTN_W &&
        y >= CT_ACT_JUMP_Y && y < CT_ACT_JUMP_Y + CT_ACT_BTN_H)
        mask |= CT_BTN_JUMP;
    if (x >= CT_ACT_RIGHT_X && x < CT_ACT_RIGHT_X + CT_ACT_BTN_W &&
        y >= CT_ACT_SHOOT_Y && y < CT_ACT_SHOOT_Y + CT_ACT_BTN_H)
        mask |= CT_BTN_SHOOT;
    return mask;
}

/* Per-touch-point tracking of which buttons are held */
static uint8_t s_touch_btns[UI_MAX_TOUCH_POINTS];

/* Recalculate input state from all active touch points */
static void ct_recalc_inputs(void)
{
    uint8_t combined = 0;
    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++) {
        combined |= s_touch_btns[i];
    }
    s_ct.input_up    = (combined & CT_BTN_UP) != 0;
    s_ct.input_down  = (combined & CT_BTN_DOWN) != 0;
    s_ct.input_left  = (combined & CT_BTN_LEFT) != 0;
    s_ct.input_right = (combined & CT_BTN_RIGHT) != 0;
    s_ct.input_jump  = (combined & CT_BTN_JUMP) != 0;
    s_ct.input_shoot = (combined & CT_BTN_SHOOT) != 0;
}

static void ct_touch_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    /* Start / restart on tap */
    if (e->type == UI_EVENT_PRESS) {
        if (s_ct.state == CT_STATE_IDLE || s_ct.state == CT_STATE_GAMEOVER ||
            s_ct.state == CT_STATE_WIN) {
            ct_start_game();
            return;
        }
    }

    if (s_ct.state != CT_STATE_PLAYING) return;

    /* Multi-touch events */
    if (e->type == UI_EVENT_TOUCH_DOWN) {
        uint8_t btns = ct_hit_test(e->pos.x, e->pos.y);
        if (e->touch_id < UI_MAX_TOUCH_POINTS) {
            s_touch_btns[e->touch_id] = btns;
        }
        ct_recalc_inputs();
        return;
    }
    if (e->type == UI_EVENT_TOUCH_MOVE) {
        uint8_t btns = ct_hit_test(e->pos.x, e->pos.y);
        if (e->touch_id < UI_MAX_TOUCH_POINTS) {
            s_touch_btns[e->touch_id] = btns;
        }
        ct_recalc_inputs();
        return;
    }
    if (e->type == UI_EVENT_TOUCH_UP) {
        if (e->touch_id < UI_MAX_TOUCH_POINTS) {
            s_touch_btns[e->touch_id] = 0;
        }
        ct_recalc_inputs();
        return;
    }

    /* Legacy single-touch fallback (for non-multi-touch events) */
    if (e->type == UI_EVENT_PRESS || e->type == UI_EVENT_DRAG) {
        uint8_t btns = ct_hit_test(e->pos.x, e->pos.y);
        /* Use slot 0 for legacy single-touch */
        s_touch_btns[0] = btns;
        ct_recalc_inputs();
    } else if (e->type == UI_EVENT_RELEASE) {
        s_touch_btns[0] = 0;
        ct_recalc_inputs();
    } else if (e->type == UI_EVENT_KEY_UP) {
        s_ct.input_up = true;
    } else if (e->type == UI_EVENT_KEY_DOWN) {
        s_ct.input_down = true;
    } else if (e->type == UI_EVENT_KEY_LEFT) {
        s_ct.input_left = true;
    } else if (e->type == UI_EVENT_KEY_RIGHT) {
        s_ct.input_right = true;
    } else if (e->type == UI_EVENT_KEY_OK) {
        s_ct.input_jump = true;
        s_ct.input_shoot = true;
    } else if (e->type == UI_EVENT_KEY_BACK) {
        /* Back key: release all */
        memset(s_touch_btns, 0, sizeof(s_touch_btns));
        s_ct.input_up = false;
        s_ct.input_down = false;
        s_ct.input_left = false;
        s_ct.input_right = false;
        s_ct.input_jump = false;
        s_ct.input_shoot = false;
    }
}

/* D-pad button events (for HOLD support) */
static void ct_btn_up_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_PRESS) s_ct.input_up = true;
    else if (e->type == UI_EVENT_RELEASE) s_ct.input_up = false;
}
static void ct_btn_down_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_PRESS) s_ct.input_down = true;
    else if (e->type == UI_EVENT_RELEASE) s_ct.input_down = false;
}
static void ct_btn_left_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_PRESS) s_ct.input_left = true;
    else if (e->type == UI_EVENT_RELEASE) s_ct.input_left = false;
}
static void ct_btn_right_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_PRESS) s_ct.input_right = true;
    else if (e->type == UI_EVENT_RELEASE) s_ct.input_right = false;
}
static void ct_btn_jump_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_PRESS) s_ct.input_jump = true;
    else if (e->type == UI_EVENT_RELEASE) s_ct.input_jump = false;
}
static void ct_btn_shoot_event(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_PRESS) s_ct.input_shoot = true;
    else if (e->type == UI_EVENT_RELEASE) s_ct.input_shoot = false;
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

/* UI rect overlap check for partial refresh */
static bool ct_rects_overlap_ui(const ui_rect_t *a, const ui_rect_t *b)
{
    return (a->x < b->x + b->w && a->x + a->w > b->x &&
            a->y < b->y + b->h && a->y + a->h > b->y);
}

/* Draw only the HUD text (without background) */
static void ct_draw_hud_text(void)
{
    ui_rect_t lives_r = {10, CT_AREA_Y + 4, 100, CT_HUD_H - 8};
    char lives_buf[32];
    int li = 0;
    const char *lp = "LIVES:";
    while (*lp && li < 25) lives_buf[li++] = *lp++;
    const char *lv = s_ct.buf_lives;
    while (*lv && li < 31) lives_buf[li++] = *lv++;
    lives_buf[li] = '\0';
    ui_draw_text_in_rect(&lives_r, lives_buf, &font_montserrat_12, UI_COLOR_WHITE, 0);

    ui_rect_t score_r = {200, CT_AREA_Y + 4, 120, CT_HUD_H - 8};
    char score_buf[32];
    int si = 0;
    const char *sp = "SCORE:";
    while (*sp && si < 25) score_buf[si++] = *sp++;
    const char *sv = s_ct.buf_score;
    while (*sv && si < 31) score_buf[si++] = *sv++;
    score_buf[si] = '\0';
    ui_draw_text_in_rect(&score_r, score_buf, &font_montserrat_12, UI_COLOR_WHITE, 0);
}

/* Draw entities that overlap with the given clip rect (for partial refresh) */
static void ct_draw_entities_in_clip(const ui_rect_t *clip)
{
    /* Mountains in background (parallax) - must draw before platforms */
    int16_t m_offset = s_ct.camera_x / 3;
    for (int i = 0; i < 8; i++) {
        int16_t mx = i * 200 - (m_offset % 200);
        int16_t mh = 40 + (i * 37) % 60;
        ui_rect_t mr = {mx, CT_AREA_Y + CT_GROUND_Y - mh - 20, 120, mh};
        if (!ct_rects_overlap_ui(&mr, clip)) continue;
        ui_draw_fill_rect(&mr, UI_HEX(0x1F3044));
    }

    /* Stars / decoration */
    for (int i = 0; i < 30; i++) {
        int16_t sx = (i * 137 + 50) % CT_AREA_W;
        int16_t sy = (i * 89 + 30) % (CT_AREA_H - 100) + CT_AREA_Y;
        ui_rect_t sr = {sx, sy, 1, 1};
        if (!ct_rects_overlap_ui(&sr, clip)) continue;
        ui_draw_pixel(sx, sy, UI_HEX(0x555577));
    }

    /* Platforms */
    for (int i = 0; i < s_ct.platform_count; i++) {
        ct_platform_t *p = &s_ct.platforms[i];
        int16_t sx = p->x - s_ct.camera_x;
        int16_t sy = p->y + CT_AREA_Y;
        ui_rect_t pr = {sx, sy, p->w, p->h};
        if (!ct_rects_overlap_ui(&pr, clip)) continue;
        ct_draw_world_rect(p->x, p->y, p->w, p->h, CT_PLATFORM_COLOR);
        ct_draw_hline(p->x, p->y, p->w, CT_PLATFORM_TOP);
    }

    /* Enemies */
    for (int i = 0; i < CT_MAX_ENEMIES; i++) {
        ct_enemy_t *e = &s_ct.enemies[i];
        if (!e->active) continue;
        int16_t ew = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_W : CT_ENEMY_W;
        int16_t eh = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_H : CT_ENEMY_H;
        int16_t sx = e->x - s_ct.camera_x;
        int16_t sy = e->y + CT_AREA_Y;
        ui_rect_t er = {sx - 6, sy - 8, ew + 12, eh + 8};
        if (!ct_rects_overlap_ui(&er, clip)) continue;

        ui_color_t body_color = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_COLOR : CT_ENEMY_COLOR;
        ui_color_t head_color = (e->type == CT_ENEMY_BOSS) ? CT_BOSS_HEAD : CT_ENEMY_HEAD;
        ct_draw_world_rect(e->x, e->y, ew, eh, body_color);

        int16_t head_w = ew * 2 / 3;
        int16_t head_h = eh / 4;
        int16_t head_x = e->x + (ew - head_w) / 2;
        ct_draw_world_rect(head_x, e->y, head_w, head_h, head_color);

        if (e->type == CT_ENEMY_BOSS) {
            int16_t bar_w = ew;
            int16_t bar_h = 4;
            ct_draw_world_rect(e->x, e->y - 8, bar_w, bar_h, UI_HEX(0x333333));
            int16_t fill_w = (int16_t)((int32_t)bar_w * e->hp / CT_BOSS_HP);
            ct_draw_world_rect(e->x, e->y - 8, fill_w, bar_h, UI_HEX(0xF44336));
        }

        int16_t gun_y = e->y + eh / 3;
        if (e->facing_right) {
            ct_draw_world_rect(e->x + ew, gun_y, 6, 3, UI_HEX(0x757575));
        } else {
            ct_draw_world_rect(e->x - 6, gun_y, 6, 3, UI_HEX(0x757575));
        }
    }

    /* Player */
    if (s_ct.state == CT_STATE_PLAYING || s_ct.state == CT_STATE_DEAD) {
        bool visible = (s_ct.invincible <= 0) || ((s_ct.invincible / 4) % 2 == 0);
        if (visible) {
            int16_t ph = s_ct.crouching ? CT_PLAYER_CROUCH_H : CT_PLAYER_H;
            int16_t sx = s_ct.px - s_ct.camera_x;
            int16_t sy = s_ct.py + CT_AREA_Y;
            ui_rect_t pr = {sx - 8, sy - 8, CT_PLAYER_W + 16, ph + 8};
            if (ct_rects_overlap_ui(&pr, clip)) {
                ct_draw_world_rect(s_ct.px, s_ct.py, CT_PLAYER_W, ph, CT_PLAYER_COLOR);

                int16_t head_w = CT_PLAYER_W * 2 / 3;
                int16_t head_h = CT_PLAYER_H / 5;
                int16_t head_x = s_ct.px + (CT_PLAYER_W - head_w) / 2;
                ct_draw_world_rect(head_x, s_ct.py, head_w, head_h, CT_PLAYER_HEAD);

                int16_t gun_y = s_ct.py + ph / 3;
                if (s_ct.facing_right) {
                    ct_draw_world_rect(s_ct.px + CT_PLAYER_W, gun_y, 8, 3, CT_PLAYER_GUN);
                } else {
                    ct_draw_world_rect(s_ct.px - 8, gun_y, 8, 3, CT_PLAYER_GUN);
                }

                if (s_ct.input_up && !s_ct.crouching) {
                    int16_t gun_x = s_ct.px + CT_PLAYER_W / 2 - 1;
                    ct_draw_world_rect(gun_x, s_ct.py - 8, 3, 8, CT_PLAYER_GUN);
                }
            }
        }
    }

    /* Bullets */
    for (int i = 0; i < CT_MAX_BULLETS; i++) {
        ct_bullet_t *b = &s_ct.bullets[i];
        if (!b->active) continue;
        int16_t bw = b->from_player ? CT_BULLET_W : CT_E_BULLET_W;
        int16_t bh = b->from_player ? CT_BULLET_H : CT_E_BULLET_H;
        int16_t sx = b->x - s_ct.camera_x;
        int16_t sy = b->y + CT_AREA_Y;
        ui_rect_t br = {sx, sy, bw, bh};
        if (!ct_rects_overlap_ui(&br, clip)) continue;

        if (b->from_player) {
            ct_draw_world_rect(b->x, b->y, CT_BULLET_W, CT_BULLET_H, CT_BULLET_COLOR);
        } else {
            ct_draw_world_rect(b->x, b->y, CT_E_BULLET_W, CT_E_BULLET_H, CT_E_BULLET_COLOR);
        }
    }

    /* Particles */
    for (int i = 0; i < CT_MAX_PARTICLES; i++) {
        ct_particle_t *p = &s_ct.particles[i];
        if (!p->active) continue;
        int16_t sx = p->x - s_ct.camera_x;
        int16_t sy = p->y + CT_AREA_Y;
        ui_rect_t pr = {sx, sy, 5, 5};
        if (!ct_rects_overlap_ui(&pr, clip)) continue;
        ct_draw_world_rect(p->x, p->y, 3, 3, CT_PARTICLE_COLOR);
    }

    /* Control overlays */
    /* D-pad */
    ui_color_t dpad_c = ui_color_blend(CT_CTRL_BG, CT_BG_SKY, CT_CTRL_ALPHA);
    ui_color_t dpad_up_c = s_ct.input_up ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t dpad_dn_c = s_ct.input_down ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t dpad_lt_c = s_ct.input_left ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t dpad_rt_c = s_ct.input_right ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;

    ui_rect_t ur = {CT_DPAD_UP_X, CT_DPAD_UP_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    if (ct_rects_overlap_ui(&ur, clip)) {
        ui_draw_fill_round_rect(&ur, 6, dpad_up_c);
        ui_draw_text_in_rect(&ur, "^", &font_montserrat_16, UI_COLOR_WHITE, 1);
    }

    ui_rect_t dr = {CT_DPAD_DOWN_X, CT_DPAD_DOWN_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    if (ct_rects_overlap_ui(&dr, clip)) {
        ui_draw_fill_round_rect(&dr, 6, dpad_dn_c);
        ui_draw_text_in_rect(&dr, "v", &font_montserrat_16, UI_COLOR_WHITE, 1);
    }

    ui_rect_t lr = {CT_DPAD_LEFT_X, CT_DPAD_LEFT_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    if (ct_rects_overlap_ui(&lr, clip)) {
        ui_draw_fill_round_rect(&lr, 6, dpad_lt_c);
        ui_draw_text_in_rect(&lr, "<", &font_montserrat_16, UI_COLOR_WHITE, 1);
    }

    ui_rect_t rr = {CT_DPAD_RIGHT_X, CT_DPAD_RIGHT_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    if (ct_rects_overlap_ui(&rr, clip)) {
        ui_draw_fill_round_rect(&rr, 6, dpad_rt_c);
        ui_draw_text_in_rect(&rr, ">", &font_montserrat_16, UI_COLOR_WHITE, 1);
    }

    /* Action buttons */
    ui_color_t jump_c = s_ct.input_jump ? ui_color_blend(CT_CTRL_BG_PRESSED, CT_BG_SKY, CT_CTRL_ALPHA) : dpad_c;
    ui_color_t shoot_c = s_ct.input_shoot ? ui_color_blend(UI_HEX(0xFF5722), CT_BG_SKY, CT_CTRL_ALPHA) : ui_color_blend(UI_HEX(0xF44336), CT_BG_SKY, CT_CTRL_ALPHA);

    ui_rect_t jr = {CT_ACT_RIGHT_X, CT_ACT_JUMP_Y, CT_ACT_BTN_W, CT_ACT_BTN_H};
    if (ct_rects_overlap_ui(&jr, clip)) {
        ui_draw_fill_round_rect(&jr, 10, jump_c);
        ui_draw_text_in_rect(&jr, "JUMP", &font_montserrat_12, UI_COLOR_WHITE, 1);
    }

    ui_rect_t sr = {CT_ACT_RIGHT_X, CT_ACT_SHOOT_Y, CT_ACT_BTN_W, CT_ACT_BTN_H};
    if (ct_rects_overlap_ui(&sr, clip)) {
        ui_draw_fill_round_rect(&sr, 10, shoot_c);
        ui_draw_text_in_rect(&sr, "FIRE", &font_montserrat_12, UI_COLOR_WHITE, 1);
    }
}

static void ct_game_enter(ui_page_t *page)
{
    (void)page;
    memset(&s_ct, 0, sizeof(s_ct));
    memset(s_touch_btns, 0, sizeof(s_touch_btns));
    s_ct.state = CT_STATE_IDLE;
    s_ct.lives = CT_LIVES;
    s_ct.need_full_redraw = true;
    ct_update_hud_texts();
}

static void ct_game_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)dirty;

    /* Update game logic */
    ct_update();

    /* Full redraw (initial enter, camera scroll, state change) */
    if (s_ct.need_full_redraw) {
        s_ct.need_full_redraw = false;

        /* Draw title bar */
        ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
        ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

        /* Draw title bar widgets (title label + back button) */
        ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
        for (uint16_t j = 0; j < page->widget_count; j++) {
            if (page->widgets[j]) {
                ui_widget_draw(page->widgets[j], &full);
            }
        }

        /* Draw game area */
        if (s_ct.state == CT_STATE_IDLE) {
            ct_draw_game();
            ct_draw_idle_overlay();
        } else if (s_ct.state == CT_STATE_PLAYING) {
            ct_draw_game();
        } else if (s_ct.state == CT_STATE_GAMEOVER) {
            ct_draw_game();
            ct_draw_gameover_overlay();
        } else if (s_ct.state == CT_STATE_WIN) {
            ct_draw_game();
            ct_draw_win_overlay();
        }

        ui_page_clear_dirty();
        return;
    }

    /* Partial refresh: single-pass atomic per dirty region */
    const ui_dirty_list_t *dl = ui_page_get_dirty_list();
    if (dl->count == 0) return;

    if (s_ct.state == CT_STATE_IDLE) {
        /* For idle state, just redraw the idle overlay */
        for (uint16_t i = 0; i < dl->count; i++) {
            const ui_rect_t *d = &dl->regions[i];
            if (d->y < APP_TITLE_BAR_H) continue;
            ui_render_set_clip(d);
            ct_draw_game();
            ct_draw_idle_overlay();
        }
        ui_page_clear_dirty();
        ui_render_reset_clip();
        return;
    }

    ui_rect_t game_area = {CT_AREA_X, CT_AREA_Y, CT_AREA_W, CT_AREA_H};
    ui_rect_t hud_rect = {0, CT_AREA_Y, CT_AREA_W, CT_HUD_H};
    bool hud_needs_redraw = false;

    for (uint16_t i = 0; i < dl->count; i++) {
        const ui_rect_t *d = &dl->regions[i];

        /* Skip title bar region */
        if (d->y + d->h <= APP_TITLE_BAR_H) continue;

        /* Clip to game area */
        ui_rect_t clip = *d;
        if (clip.y < CT_AREA_Y) {
            clip.h -= (CT_AREA_Y - clip.y);
            clip.y = CT_AREA_Y;
        }
        if (clip.w <= 0 || clip.h <= 0) continue;

        /* Erase: fill with sky background */
        ui_render_set_clip(&clip);
        ui_draw_fill_rect(&clip, CT_BG_SKY);

        /* Redraw game entities clipped to this dirty region */
        ui_render_set_clip(&game_area);
        ct_draw_entities_in_clip(&clip);

        /* Check HUD overlap */
        if (ct_rects_overlap_ui(&clip, &hud_rect)) {
            hud_needs_redraw = true;
        }

        /* Gameover/win overlay */
        if (s_ct.state == CT_STATE_GAMEOVER) {
            ct_draw_gameover_overlay();
        } else if (s_ct.state == CT_STATE_WIN) {
            ct_draw_win_overlay();
        }
    }

    /* Single HUD redraw if needed */
    if (hud_needs_redraw) {
        ui_render_set_clip(&hud_rect);
        ui_draw_fill_rect(&hud_rect, CT_HUD_BG);
        ct_draw_hud_text();
    }

    ui_page_clear_dirty();
    ui_render_reset_clip();
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void game_contra_init(void)
{
    ui_app_page_init(&s_game_ct, "Contra", 0x207);
    s_game_ct.page.flags |= UI_PAGE_FLAG_GAME;

    /* Touch area for whole game screen */
    ui_rect_t touch_rect = {0, APP_TITLE_BAR_H, CT_AREA_W, CT_AREA_H};
    ui_widget_init(&s_touch_area, &touch_rect);
    s_touch_area.bg_color = UI_COLOR_TRANSPARENT;
    s_touch_area.event_cb = ct_touch_event;

    /* D-pad buttons */
    ui_rect_t up_r = {CT_DPAD_UP_X, CT_DPAD_UP_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_widget_init(&s_btn_up, &up_r);
    s_btn_up.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_up.event_cb = ct_btn_up_event;

    ui_rect_t dn_r = {CT_DPAD_DOWN_X, CT_DPAD_DOWN_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_widget_init(&s_btn_down, &dn_r);
    s_btn_down.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_down.event_cb = ct_btn_down_event;

    ui_rect_t lt_r = {CT_DPAD_LEFT_X, CT_DPAD_LEFT_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_widget_init(&s_btn_left, &lt_r);
    s_btn_left.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_left.event_cb = ct_btn_left_event;

    ui_rect_t rt_r = {CT_DPAD_RIGHT_X, CT_DPAD_RIGHT_Y, CT_DPAD_BTN, CT_DPAD_BTN};
    ui_widget_init(&s_btn_right, &rt_r);
    s_btn_right.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_right.event_cb = ct_btn_right_event;

    /* Action buttons */
    ui_rect_t jmp_r = {CT_ACT_RIGHT_X, CT_ACT_JUMP_Y, CT_ACT_BTN_W, CT_ACT_BTN_H};
    ui_widget_init(&s_btn_jump, &jmp_r);
    s_btn_jump.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_jump.event_cb = ct_btn_jump_event;

    ui_rect_t sht_r = {CT_ACT_RIGHT_X, CT_ACT_SHOOT_Y, CT_ACT_BTN_W, CT_ACT_BTN_H};
    ui_widget_init(&s_btn_shoot, &sht_r);
    s_btn_shoot.bg_color = UI_COLOR_TRANSPARENT;
    s_btn_shoot.event_cb = ct_btn_shoot_event;

    /* Widget order: high index = high priority for events */
    s_ct_widgets[0] = &s_touch_area;
    s_ct_widgets[1] = &s_btn_up;
    s_ct_widgets[2] = &s_btn_down;
    s_ct_widgets[3] = &s_btn_left;
    s_ct_widgets[4] = &s_btn_right;
    s_ct_widgets[5] = &s_btn_jump;
    s_ct_widgets[6] = &s_btn_shoot;
    s_ct_widgets[7] = (ui_widget_t *)&s_game_ct.lbl_title;
    s_ct_widgets[8] = (ui_widget_t *)&s_game_ct.btn_back;

    ui_page_set_widgets(&s_game_ct.page, s_ct_widgets, 9);
    ui_page_set_callbacks(&s_game_ct.page, ct_game_enter, NULL,
                          ct_game_draw, NULL);
    ui_page_register(&s_game_ct.page);
}

ui_page_t *game_contra_get_page(void)
{
    return &s_game_ct.page;
}
