/*********************************************************************
 * File Name          : effect.c
 * Description        : RGB LED effect engine implementation.
 *                      HSV color space for smooth animations.
 *                      Modes: custom, solid, breathing, marquee.
 *********************************************************************/

#include "effect.h"

/* ==================================================================== */
/*  Internal State                                                      */
/* ==================================================================== */
static effect_state_t s_state;

/* Animation phase counter */
static uint32_t s_anim_phase = 0;

/* ==================================================================== */
/*  Mode: Solid - All LEDs same color with brightness                   */
/* ==================================================================== */
static void RenderSolid(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    Color_ApplyBrightness(&base, s_state.brightness, &out);
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

/* ==================================================================== */
/*  Mode: Breathing - Sinusoidal brightness modulation                  */
/* ==================================================================== */

/* Triangle wave approximation: input 0-255 -> output 0-254
 * Linear rise from 0 to 127, linear fall from 128 to 255 */
static uint8_t SineHalf(uint8_t x)
{
    if (x < 128) {
        return (x * 2 > 254) ? 254 : x * 2;
    } else {
        uint8_t d = 255 - x;
        return (d * 2 > 254) ? 254 : d * 2;
    }
}

static void RenderBreathing(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t out;
    uint8_t i;

    /* Phase cycles 0-255 over time; speed controls cycle rate */
    uint8_t phase = (uint8_t)(s_anim_phase & 0xFF);

    /* Compute breathing brightness: base brightness * sine modulation */
    uint8_t sine_val = SineHalf(phase);
    uint8_t eff_bright = (uint8_t)((uint16_t)s_state.brightness * sine_val / 255);

    /* Minimum brightness so LEDs don't fully turn off */
    if (eff_bright < 2 && s_state.brightness > 2) {
        eff_bright = 2;
    }

    Color_ApplyBrightness(&base, eff_bright, &out);
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        WS2812_SetPixel(i, out.r, out.g, out.b);
    }
}

/* ==================================================================== */
/*  Mode: Marquee - Running light (chase)                               */
/*  Single LED (or group) lights up and moves around the matrix         */
/* ==================================================================== */
static void RenderMarquee(void)
{
    rgb888_t base = { s_state.r, s_state.g, s_state.b };
    rgb888_t lit, dim;
    uint8_t i;
    uint8_t head;

    /* Apply brightness to lit and dim colors */
    Color_ApplyBrightness(&base, s_state.brightness, &lit);
    Color_ApplyBrightness(&base, s_state.brightness / 8, &dim);

    /* Head position cycles through all LEDs */
    head = (uint8_t)(s_anim_phase % WS2812_LED_COUNT);

    /* Trail: 5 LEDs with decreasing brightness */
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        /* Distance from head (wrapping) */
        int8_t dist = (int8_t)head - (int8_t)i;
        if (dist < 0) dist += WS2812_LED_COUNT;

        if (dist == 0) {
            WS2812_SetPixel(i, lit.r, lit.g, lit.b);
        } else if (dist <= 4) {
            /* Trail with fading brightness */
            uint8_t trail_bright = (uint8_t)((uint16_t)s_state.brightness * (5 - dist) / 40);
            rgb888_t trail_out;
            Color_ApplyBrightness(&base, trail_bright, &trail_out);
            WS2812_SetPixel(i, trail_out.r, trail_out.g, trail_out.b);
        } else {
            WS2812_SetPixel(i, dim.r, dim.g, dim.b);
        }
    }
}

/* ==================================================================== */
/*  Mode: Custom - Play pre-loaded frame animation                      */
/*  Each frame stores per-LED RGB888 color. Global brightness is        */
/*  applied via HSV with saturation forced to maximum (255).            */
/* ==================================================================== */
static void RenderCustom(void)
{
    uint8_t frame_idx;
    uint8_t i;

    if (s_state.custom_frame_count == 0) {
        /* No frames loaded, all off */
        for (i = 0; i < WS2812_LED_COUNT; i++) {
            WS2812_SetPixel(i, 0, 0, 0);
        }
        return;
    }

    /* Determine current frame based on animation phase */
    frame_idx = (uint8_t)(s_anim_phase % s_state.custom_frame_count);

    /* Apply per-LED RGB888 color with global brightness */
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        const rgb888_t *led_color = &s_state.custom_frames[frame_idx][i];
        rgb888_t out;
        Color_ApplyBrightness(led_color, s_state.brightness, &out);
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
    s_state.r = 255;
    s_state.g = 255;
    s_state.b = 255;
    s_state.brightness = 128;
    s_state.speed = 128;
    s_state.custom_frame_interval = 100;
    s_anim_phase = 0;
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
    s_anim_phase = 0;
}

void Effect_SetCustomFrame(uint8_t frame_idx, const uint8_t *data)
{
    if (frame_idx < EFFECT_MAX_CUSTOM_FRAMES) {
        /* Copy 49 * 3 bytes of RGB888 data */
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
    s_anim_phase = 0;
}

bool Effect_Update(void)
{
    /* Render current frame based on mode */
    switch (s_state.mode) {
        case RGB_MODE_SOLID:
            RenderSolid();
            break;

        case RGB_MODE_BREATHING:
            RenderBreathing();
            break;

        case RGB_MODE_MARQUEE:
            RenderMarquee();
            break;

        case RGB_MODE_CUSTOM:
            RenderCustom();
            break;

        default:
            RenderSolid();
            break;
    }

    /* Advance animation phase */
    s_anim_phase++;
    s_state.tick++;

    /* Push to WS2812 */
    WS2812_Refresh();

    return TRUE;
}

const effect_state_t *Effect_GetState(void)
{
    return &s_state;
}
