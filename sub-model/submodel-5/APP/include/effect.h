/*********************************************************************
 * File Name          : effect.h
 * Description        : RGB LED effect engine for 7x7 WS2812 matrix.
 *                      Supports 4 modes: custom, solid, breathing,
 *                      marquee. HSV color space is used internally
 *                      for smooth transitions and brightness control.
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
    RGB_MODE_BREATHING = 0x02,  /* Breathing effect */
    RGB_MODE_MARQUEE  = 0x03,   /* Running light (chase) */
} rgb_mode_t;

/* ==================================================================== */
/*  Custom Frame Animation                                              */
/* ==================================================================== */
#define EFFECT_MAX_CUSTOM_FRAMES    20
#define EFFECT_MIN_FRAME_INTERVAL   50      /* ms */
#define EFFECT_MAX_FRAME_INTERVAL   1000    /* ms */

/* ==================================================================== */
/*  Effect State                                                        */
/* ==================================================================== */
typedef struct {
    rgb_mode_t  mode;
    uint8_t     r;              /* Base color R (RGB888) */
    uint8_t     g;              /* Base color G */
    uint8_t     b;              /* Base color B */
    uint8_t     brightness;     /* Global brightness 0-255 */
    uint8_t     speed;          /* Animation speed parameter 0-255 */

    /* Internal animation state */
    uint32_t    tick;           /* Frame counter (incremented each update) */

    /* Custom frame data: each frame stores per-LED RGB888 color.
     * Global brightness is applied during rendering via HSV (S forced to max). */
    uint8_t     custom_frame_count;
    uint16_t    custom_frame_interval;  /* ms */
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
 * @param  speed      - Animation speed (0-255)
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
 * @param  frame_interval - Delay between frames in ms
 */
void Effect_PlayCustom(uint8_t frame_count, uint16_t frame_interval);

/**
 * @brief  Update the effect engine (call periodically, ~30Hz).
 *         Computes current frame colors and writes to WS2812 buffer.
 * @param  now_ms - Current time in milliseconds (from SysTick)
 * @return TRUE if WS2812 was refreshed, FALSE if no update needed.
 */
bool Effect_Update(uint32_t now_ms);

/**
 * @brief  Get the current effect state (for status queries).
 */
const effect_state_t *Effect_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_H */
