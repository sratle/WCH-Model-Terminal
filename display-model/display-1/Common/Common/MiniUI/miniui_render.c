/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2025/04/20
* Description        : MiniUI rendering engine implementation.
*                      Drawing primitives using SSD1963 GRAM via FMC.
*                      Optimized for 800x480 RGB565 with dirty-rect partial refresh.
*                      Uses line buffer for efficient row-by-row rendering.
********************************************************************************/
#include "miniui_render.h"
#include "../SSD1963/ssd1963.h"
#include "debug.h"
#include <string.h>

static ui_display_driver_t s_driver = {
    SSD1963_SetWindow,
    SSD1963_WriteData16,
    SSD1963_WriteBuffer,
    SSD1963_Clear,
};

void ui_render_set_driver(const ui_display_driver_t *driver)
{
    if (driver) {
        s_driver = *driver;
    }
}

/*=============================================================================
 *  Line Buffer
*  One full screen row = 800 * 2 = 1600 bytes.
*  Used for efficient dirty-rect rendering: fill bg, composite widgets, flush.
*=============================================================================*/

static ui_color_t g_line_buf[UI_SCREEN_WIDTH];

/*=============================================================================
 *  Internal State
*=============================================================================*/

static ui_rect_t g_clip_rect = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};

/*=============================================================================
 *  Helper Functions
*=============================================================================*/

static inline int16_t ui_max(int16_t a, int16_t b) { return a > b ? a : b; }
static inline int16_t ui_min(int16_t a, int16_t b) { return a < b ? a : b; }
static inline int16_t ui_clamp(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool rect_intersect(const ui_rect_t *a, const ui_rect_t *b, ui_rect_t *out)
{
    int16_t x1 = ui_max(a->x, b->x);
    int16_t y1 = ui_max(a->y, b->y);
    int16_t x2 = ui_min(a->x + a->w, b->x + b->w);
    int16_t y2 = ui_min(a->y + a->h, b->y + b->h);

    if (x2 <= x1 || y2 <= y1) return false;

    out->x = x1;
    out->y = y1;
    out->w = x2 - x1;
    out->h = y2 - y1;
    return true;
}

static inline bool rect_contains(const ui_rect_t *r, int16_t x, int16_t y)
{
    return (x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h);
}

/*=============================================================================
 *  Initialization
*=============================================================================*/

void ui_render_init(void)
{
    printf("[ui_render_init] start\r\n");
    g_clip_rect.x = 0;
    g_clip_rect.y = 0;
    g_clip_rect.w = UI_SCREEN_WIDTH;
    g_clip_rect.h = UI_SCREEN_HEIGHT;
    printf("[ui_render_init] done\r\n");
}

/*=============================================================================
 *  Line Buffer API
*=============================================================================*/

ui_color_t* ui_render_get_line_buf(void)
{
    return g_line_buf;
}

void ui_render_flush_line(int16_t y, int16_t x_start, int16_t width)
{
    if (width <= 0 || y < 0 || y >= UI_SCREEN_HEIGHT) return;
    if (x_start < 0) { width += x_start; x_start = 0; }
    if (x_start + width > UI_SCREEN_WIDTH) width = UI_SCREEN_WIDTH - x_start;
    if (width <= 0) return;

    s_driver.set_window((uint16_t)x_start, (uint16_t)y,
                      (uint16_t)(x_start + width - 1), (uint16_t)y);
    s_driver.write_buffer(&g_line_buf[x_start], (uint32_t)width);
}

void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color)
{
    if (width <= 0 || x_start < 0 || x_start >= UI_SCREEN_WIDTH) return;
    if (x_start + width > UI_SCREEN_WIDTH) width = UI_SCREEN_WIDTH - x_start;
    for (int16_t i = 0; i < width; i++) {
        g_line_buf[x_start + i] = color;
    }
}

void ui_render_set_line_pixel(int16_t x, ui_color_t color)
{
    if (x >= 0 && x < UI_SCREEN_WIDTH) {
        g_line_buf[x] = color;
    }
}

ui_color_t ui_render_get_line_pixel(int16_t x)
{
    if (x >= 0 && x < UI_SCREEN_WIDTH) {
        return g_line_buf[x];
    }
    return 0;
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
    if (rect) *rect = g_clip_rect;
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
 *  Pixel & Line Drawing (direct GRAM)
*=============================================================================*/

void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color)
{
    if (x < g_clip_rect.x || x >= g_clip_rect.x + g_clip_rect.w ||
        y < g_clip_rect.y || y >= g_clip_rect.y + g_clip_rect.h) {
        return;
    }
    s_driver.set_window((uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y);
    s_driver.write_data16(color);
}

void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color)
{
    if (y < g_clip_rect.y || y >= g_clip_rect.y + g_clip_rect.h) return;
    if (w <= 0) return;

    int16_t x1 = ui_clamp(x, g_clip_rect.x, g_clip_rect.x + g_clip_rect.w);
    int16_t x2 = ui_clamp(x + w, g_clip_rect.x, g_clip_rect.x + g_clip_rect.w);
    int16_t len = x2 - x1;
    if (len <= 0) return;

    s_driver.set_window((uint16_t)x1, (uint16_t)y, (uint16_t)(x2 - 1), (uint16_t)y);

    for (int16_t i = 0; i < len; i++) {
        g_line_buf[i] = color;
    }
    s_driver.write_buffer(g_line_buf, (uint32_t)len);
}

void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color)
{
    if (x < g_clip_rect.x || x >= g_clip_rect.x + g_clip_rect.w) return;
    if (h <= 0) return;

    int16_t y1 = ui_clamp(y, g_clip_rect.y, g_clip_rect.y + g_clip_rect.h);
    int16_t y2 = ui_clamp(y + h, g_clip_rect.y, g_clip_rect.y + g_clip_rect.h);
    int16_t len = y2 - y1;
    if (len <= 0) return;

    s_driver.set_window((uint16_t)x, (uint16_t)y1, (uint16_t)x, (uint16_t)(y2 - 1));
    while (len--) {
        s_driver.write_data16(color);
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
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx)  { err += dx; y1 += sy; }
    }
}

/*=============================================================================
 *  Rectangle Drawing
*=============================================================================*/

void ui_draw_fill_rect(const ui_rect_t *rect, ui_color_t color)
{
    ui_rect_t r;
    if (!rect_intersect(rect, &g_clip_rect, &r)) return;
    if (r.w <= 0 || r.h <= 0) return;

    for (int16_t i = 0; i < r.w; i++) {
        g_line_buf[i] = color;
    }

    s_driver.set_window((uint16_t)r.x, (uint16_t)r.y,
                      (uint16_t)(r.x + r.w - 1), (uint16_t)(r.y + r.h - 1));

    uint32_t row_count = (uint32_t)r.h;
    while (row_count--) {
        s_driver.write_buffer(g_line_buf, (uint32_t)r.w);
    }
}

void ui_draw_rect_border(const ui_rect_t *rect, ui_color_t color, int16_t thickness)
{
    if (thickness <= 0) return;

    ui_draw_hline(rect->x, rect->y, rect->w, color);
    ui_draw_hline(rect->x, rect->y + rect->h - 1, rect->w, color);
    ui_draw_vline(rect->x, rect->y + 1, rect->h - 2, color);
    ui_draw_vline(rect->x + rect->w - 1, rect->y + 1, rect->h - 2, color);

    if (thickness > 1) {
        ui_rect_t inner = { rect->x + 1, rect->y + 1, rect->w - 2, rect->h - 2 };
        if (inner.w > 0 && inner.h > 0) {
            ui_draw_rect_border(&inner, color, thickness - 1);
        }
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
*
*  Fixed fill_circle_quadrant: each hline now spans from the circle edge
*  to the center, correctly filling the quadrant corner area.
*=============================================================================*/

static void fill_circle_quadrant(int16_t cx, int16_t cy, int16_t r, ui_color_t color, uint8_t q)
{
    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;

    while (x <= y) {
        switch (q) {
        case 0:
            ui_draw_hline(cx - y, cy - x, y + 1, color);
            ui_draw_hline(cx - x, cy - y, x + 1, color);
            break;
        case 1:
            ui_draw_hline(cx, cy - x, y + 1, color);
            ui_draw_hline(cx, cy - y, x + 1, color);
            break;
        case 2:
            ui_draw_hline(cx - y, cy + x, y + 1, color);
            ui_draw_hline(cx - x, cy + y, x + 1, color);
            break;
        case 3:
            ui_draw_hline(cx, cy + x, y + 1, color);
            ui_draw_hline(cx, cy + y, x + 1, color);
            break;
        }
        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
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

    ui_rect_t center = { rect->x, rect->y + r, rect->w, rect->h - 2 * r };
    ui_draw_fill_rect(&center, color);

    ui_rect_t top = { rect->x + r, rect->y, rect->w - 2 * r, r };
    ui_draw_fill_rect(&top, color);

    ui_rect_t bottom = { rect->x + r, rect->y + rect->h - r, rect->w - 2 * r, r };
    ui_draw_fill_rect(&bottom, color);

    fill_circle_quadrant(rect->x + r, rect->y + r, r, color, 0);
    fill_circle_quadrant(rect->x + rect->w - r - 1, rect->y + r, r, color, 1);
    fill_circle_quadrant(rect->x + r, rect->y + rect->h - r - 1, r, color, 2);
    fill_circle_quadrant(rect->x + rect->w - r - 1, rect->y + rect->h - r - 1, r, color, 3);
}

static void draw_round_rect_outline(const ui_rect_t *rect, int16_t radius, ui_color_t color)
{
    int16_t r = radius;
    if (r > rect->w / 2) r = rect->w / 2;
    if (r > rect->h / 2) r = rect->h / 2;

    ui_draw_hline(rect->x + r, rect->y, rect->w - 2 * r, color);
    ui_draw_hline(rect->x + r, rect->y + rect->h - 1, rect->w - 2 * r, color);
    ui_draw_vline(rect->x, rect->y + r, rect->h - 2 * r, color);
    ui_draw_vline(rect->x + rect->w - 1, rect->y + r, rect->h - 2 * r, color);

    int16_t cx[4], cy[4];
    cx[0] = rect->x + r;                     cy[0] = rect->y + r;
    cx[1] = rect->x + rect->w - r - 1;       cy[1] = rect->y + r;
    cx[2] = rect->x + r;                     cy[2] = rect->y + rect->h - r - 1;
    cx[3] = rect->x + rect->w - r - 1;       cy[3] = rect->y + rect->h - r - 1;

    for (int q = 0; q < 4; q++) {
        int16_t x = 0, y = r;
        int16_t d = 3 - 2 * r;
        while (x <= y) {
            switch (q) {
            case 0: ui_draw_pixel(cx[q] - x, cy[q] - y, color);
                    ui_draw_pixel(cx[q] - y, cy[q] - x, color); break;
            case 1: ui_draw_pixel(cx[q] + x, cy[q] - y, color);
                    ui_draw_pixel(cx[q] + y, cy[q] - x, color); break;
            case 2: ui_draw_pixel(cx[q] - x, cy[q] + y, color);
                    ui_draw_pixel(cx[q] - y, cy[q] + x, color); break;
            case 3: ui_draw_pixel(cx[q] + x, cy[q] + y, color);
                    ui_draw_pixel(cx[q] + y, cy[q] + x, color); break;
            }
            if (d < 0) { d += 4 * x + 6; }
            else       { d += 4 * (x - y) + 10; y--; }
            x++;
        }
    }
}

void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness)
{
    if (thickness <= 0) return;
    draw_round_rect_outline(rect, radius, color);
    if (thickness > 1) {
        ui_rect_t inner = { rect->x + 1, rect->y + 1, rect->w - 2, rect->h - 2 };
        if (inner.w > 0 && inner.h > 0) {
            ui_draw_round_rect_border(&inner, radius > 1 ? radius - 1 : 0, color, thickness - 1);
        }
    }
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
        if (d < 0) { d += 4 * x + 6; }
        else       { d += 4 * (x - y) + 10; y--; }
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
        if (d < 0) { d += 4 * x + 6; }
        else       { d += 4 * (x - y) + 10; y--; }
        x++;
    }

    if (thickness > 1 && r > 1) {
        ui_draw_circle_border(cx, cy, r - 1, color, thickness - 1);
    }
}

/*=============================================================================
 *  Icon Drawing (1bpp bitmap)
*=============================================================================*/

void ui_draw_icon(int16_t x, int16_t y, const uint8_t *bitmap,
                  int16_t w, int16_t h, ui_color_t color)
{
    if (!bitmap) return;

    for (int16_t row = 0; row < h; row++) {
        uint16_t byte_idx = row * ((w + 7) / 8);
        for (int16_t col = 0; col < w; col++) {
            if (bitmap[byte_idx + col / 8] & (0x80 >> (col % 8))) {
                ui_draw_pixel(x + col, y + row, color);
            }
        }
    }
}

void ui_draw_icon_in_rect(const ui_rect_t *rect, const uint8_t *bitmap,
                           int16_t w, int16_t h, ui_color_t color)
{
    if (!bitmap || !rect) return;
    int16_t ix = rect->x + (rect->w - w) / 2;
    int16_t iy = rect->y + (rect->h - h) / 2;
    ui_draw_icon(ix, iy, bitmap, w, h, color);
}

/*=============================================================================
 *  Text Drawing
*=============================================================================*/

static const ui_glyph_t* find_glyph(const ui_font_t *font, uint16_t unicode)
{
    if (!font || !font->glyphs) return NULL;

    for (uint16_t i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].unicode == unicode) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

static void draw_glyph(int16_t x, int16_t y, const ui_glyph_t *glyph,
                       const ui_font_t *font, ui_color_t color, ui_color_t bg)
{
    int16_t gx = x + glyph->x_offset;
    int16_t gy = y + glyph->y_offset;
    uint8_t bpp = font->bpp;
    uint8_t mask = (1 << bpp) - 1;
    uint32_t bit_idx = 0;

    for (uint16_t row = 0; row < glyph->height; row++) {
        for (uint16_t col = 0; col < glyph->width; col++) {
            uint32_t byte_idx = bit_idx / 8;
            uint32_t shift = 8 - bpp - (bit_idx % 8);
            uint8_t alpha = (glyph->bitmap[byte_idx] >> shift) & mask;

            if (bpp == 1) {
                if (alpha) {
                    ui_draw_pixel(gx + col, gy + row, color);
                } else if (bg != UI_COLOR_TRANSPARENT) {
                    ui_draw_pixel(gx + col, gy + row, bg);
                }
            } else {
                if (alpha > 0) {
                    if (bg == UI_COLOR_TRANSPARENT) {
                        ui_draw_pixel(gx + col, gy + row, color);
                    } else {
                        uint8_t a8 = (uint16_t)alpha * 255 / mask;
                        ui_color_t blended = ui_color_blend(color, bg, a8);
                        ui_draw_pixel(gx + col, gy + row, blended);
                    }
                } else if (bg != UI_COLOR_TRANSPARENT) {
                    ui_draw_pixel(gx + col, gy + row, bg);
                }
            }
            bit_idx += bpp;
        }
    }
}

void ui_draw_text(int16_t x, int16_t y, const char *text,
                  const ui_font_t *font, ui_color_t color)
{
    ui_draw_text_bg(x, y, text, font, color, UI_COLOR_TRANSPARENT);
}

void ui_draw_text_bg(int16_t x, int16_t y, const char *text,
                     const ui_font_t *font, ui_color_t color, ui_color_t bg)
{
    if (!text || !font) return;

    int16_t cx = x;
    int16_t cy = y + font->baseline;

    while (*text) {
        uint16_t unicode = (uint8_t)*text;
        const ui_glyph_t *glyph = find_glyph(font, unicode);

        if (glyph) {
            draw_glyph(cx, cy, glyph, font, color, bg);
            cx += glyph->advance;
        } else {
            int16_t box_size = font->height * 2 / 3;
            int16_t box_y = cy - box_size;
            ui_rect_t box = {cx, box_y, box_size, box_size};
            ui_draw_rect(&box, color, UI_COLOR_TRANSPARENT, 0);
            cx += box_size + 1;
        }
        text++;
    }
}

void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text,
                          const ui_font_t *font, ui_color_t color, uint8_t align)
{
    if (!text || !font || !rect) return;

    int16_t tw = ui_text_width(text, font);
    int16_t x, y;

    y = rect->y + (rect->h - font->height) / 2;

    switch (align) {
    case 0: x = rect->x; break;
    case 2: x = rect->x + rect->w - tw; break;
    default: x = rect->x + (rect->w - tw) / 2; break;
    }

    ui_draw_text(x, y, text, font, color);
}

void ui_draw_text_in_rect_bg(const ui_rect_t *rect, const char *text,
                             const ui_font_t *font, ui_color_t color,
                             ui_color_t bg, uint8_t align)
{
    if (!text || !font || !rect) return;

    int16_t tw = ui_text_width(text, font);
    int16_t x, y;

    y = rect->y + (rect->h - font->height) / 2;

    switch (align) {
    case 0: x = rect->x; break;
    case 2: x = rect->x + rect->w - tw; break;
    default: x = rect->x + (rect->w - tw) / 2; break;
    }

    ui_draw_text_bg(x, y, text, font, color, bg);
}

int16_t ui_text_width(const char *text, const ui_font_t *font)
{
    if (!text || !font) return 0;

    int16_t width = 0;
    while (*text) {
        uint16_t unicode = (uint8_t)*text;
        const ui_glyph_t *glyph = find_glyph(font, unicode);
        width += glyph ? glyph->advance : (font->height * 2 / 3 + 1);
        text++;
    }
    return width;
}

/*=============================================================================
 *  Screen Operations
*=============================================================================*/

void ui_screen_clear(ui_color_t color)
{
    s_driver.clear(color);
}

void ui_full_refresh(void)
{
    s_driver.clear(UI_COLOR_BG_MAIN);
}

void ui_flush_region(const ui_rect_t *rect)
{
    (void)rect;
}
