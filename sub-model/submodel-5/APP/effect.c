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
/*  Speed helper: effective speed (minimum 1 to avoid stall)            */
/* ==================================================================== */
static uint8_t EffectiveSpeed(void)
{
    /* Invert: speed=255 → fastest (1 frame/step),
     *         speed=1   → slowest (255 frames/step),
     *         speed=0   → treated as 1 (slowest). */
    return (s_state.speed == 0 || s_state.speed == 1) ? 255 :
           (256 - s_state.speed);
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
    s_state.speed = 5;

    s_breath_hue = 0;
    s_marquee_head = 0;
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
    s_state.speed = speed;

    /* Reset animation state */
    s_state.frame_counter = 0;
    s_state.direction = 0;

    /* Per-mode reset */
    s_breath_hue = 0;
    s_marquee_head = 0;

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

bool Effect_Update(void)
{
    uint8_t eff_speed = EffectiveSpeed();

    switch (s_state.mode) {
        case RGB_MODE_SOLID:
            RenderSolid();
            break;

        case RGB_MODE_BREATHING:
            RenderBreathing();
            /* Advance animation step based on speed */
            s_state.frame_counter++;
            if (s_state.frame_counter >= eff_speed) {
                s_state.frame_counter = 0;
                StepBreathing();
            }
            break;

        case RGB_MODE_MARQUEE:
            RenderMarquee();
            /* Advance animation step based on speed */
            s_state.frame_counter++;
            if (s_state.frame_counter >= eff_speed) {
                s_state.frame_counter = 0;
                StepMarquee();
            }
            break;

        case RGB_MODE_CUSTOM:
            RenderCustom();
            /* Advance custom frame based on speed */
            if (s_state.custom_frame_count > 1) {
                s_state.frame_counter++;
                if (s_state.frame_counter >= eff_speed) {
                    s_state.frame_counter = 0;
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
