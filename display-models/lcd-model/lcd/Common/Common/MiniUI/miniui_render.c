/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI rendering engine implementation.
*                      Drawing primitives using SSD1963 GRAM via FMC.
********************************************************************************/
#include "miniui_render.h"
#include "../SSD1963/ssd1963.h"
#include <string.h>

/*=============================================================================
 *  Internal State
 *=============================================================================*/

static ui_rect_t g_clip_rect = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};

/*=============================================================================
 *  Helper Functions
 *=============================================================================*/

/* Clamp value between min and max */
static inline int16_t clamp(int16_t v, int16_t min, int16_t max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

/* Intersect two rectangles, return true if they intersect */
static bool rect_intersect(const ui_rect_t *a, const ui_rect_t *b, ui_rect_t *out)
{
    int16_t x1 = (a->x > b->x) ? a->x : b->x;
    int16_t y1 = (a->y > b->y) ? a->y : b->y;
    int16_t x2 = (a->x + a->w < b->x + b->w) ? a->x + a->w : b->x + b->w;
    int16_t y2 = (a->y + a->h < b->y + b->h) ? a->y + a->h : b->y + b->h;
    
    if (x2 <= x1 || y2 <= y1) return false;
    
    out->x = x1;
    out->y = y1;
    out->w = x2 - x1;
    out->h = y2 - y1;
    return true;
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void ui_render_init(void)
{
    g_clip_rect.x = 0;
    g_clip_rect.y = 0;
    g_clip_rect.w = UI_SCREEN_WIDTH;
    g_clip_rect.h = UI_SCREEN_HEIGHT;
}

/*=============================================================================
 *  Clipping
 *=============================================================================*/

void ui_render_set_clip(const ui_rect_t *rect)
{
    if (rect) {
        g_clip_rect = *rect;
    }
}

void ui_render_get_clip(ui_rect_t *rect)
{
    if (rect) {
        *rect = g_clip_rect;
    }
}

void ui_render_reset_clip(void)
{
    g_clip_rect.x = 0;
    g_clip_rect.y = 0;
    g_clip_rect.w = UI_SCREEN_WIDTH;
    g_clip_rect.h = UI_SCREEN_HEIGHT;
}

bool ui_render_clip_test(const ui_rect_t *rect)
{
    ui_rect_t tmp;
    return rect_intersect(rect, &g_clip_rect, &tmp);
}

/*=============================================================================
 *  Pixel & Line Drawing
 *=============================================================================*/

void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color)
{
    if (x < g_clip_rect.x || x >= g_clip_rect.x + g_clip_rect.w ||
        y < g_clip_rect.y || y >= g_clip_rect.y + g_clip_rect.h) {
        return;
    }
    SSD1963_DrawPixel((uint16_t)x, (uint16_t)y, color);
}

void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color)
{
    if (y < g_clip_rect.y || y >= g_clip_rect.y + g_clip_rect.h) return;
    
    int16_t x1 = clamp(x, g_clip_rect.x, g_clip_rect.x + g_clip_rect.w);
    int16_t x2 = clamp(x + w, g_clip_rect.x, g_clip_rect.x + g_clip_rect.w);
    int16_t len = x2 - x1;
    
    if (len <= 0) return;
    
    SSD1963_SetWindow((uint16_t)x1, (uint16_t)y, (uint16_t)(x2 - 1), (uint16_t)y);
    for (int16_t i = 0; i < len; i++) {
        SSD1963_WriteData16(color);
    }
}

void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color)
{
    if (x < g_clip_rect.x || x >= g_clip_rect.x + g_clip_rect.w) return;
    
    int16_t y1 = clamp(y, g_clip_rect.y, g_clip_rect.y + g_clip_rect.h);
    int16_t y2 = clamp(y + h, g_clip_rect.y, g_clip_rect.y + g_clip_rect.h);
    int16_t len = y2 - y1;
    
    if (len <= 0) return;
    
    SSD1963_SetWindow((uint16_t)x, (uint16_t)y1, (uint16_t)x, (uint16_t)(y2 - 1));
    for (int16_t i = 0; i < len; i++) {
        SSD1963_WriteData16(color);
    }
}

void ui_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, ui_color_t color)
{
    int16_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int16_t dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;
    
    while (1) {
        ui_draw_pixel(x1, y1, color);
        
        if (x1 == x2 && y1 == y2) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/*=============================================================================
 *  Rectangle Drawing
 *=============================================================================*/

void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color)
{
    ui_rect_t r;
    if (!rect_intersect(rect, &g_clip_rect, &r)) return;
    
    SSD1963_SetWindow((uint16_t)r.x, (uint16_t)r.y,
                      (uint16_t)(r.x + r.w - 1), (uint16_t)(r.y + r.h - 1));
    
    uint32_t pixel_count = (uint32_t)r.w * r.h;
    while (pixel_count--) {
        SSD1963_WriteData16(color);
    }
}

void ui_draw_rect_border(const ui_rect_t *rect, ui_color_t color, int16_t thickness)
{
    if (thickness <= 0) return;
    
    /* Top */
    ui_draw_hline(rect->x, rect->y, rect->w, color);
    /* Bottom */
    ui_draw_hline(rect->x, rect->y + rect->h - 1, rect->w, color);
    /* Left */
    ui_draw_vline(rect->x, rect->y, rect->h, color);
    /* Right */
    ui_draw_vline(rect->x + rect->w - 1, rect->y, rect->h, color);
    
    if (thickness > 1) {
        ui_rect_t inner = {
            rect->x + 1,
            rect->y + 1,
            rect->w - 2,
            rect->h - 2
        };
        ui_draw_rect_border(&inner, color, thickness - 1);
    }
}

void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness)
{
    ui_draw_fill_rect(rect, fill);
    if (thickness > 0) {
        ui_draw_rect_border(rect, border, thickness);
    }
}

/*=============================================================================
 *  Rounded Rectangle Drawing
 *=============================================================================*/

/* Helper: fill a circle quadrant */
static void fill_circle_quadrant(int16_t cx, int16_t cy, int16_t r, ui_color_t color, uint8_t q)
{
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    
    while (x <= y) {
        if (q == 0) { /* Top-left */
            ui_draw_hline(cx - y, cy - x, y, color);
            ui_draw_hline(cx - x, cy - y, x, color);
        } else if (q == 1) { /* Top-right */
            ui_draw_hline(cx, cy - x, y, color);
            ui_draw_hline(cx, cy - y, x, color);
        } else if (q == 2) { /* Bottom-left */
            ui_draw_hline(cx - y, cy + x, y, color);
            ui_draw_hline(cx - x, cy + y, x, color);
        } else { /* Bottom-right */
            ui_draw_hline(cx, cy + x, y, color);
            ui_draw_hline(cx, cy + y, x, color);
        }
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void ui_draw_fill_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t color)
{
    if (radius <= 0) {
        ui_draw_fill_rect(rect, color);
        return;
    }
    
    int16_t r = radius;
    if (r > rect->w / 2) r = rect->w / 2;
    if (r > rect->h / 2) r = rect->h / 2;
    
    /* Fill center rectangle */
    ui_rect_t center = {
        rect->x,
        rect->y + r,
        rect->w,
        rect->h - 2 * r
    };
    ui_draw_fill_rect(&center, color);
    
    /* Fill top rectangle */
    ui_rect_t top = {
        rect->x + r,
        rect->y,
        rect->w - 2 * r,
        r
    };
    ui_draw_fill_rect(&top, color);
    
    /* Fill bottom rectangle */
    ui_rect_t bottom = {
        rect->x + r,
        rect->y + rect->h - r,
        rect->w - 2 * r,
        r
    };
    ui_draw_fill_rect(&bottom, color);
    
    /* Fill corners */
    fill_circle_quadrant(rect->x + r, rect->y + r, r, color, 0);
    fill_circle_quadrant(rect->x + rect->w - r - 1, rect->y + r, r, color, 1);
    fill_circle_quadrant(rect->x + r, rect->y + rect->h - r - 1, r, color, 2);
    fill_circle_quadrant(rect->x + rect->w - r - 1, rect->y + rect->h - r - 1, r, color, 3);
}

void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness)
{
    /* Simplified: draw a normal rect border for now */
    ui_draw_rect_border(rect, color, thickness);
}

void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness)
{
    ui_draw_fill_round_rect(rect, radius, fill);
    if (thickness > 0) {
        ui_draw_round_rect_border(rect, radius, border, thickness);
    }
}

/*=============================================================================
 *  Circle Drawing
 *=============================================================================*/

void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color)
{
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    
    while (x <= y) {
        ui_draw_hline(cx - y, cy - x, 2 * y + 1, color);
        ui_draw_hline(cx - x, cy - y, 2 * x + 1, color);
        ui_draw_hline(cx - y, cy + x, 2 * y + 1, color);
        ui_draw_hline(cx - x, cy + y, 2 * x + 1, color);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness)
{
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    
    while (x <= y) {
        ui_draw_pixel(cx + x, cy + y, color);
        ui_draw_pixel(cx - x, cy + y, color);
        ui_draw_pixel(cx + x, cy - y, color);
        ui_draw_pixel(cx - x, cy - y, color);
        ui_draw_pixel(cx + y, cy + x, color);
        ui_draw_pixel(cx - y, cy + x, color);
        ui_draw_pixel(cx + y, cy - x, color);
        ui_draw_pixel(cx - y, cy - x, color);
        
        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
    
    if (thickness > 1) {
        ui_draw_circle_border(cx, cy, r - 1, color, thickness - 1);
    }
}

/*=============================================================================
 *  Text Drawing
 *=============================================================================*/

/* Find glyph by unicode code point */
static const ui_glyph_t* find_glyph(const ui_font_t *font, uint16_t unicode)
{
    for (uint16_t i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].unicode == unicode) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

/* Draw a single glyph */
static void draw_glyph(int16_t x, int16_t y, const ui_glyph_t *glyph, const ui_font_t *font, ui_color_t color)
{
    int16_t gx = x + glyph->x_offset;
    int16_t gy = y + glyph->y_offset;
    
    uint16_t bitmap_size = glyph->width * glyph->height;
    uint8_t bpp = font->bpp;
    uint8_t mask = (1 << bpp) - 1;
    uint8_t pixels_per_byte = 8 / bpp;
    
    uint32_t bit_idx = 0;
    
    for (uint16_t row = 0; row < glyph->height; row++) {
        for (uint16_t col = 0; col < glyph->width; col++) {
            uint32_t byte_idx = bit_idx / 8;
            uint32_t shift = 8 - bpp - (bit_idx % 8);
            uint8_t alpha = (glyph->bitmap[byte_idx] >> shift) & mask;
            
            if (alpha > 0) {
                if (bpp == 1) {
                    /* 1bpp: no blending, just draw */
                    ui_draw_pixel(gx + col, gy + row, color);
                } else {
                    /* 2bpp or 4bpp: blend with alpha */
                    uint8_t alpha_8bit = (alpha * 255) / mask;
                    /* For now, simplified: draw if alpha > half */
                    if (alpha_8bit > 128) {
                        ui_draw_pixel(gx + col, gy + row, color);
                    }
                }
            }
            bit_idx += bpp;
        }
    }
}

void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color)
{
    if (!text || !font) return;
    
    int16_t cx = x;
    int16_t cy = y + font->baseline;
    
    while (*text) {
        uint16_t unicode = (uint8_t)*text;
        const ui_glyph_t *glyph = find_glyph(font, unicode);
        
        if (glyph) {
            draw_glyph(cx, cy, glyph, font, color);
            cx += glyph->advance;
        } else {
            /* Missing glyph: advance by space width or fixed amount */
            cx += font->height / 2;
        }
        
        text++;
    }
}

void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align)
{
    if (!text || !font) return;
    
    int16_t tw = ui_text_width(text, font);
    int16_t x, y;
    
    /* Vertical center */
    y = rect->y + (rect->h - font->height) / 2;
    
    /* Horizontal alignment */
    if (align == 0) { /* Left */
        x = rect->x;
    } else if (align == 1) { /* Center */
        x = rect->x + (rect->w - tw) / 2;
    } else { /* Right */
        x = rect->x + rect->w - tw;
    }
    
    ui_draw_text(x, y, text, font, color);
}

int16_t ui_text_width(const char *text, const ui_font_t *font)
{
    if (!text || !font) return 0;
    
    int16_t width = 0;
    while (*text) {
        uint16_t unicode = (uint8_t)*text;
        const ui_glyph_t *glyph = find_glyph(font, unicode);
        
        if (glyph) {
            width += glyph->advance;
        } else {
            width += font->height / 2;
        }
        
        text++;
    }
    return width;
}

/*=============================================================================
 *  Screen Operations
 *=============================================================================*/

void ui_screen_clear(ui_color_t color)
{
    SSD1963_Clear(color);
}

void ui_full_refresh(void)
{
    ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_render_reset_clip();
    ui_draw_fill_rect(&full, UI_COLOR_BG_MAIN);
}

void ui_flush_region(const ui_rect_t *rect)
{
    /* Set SSD1963 window to the dirty region for subsequent drawing */
    if (rect) {
        ui_rect_t r;
        if (rect_intersect(rect, &g_clip_rect, &r)) {
            SSD1963_SetWindow((uint16_t)r.x, (uint16_t)r.y,
                              (uint16_t)(r.x + r.w - 1), (uint16_t)(r.y + r.h - 1));
        }
    }
}
