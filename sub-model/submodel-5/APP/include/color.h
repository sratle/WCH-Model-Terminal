/*********************************************************************
 * File Name          : color.h
 * Description        : HSV <-> RGB888 color space conversion.
 *                      Used as intermediate color space for brightness
 *                      control and smooth color transitions.
 *********************************************************************/

#ifndef __COLOR_H
#define __COLOR_H

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/*  Color Types                                                         */
/* ==================================================================== */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb888_t;

typedef struct {
    uint16_t h;     /* Hue: 0-359 degrees */
    uint8_t  s;     /* Saturation: 0-255 */
    uint8_t  v;     /* Value/Brightness: 0-255 */
} hsv_t;

/* ==================================================================== */
/*  API Functions                                                       */
/* ==================================================================== */

/**
 * @brief  Convert RGB888 to HSV color space.
 * @param  r, g, b  - Input RGB values (0-255)
 * @param  hsv      - Output HSV struct
 */
void Color_RGBtoHSV(uint8_t r, uint8_t g, uint8_t b, hsv_t *hsv);

/**
 * @brief  Convert HSV to RGB888 color space.
 * @param  hsv      - Input HSV struct
 * @param  rgb      - Output RGB888 struct
 */
void Color_HSVtoRGB(const hsv_t *hsv, rgb888_t *rgb);

/**
 * @brief  Apply brightness (0-255) to an RGB color via HSV conversion.
 *         Converts RGB->HSV, scales V by brightness, converts HSV->RGB.
 * @param  in         - Input RGB888
 * @param  brightness - Brightness factor (0=off, 255=full)
 * @param  out        - Output RGB888 with brightness applied
 */
void Color_ApplyBrightness(const rgb888_t *in, uint8_t brightness, rgb888_t *out);

/**
 * @brief  Linear interpolation between two RGB888 colors.
 * @param  a   - Start color
 * @param  b   - End color
 * @param  t   - Interpolation factor (0=100% a, 255=100% b)
 * @param  out - Output interpolated color
 */
void Color_LerpRGB(const rgb888_t *a, const rgb888_t *b, uint8_t t, rgb888_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __COLOR_H */
