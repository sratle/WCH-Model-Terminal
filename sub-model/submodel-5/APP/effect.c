/*********************************************************************
 * File Name          : effect.c
 * Description        : RGB LED effect engine for 7x7 WS2812 matrix.
 *
 * Design principles:
 *   1. Each mode has its own state variables for clean control
 *   2. Speed mapping: quadratic for natural feel
 *      - Breathing: controls hue rotation speed + breath rate
 *      - Marquee: controls head movement speed (1 LED at a time)
 *      - Custom: frame_interval controls playback, speed ignored
 *   3. Frame rate is fixed ~60fps (controlled by App_UpdateEffect)
 *
 * Speed → timing mapping (at 60fps):
 *   Breathing (hue rotation period):
 *     speed 0   → ~30s per full hue cycle
 *     speed 50  → ~8s
 *     speed 128 → ~3s
 *     speed 200 → ~1.5s
 *     speed 255 → ~0.8s
 *   Marquee (full rotation through 49 LEDs):
 *     speed 0   → ~25s
 *     speed 50  → ~6s
 *     speed 128 → ~2s
 *     speed 200 → ~0.8s
 *     speed 255 → ~0.4s
 *********************************************************************/

#include "effect.h"

/* ==================================================================== */
/*  Internal State                                                      */
/* ==================================================================== */
static effect_state_t s_state;

/* Dirty flag: set when mode/color parameters change */
static uint8_t s_dirty = 1;

/* ---- Breathing state ---- */
static uint16_t s_breath_hue = 0;       /* 0-359 degrees */
static uint8_t  s_breath_phase = 0;     /* 0-255 sine phase */

/* ---- Marquee state ---- */
static uint16_t s_marquee_acc = 0;      /* accumulator for head advancement */
static uint8_t  s_marquee_head = 0;     /* current head LED index (0-48) */

/* ---- Custom state ---- */
static uint16_t s_custom_timer = 0;     /* ms counter for frame switching */
static uint8_t  s_custom_frame_idx = 0; /* current frame being displayed */

/* ==================================================================== */
/*  Sine LUT for breathing brightness                                    */
/*  256 entries, one full sine cycle, range 30-255                      */
/*  (minimum 30 so LEDs never fully turn off)                           */
/* ==================================================================== */
static const uint8_t s_sine_lut[256] = {
    143,146,149,152,155,158,161,164,167,170,173,176,179,182,185,188,
    191,194,196,199,202,204,207,209,211,214,216,218,220,222,224,226,
    228,229,231,233,234,236,237,238,239,240,241,242,243,244,244,245,
    246,246,247,247,247,248,248,248,248,248,248,248,248,248,247,247,
    247,246,246,245,244,244,243,242,241,240,239,238,237,236,234,233,
    231,229,228,226,224,222,220,218,216,214,211,209,207,204,202,199,
    196,194,191,188,185,182,179,176,173,170,167,164,161,158,155,152,
    149,146,143,140,137,134,131,128,125,122,119,116,113,110,107,104,
    101, 98, 96, 93, 90, 88, 85, 83, 81, 78, 76, 74, 72, 70, 68, 66,
     64, 63, 61, 59, 58, 56, 55, 54, 53, 52, 51, 50, 49, 48, 48, 47,
     46, 46, 45, 45, 45, 44, 44, 44, 44, 44, 44, 44, 44, 44, 45, 45,
     45, 46, 46, 47, 48, 48, 49, 50, 51, 52, 53, 54, 55, 56, 58, 59,
     61, 63, 64, 66, 68, 70, 72, 74, 76, 78, 81, 83, 85, 88, 90, 93,
     96, 98,101,104,107,110,113,116,119,122,125,128,131,134,137,140,
    143,146,149,152,155,158,161,164,167,170,173,176,179,182,185,188,
    191,194,196,199,202,204,207,209,211,214,216,218,220,222,224,226,
};

/* ==================================================================== */
/*  Speed helper: compute per-frame increment for given speed            */
/*  Uses quadratic mapping for natural feel                             */
/*  Returns value in range [min, max]                                    */
/* ==================================================================== */
static uint16_t SpeedToInc(uint8_t speed, uint16_t min_inc, uint16_t max_inc)
{
    /* Quadratic: more resolution at low speeds, wider range at high */
    uint32_t quad = (uint32_t)speed * speed;  /* 0 - 65025 */
    uint16_t range = max_inc - min_inc;
    return min_inc + (uint16_t)(quad * range / 65025U);
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
/*  Hue rotates 0→359° for rainbow color change.                       */
/*  Brightness follows sine wave for smooth breathing.                  */
/*  Speed controls both hue rotation and breath rate.                   */
/* ==================================================================== */
static void RenderBreathing(void)
{
    hsv_t hsv;
    rgb888_t out;
    uint8_t i;

    /* s_breath_hue is 0-3599 (0.1° resolution), convert to 0-359 for HSV */
    hsv.h = s_breath_hue / 10;
    hsv.s = 255;

    /* Brightness from sine LUT, scaled by user brightness */
    uint8_t sine_val = s_sine_lut[s_breath_phase];
    hsv.v = (uint8_t)((uint16_t)sine_val * s_state.brightness / 255);
    if (hsv.v < 2 && s_state.brightness > 2) hsv.v = 2;

    Color_HSVtoRGB(&hsv, &out);
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

static void UpdateBreathing(void)
{
    /* Hue: advance by speed-dependent amount
     * Full hue cycle = 360 degrees
     * At 60fps: speed 0 → inc=1 → 360/1 = 360 frames = 6s per cycle
     *           speed 128 → inc=7 → 360/7 = 51 frames = 0.85s
     *           speed 255 → inc=28 → 360/28 = 13 frames = 0.22s
     * But we want slower rotation, so scale down:
     *   speed 0 → inc=1 → 360 frames = 6s... too fast at low speed
     * Let's use fractional accumulation instead */
    uint16_t hue_inc = SpeedToInc(s_state.speed, 2, 80);
    s_breath_hue = (s_breath_hue + hue_inc) % 3600;  /* 0-3599 for 0.1° resolution */
    /* Actual hue = s_breath_hue / 10 */

    /* Brightness sine: advance by speed-dependent amount
     * One full sine cycle = 256 phase steps
     * At 60fps: speed 0 → inc=1 → 256/1 = 256 frames = 4.3s per breath
     *           speed 128 → inc=10 → 256/10 = 26 frames = 0.43s
     *           speed 255 → inc=40 → 256/40 = 6.4 frames = 0.11s */
    uint16_t breath_inc = SpeedToInc(s_state.speed, 1, 40);
    s_breath_phase += (uint8_t)breath_inc;
}

/* ==================================================================== */
/*  Mode: Marquee                                                       */
/*  Head moves one LED at a time for smooth animation.                  */
/*  Speed controls how many frames between head advancements.           */
/* ==================================================================== */

#define MARQUEE_TRAIL_LEN  10

static void RenderMarquee(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        uint16_t dist;
        if (s_marquee_head >= i) {
            dist = s_marquee_head - i;
        } else {
            dist = (uint16_t)s_marquee_head + WS2812_LED_COUNT - i;
        }

        if (dist == 0) {
            Color_ScaleBrightness(&base, s_state.brightness, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        } else if (dist <= MARQUEE_TRAIL_LEN) {
            uint8_t fade = (uint8_t)((uint16_t)s_state.brightness *
                                     (MARQUEE_TRAIL_LEN + 1 - dist) /
                                     (MARQUEE_TRAIL_LEN + 1));
            Color_ScaleBrightness(&base, fade, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        } else {
            uint8_t bg_bright = (uint8_t)((uint16_t)s_state.brightness * 3 / 32);
            if (bg_bright < 1) bg_bright = 1;
            Color_ScaleBrightness(&base, bg_bright, &out);
            WS2812_SetPixel(i, out.r, out.g, out.b);
        }
    }
}

static void UpdateMarquee(void)
{
    /* Accumulator-based head advancement.
     * Each frame, add speed-dependent amount to accumulator.
     * When accumulator >= threshold, advance head by 1 LED.
     *
     * Threshold = 1000 (arbitrary, gives good resolution)
     * Rate: speed 0 → 40  (advance every 25 frames = 0.42s/LED, full rotation ~20s)
     *        speed 128 → 500 (advance every 2 frames = 33ms/LED, full rotation ~1.6s)
     *        speed 255 → 2500 (advance every frame, ~2.5 LEDs/frame, full rotation ~0.3s)
     */
    uint16_t rate = SpeedToInc(s_state.speed, 40, 2500);
    s_marquee_acc += rate;
    while (s_marquee_acc >= 1000) {
        s_marquee_acc -= 1000;
        s_marquee_head = (s_marquee_head + 1) % WS2812_LED_COUNT;
    }
}

/* ==================================================================== */
/*  Mode: Custom - Play pre-loaded frame animation                      */
/*  Frame switching controlled by frame_interval (ms).                  */
/*  Speed parameter is ignored for custom mode.                         */
/* ==================================================================== */
static void RenderCustom(void)
{
    uint8_t i;

    if (s_state.custom_frame_count == 0) {
        for (i = 0; i < WS2812_LED_COUNT; i++) {
            WS2812_SetPixel(i, 0, 0, 0);
        }
        return;
    }

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        const rgb888_t *led_color = &s_state.custom_frames[s_custom_frame_idx][i];
        rgb888_t out;
        /* Simple brightness scaling - preserve original color, don't force S=255 */
        out.r = (uint8_t)((uint16_t)led_color->r * s_state.brightness / 255);
        out.g = (uint8_t)((uint16_t)led_color->g * s_state.brightness / 255);
        out.b = (uint8_t)((uint16_t)led_color->b * s_state.brightness / 255);
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

static void UpdateCustom(void)
{
    /* s_custom_timer accumulates ~16ms per frame (set by App_UpdateEffect delay) */
    s_custom_timer += 16;  /* approximate ms per frame */
    if (s_custom_timer >= s_state.custom_frame_interval) {
        s_custom_timer = 0;
        s_custom_frame_idx++;
        if (s_custom_frame_idx >= s_state.custom_frame_count) {
            s_custom_frame_idx = 0;
        }
    }
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void Effect_Init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.mode = RGB_MODE_SOLID;
    s_state.r = 255;
    s_state.g = 255;
    s_state.b = 255;
    s_state.brightness = 128;
    s_state.speed = 128;
    s_state.custom_frame_interval = 100;

    s_breath_hue = 0;
    s_breath_phase = 0;
    s_marquee_acc = 0;
    s_marquee_head = 0;
    s_custom_timer = 0;
    s_custom_frame_idx = 0;
}

void Effect_SetMode(rgb_mode_t mode, uint8_t r, uint8_t g, uint8_t b,
                    uint8_t brightness, uint8_t speed)
{
    s_state.mode = mode;
    s_state.r = r;
    s_state.g = g;
    s_state.b = b;
    s_state.brightness = brightness;
    s_state.speed = speed;

    /* Reset per-mode state */
    s_breath_hue = 0;
    s_breath_phase = 0;
    s_marquee_acc = 0;
    s_marquee_head = 0;
    s_custom_timer = 0;
    s_custom_frame_idx = 0;

    s_dirty = 1;
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
    if (frame_count > EFFECT_MAX_CUSTOM_FRAMES) {
        frame_count = EFFECT_MAX_CUSTOM_FRAMES;
    }
    if (frame_interval < EFFECT_MIN_FRAME_INTERVAL) {
        frame_interval = EFFECT_MIN_FRAME_INTERVAL;
    }
    if (frame_interval > EFFECT_MAX_FRAME_INTERVAL) {
        frame_interval = EFFECT_MAX_FRAME_INTERVAL;
    }
    s_state.custom_frame_count = frame_count;
    s_state.custom_frame_interval = frame_interval;
    s_state.mode = RGB_MODE_CUSTOM;
    s_custom_timer = 0;
    s_custom_frame_idx = 0;
}

bool Effect_Update(void)
{
    switch (s_state.mode) {
        case RGB_MODE_SOLID:
            if (!s_dirty) return FALSE;
            RenderSolid();
            s_dirty = 0;
            break;

        case RGB_MODE_BREATHING:
            RenderBreathing();
            UpdateBreathing();
            break;

        case RGB_MODE_MARQUEE:
            RenderMarquee();
            UpdateMarquee();
            break;

        case RGB_MODE_CUSTOM:
            RenderCustom();
            UpdateCustom();
            break;

        default:
            if (!s_dirty) return FALSE;
            RenderSolid();
            s_dirty = 0;
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
