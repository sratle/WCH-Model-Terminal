/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.h
* Author             : LCD Model Team
* Version            : V2.0.0
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
 *  Display Driver Interface (portable)
 *=============================================================================*/

typedef struct {
    void (*set_window)(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void (*write_data16)(uint16_t data);
    void (*write_buffer)(const uint16_t *buf, uint32_t len);
    void (*clear)(uint16_t color);
} ui_display_driver_t;

void ui_render_set_driver(const ui_display_driver_t *driver);
void ui_render_gram_invalidate(void);

/*=============================================================================
 *  Line Buffer API (for efficient dirty-rect row-by-row rendering)
*=============================================================================*/

ui_color_t* ui_render_get_line_buf(void);
void ui_render_flush_line(int16_t y, int16_t x_start, int16_t width);
void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color);
void ui_render_set_line_pixel(int16_t x, ui_color_t color);
ui_color_t ui_render_get_line_pixel(int16_t x);

/*=============================================================================
 *  Clipping
*=============================================================================*/

void ui_render_set_clip(const ui_rect_t *rect);
void ui_render_get_clip(ui_rect_t *rect);
void ui_render_reset_clip(void);
bool ui_render_clip_test(const ui_rect_t *rect);

/*=============================================================================
 *  Pixel & Line Drawing
 *=============================================================================*/

void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color);
void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color);
void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color);
void ui_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, ui_color_t color);

/*=============================================================================
 *  Rectangle Drawing
 *=============================================================================*/

void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color);
void ui_draw_rect_border(const ui_rect_t *rect, ui_color_t color, int16_t thickness);
void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness);

/*=============================================================================
 *  Rounded Rectangle Drawing
 *=============================================================================*/

void ui_draw_fill_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t color);
void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness);
void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness);

/*=============================================================================
 *  Circle Drawing
 *=============================================================================*/

void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color);
void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness);

/*=============================================================================
 *  Icon Drawing (1bpp bitmap)
 *=============================================================================*/

void ui_draw_icon(int16_t x, int16_t y, const uint8_t *bitmap,
                  int16_t w, int16_t h, ui_color_t color);
void ui_draw_icon_in_rect(const ui_rect_t *rect, const uint8_t *bitmap,
                           int16_t w, int16_t h, ui_color_t color);

/*=============================================================================
 *  Text Drawing
 *=============================================================================*/

void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color);
void ui_draw_text_bg(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg);
void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align);
void ui_draw_text_in_rect_bg(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg, uint8_t align);
int16_t ui_text_width(const char *text, const ui_font_t *font);

/*=============================================================================
 *  Screen Operations
 *=============================================================================*/

void ui_screen_clear(ui_color_t color);
void ui_full_refresh(void);
void ui_flush_region(const ui_rect_t *rect);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_RENDER_H */
