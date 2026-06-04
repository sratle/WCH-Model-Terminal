/*********************************************************************
 * File Name          : effect.h
 * Description        : RGB LED effect engine for 7x7 WS2812 matrix.
 *                      Supports 4 modes: custom, solid, breathing,
 *                      marquee. HSV color space is used internally
 *                      for smooth transitions and brightness control.
 *
 *                      Speed mechanism (frame-based, unified):
 *                        speed=N means animation advances every N frames.
 *                        speed=0 is treated as speed=1.
 *                        At ~60fps: speed=1 → 60 steps/s, speed=60 → 1 step/s.
 *********************************************************************/

#ifndef __EFFECT_H
#define __EFFECT_H

#include "CH58x_common.h"
#include "color.h"
#include "ws2812.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/*  Effect Modes                                                        */
/* ==================================================================== */
typedef enum {
    RGB_MODE_CUSTOM   = 0x00,   /* Custom frame animation from Core */
    RGB_MODE_SOLID    = 0x01,   /* All LEDs same color */
    RGB_MODE_BREATHING = 0x02,  /* Hue rotation 0→359→0 */
    RGB_MODE_MARQUEE  = 0x03,   /* Bounce running light */
} rgb_mode_t;

/* ==================================================================== */
/*  Custom Frame Animation                                              */
/* ==================================================================== */
#define EFFECT_MAX_CUSTOM_FRAMES    20

/* ==================================================================== */
/*  Marquee Configuration                                               */
/* ==================================================================== */
#define MARQUEE_TRAIL_LEN   5   /* Number of trail LEDs behind head */

/* ==================================================================== */
/*  Effect State                                                        */
/* ==================================================================== */
typedef struct {
    rgb_mode_t  mode;
    uint8_t     r;              /* Base color R (RGB888) */
    uint8_t     g;              /* Base color G */
    uint8_t     b;              /* Base color B */
    uint8_t     brightness;     /* Global brightness 0-255 */
    uint8_t     speed;          /* Animation speed: advance every N frames (0=1) */

    /* Frame counter: incremented each Effect_Update() call.
     * When frame_counter >= effective_speed, animation advances one step
     * and frame_counter is reset to 0. */
    uint32_t    frame_counter;

    /* Direction flag for bounce animations (breathing hue, marquee head).
     * 0 = forward (increasing), 1 = backward (decreasing). */
    uint8_t     direction;

    /* Internal animation state */
    uint32_t    tick;           /* Frame counter (incremented each update) */

    /* Custom frame data: each frame stores per-LED RGB888 color.
     * Global brightness is applied during rendering via HSV (S forced to max). */
    uint8_t     custom_frame_count;
    rgb888_t    custom_frames[EFFECT_MAX_CUSTOM_FRAMES][WS2812_LED_COUNT];
} effect_state_t;

/* ==================================================================== */
/*  API Functions                                                       */
/* ==================================================================== */

/**
 * @brief  Initialize effect engine to default state (all off).
 */
void Effect_Init(void);

/**
 * @brief  Set the effect mode with parameters.
 * @param  mode       - RGB mode (0-3)
 * @param  r, g, b    - Base color (RGB888)
 * @param  brightness - Global brightness (0-255)
 * @param  speed      - Animation speed: advance every N frames (0 treated as 1)
 */
void Effect_SetMode(rgb_mode_t mode, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t brightness, uint8_t speed);

/**
 * @brief  Upload a custom animation frame.
 * @param  frame_idx - Frame index (0 to EFFECT_MAX_CUSTOM_FRAMES-1)
 * @param  data      - Array of 49*3 bytes RGB888 data (R,G,B per LED)
 */
void Effect_SetCustomFrame(uint8_t frame_idx, const uint8_t *data);

/**
 * @brief  Configure custom animation playback.
 * @param  frame_count   - Total number of frames
 * @param  frame_interval - Ignored (speed is used instead), kept for protocol compat
 */
void Effect_PlayCustom(uint8_t frame_count, uint16_t frame_interval);

/**
 * @brief  Update the effect engine (call in main loop).
 *         Computes current frame colors and writes to WS2812 buffer.
 *         Animation speed is controlled by frame_counter vs speed.
 * @return TRUE if WS2812 was refreshed, FALSE if no update needed.
 */
bool Effect_Update(void);

/**
 * @brief  Get the current effect state (for status queries).
 */
const effect_state_t *Effect_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_H */
