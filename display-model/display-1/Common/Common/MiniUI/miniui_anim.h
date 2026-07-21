/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_anim.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI animation system API.
*                      Simple linear and ease-out interpolation.
********************************************************************************/
#ifndef __MINIUI_ANIM_H
#define __MINIUI_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"

/*=============================================================================
 *  Animation Configuration
 *=============================================================================*/

#define UI_MAX_ANIMATIONS   8

/*=============================================================================
 *  Easing Type
 *=============================================================================*/

typedef enum {
    UI_EASE_LINEAR,
    UI_EASE_OUT,
    UI_EASE_IN,
} ui_ease_type_t;

/*=============================================================================
 *  Animation Structure
 *=============================================================================*/

struct ui_anim {
    int32_t start;
    int32_t end;
    int32_t current;
    uint16_t duration_ms;
    uint16_t elapsed_ms;
    ui_ease_type_t ease_type;
    bool active;
    ui_anim_update_cb_t update_cb;
    void *user_data;
};

/*=============================================================================
 *  Animation API
 *=============================================================================*/

/* Initialize animation system */
void ui_anim_init(void);

/* Start a new animation with specified easing */
ui_anim_t* ui_anim_start(int32_t start, int32_t end, uint16_t duration_ms,
                         ui_ease_type_t ease, ui_anim_update_cb_t update_cb, void *user_data);

/* Stop an animation */
void ui_anim_stop(ui_anim_t *anim);

/* Update all active animations (call every frame) */
void ui_anim_tick(uint16_t delta_ms);

/* Check if any animations are active */
bool ui_anim_is_active(void);

/*=============================================================================
 *  Smooth Scroller
 *
 *  Helper for pixel-level smooth (animated) scrolling of list content.
 *  The app keeps a logical scroll target (goal); the scroller tweens the
 *  visible position toward it with an ease-out animation.  on_move fires
 *  whenever the position changes so the app can invalidate its viewport.
 *
 *  Typical usage:
 *    ui_scroller_init(&s, 0, my_invalidate_cb, NULL);
 *    // wheel handler:  ui_scroller_set_goal(&s, clamp(goal - delta * ITEM_H));
 *    // draw/hit-test: use s.pos as the pixel scroll offset
 *=============================================================================*/

#define UI_SCROLLER_DURATION_MS  120   /* ease-out tween duration per re-target */

typedef struct {
    int32_t pos;        /* current animated position (px) */
    int32_t goal;       /* scroll target (px) */
    ui_anim_t *anim;    /* active tween (NULL when idle) */
    ui_anim_update_cb_t on_move;   /* called after pos changes (invalidate) */
    void *user_data;
} ui_scroller_t;

/* Initialize a scroller at a fixed position */
void ui_scroller_init(ui_scroller_t *s, int32_t pos, ui_anim_update_cb_t on_move, void *user_data);

/* Set a new scroll target; the position tweens toward it (ease-out).
 * Re-targeting mid-animation continues smoothly from the current pos. */
void ui_scroller_set_goal(ui_scroller_t *s, int32_t goal);

/* Jump directly to a position without animation */
void ui_scroller_jump(ui_scroller_t *s, int32_t pos);

/* True while the position is still animating toward the goal */
bool ui_scroller_animating(const ui_scroller_t *s);

/*=============================================================================
 *  Easing Functions
 *=============================================================================*/

/* Linear interpolation: t goes from 0.0 to 1.0 */
int32_t ui_ease_linear(int32_t start, int32_t end, float t);

/* Ease-out: deceleration */
int32_t ui_ease_out(int32_t start, int32_t end, float t);

/* Ease-in: acceleration */
int32_t ui_ease_in(int32_t start, int32_t end, float t);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_ANIM_H */
