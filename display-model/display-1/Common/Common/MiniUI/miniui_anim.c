/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_anim.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI animation system implementation.
********************************************************************************/
#include "miniui_anim.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Internal State
 *=============================================================================*/

static ui_anim_t s_animations[UI_MAX_ANIMATIONS];

/*=============================================================================
 *  Animation System Implementation
 *=============================================================================*/

void ui_anim_init(void)
{
    printf("[ui_anim_init] start\r\n");
    memset(s_animations, 0, sizeof(s_animations));
    printf("[ui_anim_init] done\r\n");
}

ui_anim_t* ui_anim_start(int32_t start, int32_t end, uint16_t duration_ms,
                         ui_ease_type_t ease, ui_anim_update_cb_t update_cb, void *user_data)
{
    for (uint8_t i = 0; i < UI_MAX_ANIMATIONS; i++) {
        if (!s_animations[i].active) {
            s_animations[i].start = start;
            s_animations[i].end = end;
            s_animations[i].current = start;
            s_animations[i].duration_ms = duration_ms;
            s_animations[i].elapsed_ms = 0;
            s_animations[i].ease_type = ease;
            s_animations[i].active = true;
            s_animations[i].update_cb = update_cb;
            s_animations[i].user_data = user_data;
            return &s_animations[i];
        }
    }
    return NULL;
}

void ui_anim_stop(ui_anim_t *anim)
{
    if (anim) {
        anim->active = false;
    }
}

void ui_anim_tick(uint16_t delta_ms)
{
    for (uint8_t i = 0; i < UI_MAX_ANIMATIONS; i++) {
        ui_anim_t *anim = &s_animations[i];
        if (!anim->active) continue;
        
        anim->elapsed_ms += delta_ms;
        
        if (anim->elapsed_ms >= anim->duration_ms) {
            anim->current = anim->end;
            anim->active = false;
            if (anim->update_cb) {
                anim->update_cb(anim->current, anim->user_data);
            }
        } else {
            float t = (float)anim->elapsed_ms / anim->duration_ms;
            switch (anim->ease_type) {
            case UI_EASE_IN:
                anim->current = ui_ease_in(anim->start, anim->end, t);
                break;
            case UI_EASE_OUT:
                anim->current = ui_ease_out(anim->start, anim->end, t);
                break;
            default:
                anim->current = ui_ease_linear(anim->start, anim->end, t);
                break;
            }
            if (anim->update_cb) {
                anim->update_cb(anim->current, anim->user_data);
            }
        }
    }
}

bool ui_anim_is_active(void)
{
    for (uint8_t i = 0; i < UI_MAX_ANIMATIONS; i++) {
        if (s_animations[i].active) return true;
    }
    return false;
}

/*=============================================================================
 *  Smooth Scroller Implementation
 *=============================================================================*/

static void ui_scroller_step(int32_t value, void *user_data)
{
    ui_scroller_t *s = (ui_scroller_t *)user_data;
    s->pos = value;
    if (value == s->goal) {
        /* Tween finished: the anim slot is deactivated by ui_anim_tick
         * right before this callback — clear our handle so a later
         * ui_anim_stop() can never kill an unrelated animation that
         * reused the freed slot. */
        s->anim = NULL;
    }
    if (s->on_move) s->on_move(value, s->user_data);
}

void ui_scroller_init(ui_scroller_t *s, int32_t pos, ui_anim_update_cb_t on_move, void *user_data)
{
    if (!s) return;
    s->pos = pos;
    s->goal = pos;
    s->anim = NULL;
    s->on_move = on_move;
    s->user_data = user_data;
}

void ui_scroller_set_goal(ui_scroller_t *s, int32_t goal)
{
    if (!s) return;
    s->goal = goal;
    if (s->pos == goal) {
        if (s->anim) { ui_anim_stop(s->anim); s->anim = NULL; }
        return;
    }
    if (s->anim) ui_anim_stop(s->anim);
    s->anim = ui_anim_start(s->pos, goal, UI_SCROLLER_DURATION_MS, UI_EASE_OUT,
                            ui_scroller_step, s);
    if (!s->anim) {
        /* Animation slots exhausted: jump directly to avoid a stuck view */
        s->pos = goal;
        if (s->on_move) s->on_move(goal, s->user_data);
    }
}

void ui_scroller_jump(ui_scroller_t *s, int32_t pos)
{
    if (!s) return;
    if (s->anim) { ui_anim_stop(s->anim); s->anim = NULL; }
    s->goal = pos;
    if (s->pos != pos) {
        s->pos = pos;
        if (s->on_move) s->on_move(pos, s->user_data);
    }
}

bool ui_scroller_animating(const ui_scroller_t *s)
{
    return s && s->anim != NULL;
}

/*=============================================================================
 *  Easing Functions
 *=============================================================================*/

int32_t ui_ease_linear(int32_t start, int32_t end, float t)
{
    if (t <= 0.0f) return start;
    if (t >= 1.0f) return end;
    return start + (int32_t)((end - start) * t);
}

int32_t ui_ease_out(int32_t start, int32_t end, float t)
{
    if (t <= 0.0f) return start;
    if (t >= 1.0f) return end;
    /* Ease-out quadratic: t = 1 - (1-t)^2 */
    float t2 = 1.0f - (1.0f - t) * (1.0f - t);
    return start + (int32_t)((end - start) * t2);
}

int32_t ui_ease_in(int32_t start, int32_t end, float t)
{
    if (t <= 0.0f) return start;
    if (t >= 1.0f) return end;
    /* Ease-in quadratic: t = t^2 */
    float t2 = t * t;
    return start + (int32_t)((end - start) * t2);
}
