/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI rendering engine API for E-ink display.
*                      1bpp compositing renderer: all drawing primitives write
*                      to a 1bpp line buffer; the page manager flushes
*                      completed regions to e-paper via partial refresh.
********************************************************************************/
#ifndef __MINIUI_RENDER_H
#define __MINIUI_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"
#include "miniui_color.h"

/*=============================================================================
 *  Screen Dimensions (648x480 E-ink)
 *=============================================================================*/

#define UI_SCREEN_WIDTH     648
#define UI_SCREEN_HEIGHT    480

/* Row bytes for full width (648/8 = 81) */
#define UI_SCREEN_ROW_BYTES  (UI_SCREEN_WIDTH / 8)

/* Number of rows batched per compose cycle */
#define UI_COMPOSE_BATCH    8

/*=============================================================================
 *  Render Engine Initialization
 *=============================================================================*/

void ui_render_init(void);

/*=============================================================================
 *  Compositing Engine API
 *
 *  Usage (called by page manager):
 *    1. ui_render_begin_rect(&row_target)  - set target range
 *    2. fill background via ui_render_fill_line_buf() or drawing primitives
 *    3. call widget draw_cb's
 *    4. ui_render_flush_rows(y, batch_h, x, w) - flush to frame buffer
 *=============================================================================*/

void ui_render_begin_rect(const ui_rect_t *rect);
void ui_render_get_target(ui_rect_t *rect);
void ui_render_push_target(const ui_rect_t *clip);
void ui_render_pop_target(void);

/* Fill a range of the line buffer with a solid color */
void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color);

/* Flush composited scanlines to frame buffer */
void ui_render_flush_rows(int16_t y_start, int16_t row_count,
                          int16_t x_start, int16_t width);

/* Get a pointer to the 1bpp line buffer */
uint8_t* ui_render_get_line_buf(void);

/* Set/get a single pixel in the line buffer */
void ui_render_set_line_pixel(int16_t x, ui_color_t color);
ui_color_t ui_render_get_line_pixel(int16_t x);

/*=============================================================================
 *  Drawing Primitives (1bpp, clipped to compositing target)
 *=============================================================================*/

void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color);
void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color);
void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color);
void ui_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, ui_color_t color);

void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color);
void ui_draw_rect_border(const ui_rect_t *rect, ui_color_t color, int16_t thickness);
void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness);

void ui_draw_fill_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t color);
void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness);
void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness);

void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color);
void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness);

void ui_draw_icon(int16_t x, int16_t y, const uint8_t *bitmap,
                  int16_t w, int16_t h, ui_color_t color);
void ui_draw_icon_in_rect(const ui_rect_t *rect, const uint8_t *bitmap,
                           int16_t w, int16_t h, ui_color_t color);

void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color);
void ui_draw_text_bg(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg);
void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align);
void ui_draw_text_in_rect_bg(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg, uint8_t align);
int16_t ui_text_width(const char *text, const ui_font_t *font);

/*=============================================================================
 *  Touch Cursor Drawing
 *=============================================================================*/

/* Draw a small arrow cursor at (x, y) with white outline + black fill.
 * Cursor size: 8x11 pixels + 1px outline = 10x13 dirty region. */
void ui_draw_touch_cursor(int16_t x, int16_t y);

/*=============================================================================
 *  E-ink Refresh Operations
 *=============================================================================*/

/* Flush all dirty regions to e-paper display */
void ui_render_flush_to_display(void);

/* Full screen clear and refresh */
void ui_screen_clear(ui_color_t color);

/* Get the frame buffer (new data) */
uint8_t* ui_render_get_frame_buf(void);

/* Get the old frame buffer */
uint8_t* ui_render_get_old_buf(void);

/* Swap new/old buffers (call after refresh) */
void ui_render_swap_buffers(void);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_RENDER_H */
