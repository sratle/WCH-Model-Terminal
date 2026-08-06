/*********************************************************************
 * File Name          : effect.h
 * Description        : RGB LED effect engine for 7x7 WS2812 matrix.
 *                      Supports 4 modes: custom, solid, breathing,
 *                      marquee. HSV color space is used internally
 *                      for smooth transitions and brightness control.
 *
 *                      Speed 档位机制（协议 V1.6）：
 *                        speed 为档位 1~10，8 档 = 每步 1 帧（10ms/步），
 *                        每升一档步进时间 × 0.8，非法值按 8 档处理。
 *                        帧周期固定 10ms（100fps），9/10 档单帧多步。
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
/*  One-shot Animations (事件性动画，播放一次后回到原模式)                */
/* ==================================================================== */
#define RIPPLE_TRAIL_LEN    4   /* 波纹尾迹长度（亮度衰减步数） */

#define WAVE_TRAIL_LEN      3   /* 波浪尾迹长度 */

/* Edge wave directions */
#define WAVE_DIR_L2R        0x00    /* 左 → 右 */
#define WAVE_DIR_R2L        0x01    /* 右 → 左 */
#define WAVE_DIR_T2B        0x02    /* 上 → 下 */
#define WAVE_DIR_B2T        0x03    /* 下 → 上 */

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
    uint8_t     speed;          /* 速度档位 1~10（8 档 = 10ms/步，升档 × 0.8） */

    /* fp8 步进累加器：每帧 += 256，达到档位阈值时步进动画 */
    uint32_t    frame_counter;

    /* Direction flag for bounce animations (breathing hue, marquee head).
     * 0 = forward (increasing), 1 = backward (decreasing). */
    uint8_t     direction;

    /* Internal animation state */
    uint32_t    tick;           /* Frame counter (incremented each update) */

    /* Custom frame data: each frame stores per-LED RGB888 color.
     * Global brightness is applied during rendering via HSV (S forced to max). */
    uint8_t     custom_frame_count;       /* Total loaded frames */
    uint8_t     custom_current_frame;     /* Current display frame index (cycles 0..count-1) */
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
 * @param  speed      - 速度档位 1~10（非法值按 8 档处理）
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
 * @brief  Trigger a one-shot center ripple animation.
 *         一次性动画事件：亮度波纹从中心扩散到边缘一次后结束，
 *         自动回到原模式渲染。颜色/基准亮度沿用模式1配置的 r/g/b。
 * @param  speed - Animation speed (same semantics as Effect_SetMode)
 */
void Effect_TriggerRipple(uint8_t speed);

/**
 * @brief  Trigger a one-shot edge wave animation.
 *         一次性动画事件：亮度波浪从一侧传播到另一侧一次后结束，
 *         自动回到原模式渲染。颜色/基准亮度沿用模式1配置的 r/g/b。
 * @param  direction - WAVE_DIR_L2R / R2L / T2B / B2T
 * @param  speed     - Animation speed (same semantics as Effect_SetMode)
 */
void Effect_TriggerWave(uint8_t direction, uint8_t speed);

/**
 * @brief  Query whether a one-shot animation is currently playing.
 */
uint8_t Effect_IsOneShotActive(void);

/**
 * @brief  Get the current effect state (for status queries).
 */
const effect_state_t *Effect_GetState(void);

#ifdef __cplusplus
}
#endif

#endif /* __EFFECT_H */
