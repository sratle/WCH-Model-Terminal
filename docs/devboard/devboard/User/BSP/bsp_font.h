/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_font.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : Bitmap font type definitions for the LCD driver.
*                      The layout matches the display-2 MiniUI font format
*                      (1bpp, row-major, MSB-first), so font data extracted
*                      from LVGL can be reused directly.
********************************************************************************/
#ifndef __BSP_FONT_H
#define __BSP_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/*=============================================================================
 *  Glyph / Font Types
 *=============================================================================*/

/* Glyph descriptor for a single character.
 * Coordinate convention (same as MiniUI / LVGL):
 *   - The text drawing API receives the TOP-LEFT y of the text line.
 *   - The baseline sits at (y_top + font->baseline).
 *   - y_offset is NEGATIVE: distance from the baseline up to the glyph top.
 *   - Bitmap is 1bpp, row-major, MSB first, rows packed continuously:
 *     bit index of pixel (col,row) = row * width + col. */
typedef struct {
    uint16_t unicode;         /* Unicode code point                        */
    uint8_t  width;           /* Glyph width in pixels                     */
    uint8_t  height;          /* Glyph height in pixels                    */
    int8_t   x_offset;        /* X offset from cursor                      */
    int8_t   y_offset;        /* Y offset from baseline (negative = above) */
    uint8_t  advance;         /* Cursor advance after this glyph           */
    const uint8_t *bitmap;    /* Glyph bitmap data (1bpp)                  */
} lcd_glyph_t;

/* Font descriptor */
typedef struct {
    const lcd_glyph_t *glyphs; /* Glyph table, sorted by unicode ascending */
    uint16_t glyph_count;      /* Number of glyphs in the table            */
    uint8_t  height;           /* Line height (max)                        */
    int8_t   baseline;         /* Baseline offset from the line top        */
    uint8_t  bpp;              /* Bits per pixel (1 for these fonts)       */
} lcd_font_t;

/*=============================================================================
 *  Font Lookup API
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_FontGetGlyph
 *
 * @brief   Binary-search a glyph by unicode code point.
 *
 * @param   font    - font descriptor
 * @param   unicode - code point to look up
 *
 * @return  Pointer to the glyph, or NULL if not present in the font.
 */
const lcd_glyph_t *LCD_FontGetGlyph(const lcd_font_t *font, uint16_t unicode);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_FONT_H */
