/*********************************************************************
 * File Name          : color.c
 * Description        : HSV <-> RGB888 color space conversion.
 *                      All calculations use integer arithmetic for
 *                      efficiency on RISC-V without FPU.
 *********************************************************************/

#include "color.h"

void Color_RGBtoHSV(uint8_t r, uint8_t g, uint8_t b, hsv_t *hsv)
{
    uint8_t cmax, cmin, delta;
    uint16_t h;

    /* Find max and min */
    cmax = r;
    if (g > cmax) cmax = g;
    if (b > cmax) cmax = b;

    cmin = r;
    if (g < cmin) cmin = g;
    if (b < cmin) cmin = b;

    delta = cmax - cmin;

    /* Hue */
    if (delta == 0) {
        h = 0;
    } else if (cmax == r) {
        int16_t diff = (int16_t)g - (int16_t)b;
        h = (uint16_t)((60L * diff / delta + 360) % 360);
    } else if (cmax == g) {
        int16_t diff = (int16_t)b - (int16_t)r;
        h = (uint16_t)((60L * diff / delta + 120 + 360) % 360);
    } else {
        int16_t diff = (int16_t)r - (int16_t)g;
        h = (uint16_t)((60L * diff / delta + 240 + 360) % 360);
    }

    /* Saturation: scale 0-255 (0=0%, 255=100%) */
    uint8_t s;
    if (cmax == 0) {
        s = 0;
    } else {
        s = (uint8_t)(255UL * delta / cmax);
    }

    hsv->h = h;
    hsv->s = s;
    hsv->v = cmax;
}

void Color_HSVtoRGB(const hsv_t *hsv, rgb888_t *rgb)
{
    uint8_t r, g, b;

    if (hsv->s == 0) {
        /* Achromatic */
        r = g = b = hsv->v;
    } else {
        uint16_t region = hsv->h / 60;
        uint16_t remainder = (hsv->h - (region * 60));

        /* Standard HSV→RGB formulas (q and t naming matches textbook):
         * q = v * (1 - s * (h - 60*region) / 60)
         * t = v * (1 - s * (60 - (h - 60*region)) / 60) */
        uint8_t p = (uint8_t)((uint32_t)hsv->v * (255 - hsv->s) / 255);
        uint8_t q = (uint8_t)((uint32_t)hsv->v * (255 - (uint16_t)hsv->s * remainder / 60) / 255);
        uint8_t t = (uint8_t)((uint32_t)hsv->v * (255 - (uint16_t)hsv->s * (60 - remainder) / 60) / 255);

        switch (region) {
            case 0:  r = hsv->v; g = t;     b = p;     break;
            case 1:  r = q;     g = hsv->v; b = p;     break;
            case 2:  r = p;     g = hsv->v; b = t;     break;
            case 3:  r = p;     g = q;      b = hsv->v; break;
            case 4:  r = t;     g = p;      b = hsv->v; break;
            default: r = hsv->v; g = p;     b = q;     break;
        }
    }

    rgb->r = r;
    rgb->g = g;
    rgb->b = b;
}

void Color_ScaleBrightness(const rgb888_t *in, uint8_t brightness, rgb888_t *out)
{
    /* Simple linear brightness scaling - preserves original color/hue */
    out->r = (uint8_t)((uint16_t)in->r * brightness / 255);
    out->g = (uint8_t)((uint16_t)in->g * brightness / 255);
    out->b = (uint8_t)((uint16_t)in->b * brightness / 255);
}

void Color_ApplyBrightness(const rgb888_t *in, uint8_t brightness, rgb888_t *out)
{
    hsv_t hsv;
    Color_RGBtoHSV(in->r, in->g, in->b, &hsv);

    /* Force saturation to maximum for vivid colors */
    hsv.s = 255;

    /* Scale V by brightness factor */
    hsv.v = (uint8_t)((uint16_t)hsv.v * brightness / 255);

    Color_HSVtoRGB(&hsv, out);
}

void Color_LerpRGB(const rgb888_t *a, const rgb888_t *b, uint8_t t, rgb888_t *out)
{
    uint8_t inv_t = 255 - t;
    out->r = (uint8_t)(((uint16_t)a->r * inv_t + (uint16_t)b->r * t) / 255);
    out->g = (uint8_t)(((uint16_t)a->g * inv_t + (uint16_t)b->g * t) / 255);
    out->b = (uint8_t)(((uint16_t)a->b * inv_t + (uint16_t)b->b * t) / 255);
}
