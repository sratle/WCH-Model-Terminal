/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI rendering engine API.
*                      Low-level drawing primitives for RGB565 display.
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

/*=============================================================================
 *  Render Engine Initialization
 *=============================================================================*/

void ui_render_init(void);

/*=============================================================================
 *  Clipping
 *=============================================================================*/

/* Set global clip rectangle. All drawing is clipped to this region. */
void ui_render_set_clip(const ui_rect_t *rect);

/* Get current clip rectangle */
void ui_render_get_clip(ui_rect_t *rect);

/* Reset clip to full screen */
void ui_render_reset_clip(void);

/* Check if a rectangle intersects with current clip region */
bool ui_render_clip_test(const ui_rect_t *rect);

/*=============================================================================
 *  Pixel & Line Drawing
 *=============================================================================*/

/* Draw a single pixel (clipped) */
void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color);

/* Draw a horizontal line (clipped) */
void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color);

/* Draw a vertical line (clipped) */
void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color);

/* Draw a line between two points (Bresenham, clipped) */
void ui_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, ui_color_t color);

/*=============================================================================
 *  Rectangle Drawing
 *=============================================================================*/

/* Fill a rectangle with solid color (clipped) */
void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color);

/* Draw a rectangle border (clipped) */
void ui_draw_rect_border(const ui_rect_t *rect, ui_color_t color, int16_t thickness);

/* Draw a filled rectangle with border */
void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness);

/*=============================================================================
 *  Rounded Rectangle Drawing
 *=============================================================================*/

/* Fill a rounded rectangle (clipped) */
void ui_draw_fill_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t color);

/* Draw a rounded rectangle border (clipped) */
void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness);

/* Draw a filled rounded rectangle with border */
void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness);

/*=============================================================================
 *  Circle Drawing
 *=============================================================================*/

/* Fill a circle (clipped) */
void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color);

/* Draw a circle border (clipped) */
void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness);

/*=============================================================================
 *  Text Drawing
 *=============================================================================*/

/* Draw text at position with font (clipped, supports 1/2/4 bpp fonts) */
void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color);

/* Draw text in a rectangle with alignment (clipped) */
/* align: 0=left, 1=center, 2=right */
void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align);

/* Get text width in pixels */
int16_t ui_text_width(const char *text, const ui_font_t *font);

/*=============================================================================
 *  Screen Operations
 *=============================================================================*/

/* Clear entire screen with color */
void ui_screen_clear(ui_color_t color);

/* Full screen refresh - set window to full screen and clear */
void ui_full_refresh(void);

/* Flush a dirty region to display (set window and prepare for drawing) */
void ui_flush_region(const ui_rect_t *rect);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_RENDER_H */
