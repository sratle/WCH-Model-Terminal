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
 *  Animation Structure
 *=============================================================================*/

struct ui_anim {
    int32_t start;
    int32_t end;
    int32_t current;
    uint16_t duration_ms;
    uint16_t elapsed_ms;
    bool active;
    ui_anim_update_cb_t update_cb;
    void *user_data;
};

/*=============================================================================
 *  Animation API
 *=============================================================================*/

/* Initialize animation system */
void ui_anim_init(void);

/* Start a new animation */
ui_anim_t* ui_anim_start(int32_t start, int32_t end, uint16_t duration_ms,
                         ui_anim_update_cb_t update_cb, void *user_data);

/* Stop an animation */
void ui_anim_stop(ui_anim_t *anim);

/* Update all active animations (call every frame) */
void ui_anim_tick(uint16_t delta_ms);

/* Check if any animations are active */
bool ui_anim_is_active(void);

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
