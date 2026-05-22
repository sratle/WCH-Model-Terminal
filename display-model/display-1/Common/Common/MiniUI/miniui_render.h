/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.h
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/22
* Description        : MiniUI rendering engine API.
*                      Compositing-based renderer: all drawing primitives write
*                      to a line buffer; the page manager flushes completed
*                      scanlines to GRAM via SSD1963.
********************************************************************************/
#ifndef __MINIUI_RENDER_H
#define __MINIUI_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"
#include "miniui_color.h"

/*=============================================================================
 *  Screen Dimensions
 *=============================================================================*/

#define UI_SCREEN_WIDTH     800
#define UI_SCREEN_HEIGHT    480

/* Number of rows batched per compose+flush cycle.
 * Amortizes SetWindow overhead (11 FMC writes per call). */
#define UI_COMPOSE_BATCH    8

/*=============================================================================
 *  Render Engine Initialization
 *=============================================================================*/

void ui_render_init(void);

/*=============================================================================
 *  Display Driver Interface (portable)
 *=============================================================================*/

typedef struct {
    void (*set_window)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void (*write_data16)(uint16_t data);
    void (*write_buffer)(const uint16_t *buf, uint32_t len);
    void (*clear)(uint16_t color);
} ui_display_driver_t;

void ui_render_set_driver(const ui_display_driver_t *driver);

/*=============================================================================
 *  Compositing Engine API
 *
 *  Usage (called by page manager):
 *    1. ui_render_begin_rect(&row_target)  - set target range for this row
 *    2. fill background via ui_render_fill_line_buf() or drawing primitives
 *    3. call widget draw_cb's (they write to line_buf within target)
 *    4. ui_render_flush_row(y, x, w)       - write composited row to GRAM
 *=============================================================================*/

/* Set the compositing target rectangle. All drawing primitives will write
 * to the line buffer within this range. Typically one scanline at a time. */
void ui_render_begin_rect(const ui_rect_t *rect);

/* Get the current compositing target rectangle */
void ui_render_get_target(ui_rect_t *rect);

/* Fill a range of the line buffer with a solid color (fast background fill) */
void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color);

/* Flush one composited scanline from line buffer to GRAM */
void ui_render_flush_row(int16_t y, int16_t x_start, int16_t width);

/* Flush multiple composited scanlines to GRAM (band optimization) */
void ui_render_flush_rows(int16_t y_start, int16_t row_count,
                          int16_t x_start, int16_t width);

/* Get a pointer to the line buffer (for direct pixel manipulation) */
ui_color_t* ui_render_get_line_buf(void);

/* Set/get a single pixel in the line buffer */
void ui_render_set_line_pixel(int16_t x, ui_color_t color);
ui_color_t ui_render_get_line_pixel(int16_t x);

/*=============================================================================
 *  Drawing Primitives
 *
 *  These write to the line buffer (not directly to GRAM).
 *  They are clipped to the current compositing target rectangle.
 *  The page manager calls begin_rect() before and flush_row() after.
 *=============================================================================*/

/* Pixel & Line Drawing */
void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color);
void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color);
void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color);
void ui_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, ui_color_t color);

/* Rectangle Drawing */
void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color);
void ui_draw_rect_border(const ui_rect_t *rect, ui_color_t color, int16_t thickness);
void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness);

/* Rounded Rectangle Drawing */
void ui_draw_fill_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t color);
void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness);
void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness);

/* Circle Drawing */
void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color);
void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness);

/* Icon Drawing (1bpp bitmap) */
void ui_draw_icon(int16_t x, int16_t y, const uint8_t *bitmap,
                  int16_t w, int16_t h, ui_color_t color);
void ui_draw_icon_in_rect(const ui_rect_t *rect, const uint8_t *bitmap,
                           int16_t w, int16_t h, ui_color_t color);

/* Text Drawing */
void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color);
void ui_draw_text_bg(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg);
void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align);
void ui_draw_text_in_rect_bg(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg, uint8_t align);
int16_t ui_text_width(const char *text, const ui_font_t *font);

/*=============================================================================
 *  Screen Operations (direct GRAM access, not via line buffer)
 *=============================================================================*/

void ui_screen_clear(ui_color_t color);
void ui_full_refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_RENDER_H */
