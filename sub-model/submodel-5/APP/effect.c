/*********************************************************************
 * File Name          : effect.c
 * Description        : RGB LED effect engine for 7x7 WS2812 matrix.
 *
 * Architecture: frame-based @ 100fps
 *   Effect_Update() is called once per frame. It renders the current
 *   state into the LED buffer and calls WS2812_Refresh() to send.
 *   Speed controls animation stepping:
 *     speed=255 → advance every frame (fastest)
 *     speed=1   → advance every 255 frames (slowest)
 *     speed=0   → treated as speed=1 (slowest)
 *   Render rate is always 100fps regardless of speed.
 *
 * Modes:
 *   0 (Custom):    Frame animation cycling through loaded frames
 *   1 (Solid):     All LEDs same color, brightness via Color_Scale
 *   2 (Breathing): Hue rotation 0→359→0
 *   3 (Marquee):   Bounce 0→48→0 with trail
 *
 * Speed 档位 1~10（协议 V1.6）：8 档 = 10ms/步，每升一档 × 0.8。
 *********************************************************************/

#include "effect.h"

/* ==================================================================== */
/*  Internal State                                                      */
/* ==================================================================== */
static effect_state_t s_state;

/* ---- Breathing state ---- */
static uint16_t s_breath_hue = 0;       /* 0-359 degrees */

/* ---- Marquee state ---- */
static uint8_t  s_marquee_head = 0;     /* current head LED index (0-48) */

/* ---- Custom state ---- */
/* custom_current_frame tracks which frame to display (cycles 0..count-1) */

/* ==================================================================== */
/*  Speed 档位机制（协议 V1.6）                                          */
/*  speed 为档位 1~10：                                                  */
/*    8 档 = 每步 1 帧（10ms/步，原 speed=255 行为）                     */
/*    每升一档，动画步进时间 × 0.8；每降一档 ÷ 0.8                       */
/*  帧周期固定 10ms（100fps），9/10 档快于帧周期，                       */
/*  用 fp8 累加器实现单帧多步。                                          */
/* ==================================================================== */

/* 档位 → 每步帧数（8.8 定点）。非法档位按 8 档处理 */
static const uint16_t s_speed_step_fp8[11] = {
    256,            /* 0: 非法 → 按 8 档 */
    1221, 977, 781, /* 1~3: 47.7ms 38.1ms 30.5ms */
    625, 500, 400,  /* 4~6: 24.4ms 19.5ms 15.6ms */
    320, 256, 205,  /* 7~9: 12.5ms 10.0ms 8.0ms */
    164             /* 10:  6.4ms */
};

static uint16_t SpeedStepFp8(uint8_t speed)
{
    if (speed < 1 || speed > 10)
        speed = 8;
    return s_speed_step_fp8[speed];
}

/* 步进累加器：每帧调用一次，返回本帧应步进的次数（0 或更多） */
static uint8_t SpeedTick(uint32_t *acc, uint8_t speed)
{
    uint16_t step = SpeedStepFp8(speed);
    uint8_t count = 0;

    *acc += 256;
    while (*acc >= step && count < 8) {     /* 单帧最多 8 步，防卡死 */
        *acc -= step;
        count++;
    }
    return count;
}

/* ==================================================================== */
/*  Mode: Solid                                                         */
/* ==================================================================== */
static void RenderSolid(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    Color_ScaleBrightness(&base, s_state.brightness, &out);
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

/* ==================================================================== */
/*  Mode: Breathing                                                     */
/*  Hue bounces 0→359→0 for rainbow color cycling.                     */
/*  Brightness is fixed at user brightness (V = brightness).            */
/*  S = 255 (maximum saturation) for vivid colors.                      */
/*  Speed controls how many frames between each ±1 hue step.            */
/* ==================================================================== */
static void RenderBreathing(void)
{
    hsv_t hsv;
    rgb888_t out;
    uint8_t i;

    hsv.h = s_breath_hue;
    hsv.s = 255;
    hsv.v = s_state.brightness;

    Color_HSVtoRGB(&hsv, &out);
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

static void StepBreathing(void)
{
    if (s_state.direction == 0) {
        /* Forward: hue increasing */
        if (s_breath_hue >= 359) {
            s_state.direction = 1; /* switch to backward */
        } else {
            s_breath_hue++;
        }
    } else {
        /* Backward: hue decreasing */
        if (s_breath_hue == 0) {
            s_state.direction = 0; /* switch to forward */
        } else {
            s_breath_hue--;
        }
    }
}

/* ==================================================================== */
/*  Mode: Marquee                                                       */
/*  Head bounces 0→48→0 with a trail of MARQUEE_TRAIL_LEN LEDs.        */
/*  Non-head/non-trail LEDs are completely off (0,0,0).                */
/*  Speed controls how many frames between each ±1 head step.           */
/* ==================================================================== */
static void RenderMarquee(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        /* Calculate distance from head (wrap-around for trail) */
        uint16_t dist;

        if (s_state.direction == 0) {
            /* Forward: trail is behind (lower indices) */
            if (i <= s_marquee_head) {
                dist = s_marquee_head - i;
            } else {
                dist = (uint16_t)(WS2812_LED_COUNT - i) + s_marquee_head;
            }
        } else {
            /* Backward: trail is behind (higher indices) */
            if (i >= s_marquee_head) {
                dist = i - s_marquee_head;
            } else {
                dist = (uint16_t)(WS2812_LED_COUNT - s_marquee_head) + i;
            }
        }

        if (dist == 0) {
            /* Head LED: full brightness */
            Color_ScaleBrightness(&base, s_state.brightness, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        } else if (dist <= MARQUEE_TRAIL_LEN) {
            /* Trail LED: brightness fades linearly */
            uint8_t fade = (uint8_t)((uint16_t)s_state.brightness *
                                     (MARQUEE_TRAIL_LEN + 1 - dist) /
                                     (MARQUEE_TRAIL_LEN + 1));
            Color_ScaleBrightness(&base, fade, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        } else {
            /* Background: completely off */
            WS2812_SetPixel(i, 0, 0, 0);
        }
    }
}

static void StepMarquee(void)
{
    if (s_state.direction == 0) {
        /* Forward: head increasing */
        if (s_marquee_head >= WS2812_LED_COUNT - 1) {
            s_state.direction = 1; /* switch to backward */
        } else {
            s_marquee_head++;
        }
    } else {
        /* Backward: head decreasing */
        if (s_marquee_head == 0) {
            s_state.direction = 0; /* switch to forward */
        } else {
            s_marquee_head--;
        }
    }
}

/* ==================================================================== */
/*  One-shot: Ripple (center → edge, 播放一次)                           */
/*  波前相位 s_shot_phase 从中心向外推进一次：                           */
/*    每颗 LED 按到中心的距离平方 d2 (0~18) 分环，相位 == d2 时最亮，    */
/*    尾迹 RIPPLE_TRAIL_LEN 步内线性衰减。                               */
/*    相位到达末尾后动画结束，回到原模式渲染。                           */
/* ==================================================================== */

/* One-shot state */
#define ONESHOT_TYPE_RIPPLE 0
#define ONESHOT_TYPE_WAVE   1

static uint8_t  s_shot_active = 0;
static uint8_t  s_shot_type = 0;
static uint8_t  s_shot_direction = 0;
static uint8_t  s_shot_speed = 0;
static uint32_t s_shot_frame_counter = 0;
static uint8_t  s_shot_phase = 0;

/* 7x7 矩阵中心 (3,3)，LED i 的 dx²+dy²（取值 0~18） */
static uint8_t RippleD2(uint8_t i)
{
    int8_t dx = (int8_t)(i % 7) - 3;
    int8_t dy = (int8_t)(i / 7) - 3;
    return (uint8_t)(dx * dx + dy * dy);
}

static void RenderShotRipple(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        uint8_t d2 = RippleD2(i);

        if (s_shot_phase >= d2 && (s_shot_phase - d2) <= RIPPLE_TRAIL_LEN) {
            uint8_t fade = (uint8_t)((uint16_t)s_state.brightness *
                                     (RIPPLE_TRAIL_LEN + 1 - (s_shot_phase - d2)) /
                                     (RIPPLE_TRAIL_LEN + 1));
            Color_ScaleBrightness(&base, fade, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        } else {
            WS2812_SetPixel(i, 0, 0, 0);
        }
    }
}

/* ==================================================================== */
/*  One-shot: Edge Wave (one side → opposite side, 播放一次)             */
/* ==================================================================== */

/* 取 LED i 在当前波浪方向上的"相位坐标"（0~6） */
static uint8_t WavePos(uint8_t i)
{
    uint8_t x = i % 7;
    uint8_t y = i / 7;

    switch (s_shot_direction) {
        case WAVE_DIR_L2R: return x;
        case WAVE_DIR_R2L: return (uint8_t)(6 - x);
        case WAVE_DIR_T2B: return y;
        case WAVE_DIR_B2T: return (uint8_t)(6 - y);
        default:           return x;
    }
}

static void RenderShotWave(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        uint8_t pos = WavePos(i);

        if (s_shot_phase >= pos && (s_shot_phase - pos) <= WAVE_TRAIL_LEN) {
            uint8_t fade = (uint8_t)((uint16_t)s_state.brightness *
                                     (WAVE_TRAIL_LEN + 1 - (s_shot_phase - pos)) /
                                     (WAVE_TRAIL_LEN + 1));
            Color_ScaleBrightness(&base, fade, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        } else {
            WS2812_SetPixel(i, 0, 0, 0);
        }
    }
}

/* One-shot 帧推进；返回 1=仍在播放，0=本帧结束 */
static uint8_t StepOneShot(void)
{
    uint8_t max_phase;
    uint8_t steps = SpeedTick(&s_shot_frame_counter, s_shot_speed);

    if (steps == 0)
        return 1;

    max_phase = (s_shot_type == ONESHOT_TYPE_RIPPLE)
                ? (18 + RIPPLE_TRAIL_LEN)
                : (6 + WAVE_TRAIL_LEN);

    while (steps-- > 0) {
        s_shot_phase++;
        if (s_shot_phase > max_phase) {
            /* 动画结束：回到原模式渲染 */
            s_shot_active = 0;
            return 0;
        }
    }
    return 1;
}

/* ==================================================================== */
/*  Mode: Custom - Cycle through loaded frames                          */
/*  speed=255 → advance every RGB refresh (fastest)                    */
/*  speed=1   → advance every 255 RGB refreshes (slowest)              */
/*  Brightness applied via Color_ScaleBrightness.                      */
/* ==================================================================== */
static void RenderCustom(void)
{
    uint8_t i;
    uint8_t fi = s_state.custom_current_frame;

    if (s_state.custom_frame_count == 0 || fi >= s_state.custom_frame_count) {
        for (i = 0; i < WS2812_LED_COUNT; i++) {
            WS2812_SetPixel(i, 0, 0, 0);
        }
        return;
    }

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        const rgb888_t *led_color = &s_state.custom_frames[fi][i];
        rgb888_t out;
        Color_ScaleBrightness(led_color, s_state.brightness, &out);
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void Effect_Init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = RGB_MODE_SOLID;
    s_state.r = 0;
    s_state.g = 0;
    s_state.b = 0;
    s_state.brightness = 0;
    s_state.speed = 8;      /* 默认 8 档（10ms/步） */

    s_breath_hue = 0;
    s_marquee_head = 0;
    s_shot_active = 0;
}

void Effect_SetMode(rgb_mode_t mode, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t brightness, uint8_t speed)
{
    /* If all parameters are identical to current state, skip entirely.
     * This prevents repeated SetMode (e.g. from heartbeat/Config_Apply)
     * from resetting animation state (hue, head, frame_counter). */
    if (s_state.mode == mode &&
        s_state.r == r && s_state.g == g && s_state.b == b &&
        s_state.brightness == brightness && s_state.speed == speed) {
        return;
    }

    s_state.mode = mode;
    s_state.r = r;
    s_state.g = g;
    s_state.b = b;
    s_state.brightness = brightness;
    /* speed 为档位 1~10，非法值按 8 档处理 */
    s_state.speed = (speed < 1 || speed > 10) ? 8 : speed;

    /* Reset animation state */
    s_state.frame_counter = 0;
    s_state.direction = 0;

    /* Per-mode reset */
    s_breath_hue = 0;
    s_marquee_head = 0;
    s_shot_active = 0;      /* 模式切换时取消正在播放的一次性动画 */

    /* For custom mode: do NOT clear custom_frame_count here.
     * Frame data may already be loaded (or will be loaded via SendFrame). */
}

void Effect_SetCustomFrame(uint8_t frame_idx, const uint8_t *data)
{
    if (frame_idx < EFFECT_MAX_CUSTOM_FRAMES) {
        memcpy(s_state.custom_frames[frame_idx], data,
               WS2812_LED_COUNT * 3);
    }
}

void Effect_PlayCustom(uint8_t frame_count, uint16_t frame_interval)
{
    (void)frame_interval; /* ignored: speed is used instead */

    if (frame_count > EFFECT_MAX_CUSTOM_FRAMES) {
        frame_count = EFFECT_MAX_CUSTOM_FRAMES;
    }
    s_state.custom_frame_count = frame_count;
    s_state.custom_current_frame = 0;  /* 从第 0 帧开始播放 */
    s_state.mode = RGB_MODE_CUSTOM;
    s_state.frame_counter = 0;
}

void Effect_TriggerRipple(uint8_t speed)
{
    /* 一次性动画事件：不改变当前模式配置，播完自动回原模式 */
    s_shot_type = ONESHOT_TYPE_RIPPLE;
    s_shot_speed = speed;
    s_shot_frame_counter = 0;
    s_shot_phase = 0;
    s_shot_active = 1;
}

void Effect_TriggerWave(uint8_t direction, uint8_t speed)
{
    if (direction > WAVE_DIR_B2T)
        direction = WAVE_DIR_L2R;

    /* 一次性动画事件：不改变当前模式配置，播完自动回原模式 */
    s_shot_type = ONESHOT_TYPE_WAVE;
    s_shot_direction = direction;
    s_shot_speed = speed;
    s_shot_frame_counter = 0;
    s_shot_phase = 0;
    s_shot_active = 1;
}

uint8_t Effect_IsOneShotActive(void)
{
    return s_shot_active;
}

bool Effect_Update(void)
{
    uint8_t steps;

    /* 一次性动画事件优先：播放期间覆盖模式渲染，播完自动回原模式 */
    if (s_shot_active) {
        if (s_shot_type == ONESHOT_TYPE_RIPPLE)
            RenderShotRipple();
        else
            RenderShotWave();
        StepOneShot();
        s_state.tick++;
        WS2812_Refresh();
        return TRUE;
    }

    steps = SpeedTick(&s_state.frame_counter, s_state.speed);

    switch (s_state.mode) {
        case RGB_MODE_SOLID:
            RenderSolid();
            break;

        case RGB_MODE_BREATHING:
            RenderBreathing();
            while (steps-- > 0)
                StepBreathing();
            break;

        case RGB_MODE_MARQUEE:
            RenderMarquee();
            while (steps-- > 0)
                StepMarquee();
            break;

        case RGB_MODE_CUSTOM:
            RenderCustom();
            /* Advance custom frame based on speed */
            if (s_state.custom_frame_count > 1) {
                while (steps-- > 0) {
                    s_state.custom_current_frame++;
                    if (s_state.custom_current_frame >= s_state.custom_frame_count) {
                        s_state.custom_current_frame = 0;
                    }
                }
            }
            break;

        default:
            RenderSolid();
            break;
    }

    s_state.tick++;
    WS2812_Refresh();
    return TRUE;
}

const effect_state_t *Effect_GetState(void)
{
    return &s_state;
}
