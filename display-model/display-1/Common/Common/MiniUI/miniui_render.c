/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.c
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/22
* Description        : MiniUI compositing rendering engine.
*                      All drawing primitives write to a line buffer.
*                      The page manager composites widgets row-by-row and
*                      flushes completed scanlines to SSD1963 GRAM.
********************************************************************************/
#include "miniui_render.h"
#include "../SSD1963/ssd1963.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Display Driver
 *=============================================================================*/

static ui_display_driver_t s_driver = {
    SSD1963_SetWindow,
    SSD1963_WriteData16,
    SSD1963_WriteBuffer,
    SSD1963_Clear,
};

void ui_render_set_driver(const ui_display_driver_t *driver)
{
    if (driver) s_driver = *driver;
}

/*=============================================================================
 *  Line Buffer & Compositing Target
 *=============================================================================*/

static ui_color_t g_line_buf[UI_SCREEN_WIDTH];

/* Current compositing target: all drawing is clipped to this rect.
 * Typically a single scanline: {x, y, w, 1}. */
static ui_rect_t g_target = {0, 0, UI_SCREEN_WIDTH, 1};

/*=============================================================================
 *  Helper Functions
 *=============================================================================*/

static inline int16_t ui_max(int16_t a, int16_t b) { return a > b ? a : b; }
static inline int16_t ui_min(int16_t a, int16_t b) { return a < b ? a : b; }

static bool rect_intersect(const ui_rect_t *a, const ui_rect_t *b, ui_rect_t *out)
{
    int16_t x1 = ui_max(a->x, b->x);
    int16_t y1 = ui_max(a->y, b->y);
    int16_t x2 = ui_min(a->x + a->w, b->x + b->w);
    int16_t y2 = ui_min(a->y + a->h, b->y + b->h);
    if (x2 <= x1 || y2 <= y1) return false;
    out->x = x1; out->y = y1; out->w = x2 - x1; out->h = y2 - y1;
    return true;
}

/* Fill line_buf[x..x+w-1] with color. Clipped to target x-range. */
static void lb_fill(int16_t x, int16_t w, ui_color_t color)
{
    int16_t x1 = ui_max(x, g_target.x);
    int16_t x2 = ui_min(x + w, g_target.x + g_target.w);
    for (int16_t i = x1; i < x2; i++) {
        g_line_buf[i] = color;
    }
}

/* Set a single pixel in line_buf. Clipped to target x-range. */
static inline void lb_pixel(int16_t x, ui_color_t color)
{
    if (x >= g_target.x && x < g_target.x + g_target.w) {
        g_line_buf[x] = color;
    }
}

/* Check if y is within the current target row(s) */
static inline bool in_target_y(int16_t y)
{
    return y >= g_target.y && y < g_target.y + g_target.h;
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void ui_render_init(void)
{
    printf("[ui_render_init] compositing engine v4.0\r\n");
    g_target.x = 0;
    g_target.y = 0;
    g_target.w = UI_SCREEN_WIDTH;
    g_target.h = 1;
    memset(g_line_buf, 0, sizeof(g_line_buf));
    printf("[ui_render_init] done\r\n");
}

/*=============================================================================
 *  Compositing Engine API
 *=============================================================================*/

void ui_render_begin_rect(const ui_rect_t *rect)
{
    if (rect) g_target = *rect;
}

void ui_render_get_target(ui_rect_t *rect)
{
    if (rect) *rect = g_target;
}

ui_color_t* ui_render_get_line_buf(void)
{
    return g_line_buf;
}

void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color)
{
    lb_fill(x_start, width, color);
}

void ui_render_set_line_pixel(int16_t x, ui_color_t color)
{
    lb_pixel(x, color);
}

ui_color_t ui_render_get_line_pixel(int16_t x)
{
    if (x >= 0 && x < UI_SCREEN_WIDTH) return g_line_buf[x];
    return 0;
}

void ui_render_flush_row(int16_t y, int16_t x_start, int16_t width)
{
    if (width <= 0 || y < 0 || y >= UI_SCREEN_HEIGHT) return;
    if (x_start < 0) { width += x_start; x_start = 0; }
    if (x_start + width > UI_SCREEN_WIDTH) width = UI_SCREEN_WIDTH - x_start;
    if (width <= 0) return;

    s_driver.set_window((uint16_t)x_start, (uint16_t)y,
                        (uint16_t)(x_start + width - 1), (uint16_t)y);
    s_driver.write_buffer(&g_line_buf[x_start], (uint32_t)width);
}

void ui_render_flush_rows(int16_t y_start, int16_t row_count,
                          int16_t x_start, int16_t width)
{
    if (width <= 0 || row_count <= 0) return;
    if (y_start < 0) { row_count += y_start; y_start = 0; }
    if (y_start + row_count > UI_SCREEN_HEIGHT) row_count = UI_SCREEN_HEIGHT - y_start;
    if (x_start < 0) { width += x_start; x_start = 0; }
    if (x_start + width > UI_SCREEN_WIDTH) width = UI_SCREEN_WIDTH - x_start;
    if (width <= 0 || row_count <= 0) return;

    s_driver.set_window((uint16_t)x_start, (uint16_t)y_start,
                        (uint16_t)(x_start + width - 1),
                        (uint16_t)(y_start + row_count - 1));
    for (int16_t r = 0; r < row_count; r++) {
        s_driver.write_buffer(&g_line_buf[x_start], (uint32_t)width);
    }
}

/*=============================================================================
 *  Pixel & Line Drawing (write to line_buf, clipped to g_target)
 *=============================================================================*/

void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color)
{
    if (in_target_y(y)) lb_pixel(x, color);
}

void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color)
{
    if (!in_target_y(y) || w <= 0) return;
    lb_fill(x, w, color);
}

void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color)
{
    if (h <= 0) return;
    int16_t y1 = ui_max(y, g_target.y);
    int16_t y2 = ui_min(y + h, g_target.y + g_target.h);
    for (int16_t row = y1; row < y2; row++) {
        lb_pixel(x, color);
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
        if (in_target_y(y1)) lb_pixel(x1, color);
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
    if (!rect_intersect(rect, &g_target, &r)) return;
    /* Only fill within the target row(s) */
    lb_fill(r.x, r.w, color);
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
        if (inner.w > 0 && inner.h > 0)
            ui_draw_rect_border(&inner, color, thickness - 1);
    }
}

void ui_draw_rect(const ui_rect_t *rect, ui_color_t fill, ui_color_t border, int16_t thickness)
{
    ui_draw_fill_rect(rect, fill);
    if (thickness > 0) ui_draw_rect_border(rect, border, thickness);
}

/*=============================================================================
 *  Rounded Rectangle Drawing
 *
 *  Each row of the corner is drawn as a horizontal span in the line buffer.
 *  The page manager calls us once per target row, so we only emit the
 *  span for the current row.
 *=============================================================================*/

/* Draw a single row of a filled circle quadrant.
 * cx, cy: center; r: radius; q: quadrant (0=TL, 1=TR, 2=BL, 3=BR)
 * Only draws if the quadrant row intersects g_target.y */
static void fill_circle_quad_row(int16_t cx, int16_t cy, int16_t r,
                                  ui_color_t color, uint8_t q)
{
    /* Use midpoint circle to find the x-extents at each y in target range */
    int16_t y_lo = ui_max(cy - r, g_target.y);
    int16_t y_hi = ui_min(cy + r, g_target.y + g_target.h - 1);

    for (int16_t y = y_lo; y <= y_hi; y++) {
        int16_t dy = y - cy;
        /* x extent at this y: x = sqrt(r^2 - dy^2) */
        int32_t dx2 = (int32_t)r * r - (int32_t)dy * dy;
        if (dx2 < 0) continue;
        int16_t dx = 0;
        /* Integer sqrt approximation */
        for (dx = r; dx > 0; dx--) {
            if ((int32_t)dx * dx <= dx2) break;
        }

        switch (q) {
        case 0: /* top-left: y <= cy, x <= cx */
            if (y <= cy) lb_fill(cx - dx, dx + 1, color);
            break;
        case 1: /* top-right: y <= cy, x >= cx */
            if (y <= cy) lb_fill(cx, dx + 1, color);
            break;
        case 2: /* bottom-left: y >= cy, x <= cx */
            if (y >= cy) lb_fill(cx - dx, dx + 1, color);
            break;
        case 3: /* bottom-right: y >= cy, x >= cx */
            if (y >= cy) lb_fill(cx, dx + 1, color);
            break;
        }
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

    /* Center band (full width, between the two corner rows) */
    ui_rect_t center = { rect->x, rect->y + r, rect->w, rect->h - 2 * r };
    ui_draw_fill_rect(&center, color);

    /* Top and bottom straight bands */
    ui_rect_t top = { rect->x + r, rect->y, rect->w - 2 * r, r };
    ui_draw_fill_rect(&top, color);
    ui_rect_t bot = { rect->x + r, rect->y + rect->h - r, rect->w - 2 * r, r };
    ui_draw_fill_rect(&bot, color);

    /* Four corner quadrants */
    fill_circle_quad_row(rect->x + r,           rect->y + r,           r, color, 0); /* TL */
    fill_circle_quad_row(rect->x + rect->w - r - 1, rect->y + r,       r, color, 1); /* TR */
    fill_circle_quad_row(rect->x + r,           rect->y + rect->h - r - 1, r, color, 2); /* BL */
    fill_circle_quad_row(rect->x + rect->w - r - 1, rect->y + rect->h - r - 1, r, color, 3); /* BR */
}

void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness)
{
    /* For simplicity, draw as a series of outline pixels per target row.
     * This is called rarely and thickness is usually 1-2px. */
    if (thickness <= 0) return;
    /* Top/bottom edges */
    ui_draw_hline(rect->x + radius, rect->y, rect->w - 2 * radius, color);
    ui_draw_hline(rect->x + radius, rect->y + rect->h - 1, rect->w - 2 * radius, color);
    /* Left/right edges */
    ui_draw_vline(rect->x, rect->y + radius, rect->h - 2 * radius, color);
    ui_draw_vline(rect->x + rect->w - 1, rect->y + radius, rect->h - 2 * radius, color);
    /* Corner arcs - use midpoint circle for outline only */
    int16_t r = radius;
    if (r > rect->w / 2) r = rect->w / 2;
    if (r > rect->h / 2) r = rect->h / 2;
    if (r <= 0) return;

    int16_t cx[4] = { rect->x + r, rect->x + rect->w - r - 1,
                       rect->x + r, rect->x + rect->w - r - 1 };
    int16_t cy[4] = { rect->y + r, rect->y + r,
                       rect->y + rect->h - r - 1, rect->y + rect->h - r - 1 };

    int16_t x = 0, y = r;
    int16_t d = 3 - 2 * r;
    while (x <= y) {
        for (int q = 0; q < 4; q++) {
            int16_t sx = (q & 1) ? cx[q] + x : cx[q] - x;
            int16_t sy = (q & 1) ? cx[q] + y : cx[q] - y;
            int16_t px_sy = (q < 2) ? cy[q] - y : cy[q] + y;
            int16_t px_sx = (q < 2) ? cy[q] - x : cy[q] + x;
            (void)sy;
            ui_draw_pixel(sx, px_sy, color);
            ui_draw_pixel((q & 1) ? cx[q] + y : cx[q] - y, px_sx, color);
        }
        if (d < 0) { d += 4 * x + 6; }
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

void ui_draw_round_rect(const ui_rect_t *rect, int16_t radius, ui_color_t fill, ui_color_t border, int16_t thickness)
{
    ui_draw_fill_round_rect(rect, radius, fill);
    if (thickness > 0) ui_draw_round_rect_border(rect, radius, border, thickness);
}

/*=============================================================================
 *  Circle Drawing
 *=============================================================================*/

void ui_draw_fill_circle(int16_t cx, int16_t cy, int16_t r, ui_color_t color)
{
    if (r <= 0) return;
    int16_t y_lo = ui_max(cy - r, g_target.y);
    int16_t y_hi = ui_min(cy + r, g_target.y + g_target.h - 1);

    for (int16_t y = y_lo; y <= y_hi; y++) {
        int16_t dy = y - cy;
        int32_t dx2 = (int32_t)r * r - (int32_t)dy * dy;
        if (dx2 < 0) continue;
        int16_t dx = 0;
        for (dx = r; dx > 0; dx--) {
            if ((int32_t)dx * dx <= dx2) break;
        }
        lb_fill(cx - dx, 2 * dx + 1, color);
    }
}

void ui_draw_circle_border(int16_t cx, int16_t cy, int16_t r, ui_color_t color, int16_t thickness)
{
    if (r <= 0) return;
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
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
    if (thickness > 1 && r > 1)
        ui_draw_circle_border(cx, cy, r - 1, color, thickness - 1);
}

/*=============================================================================
 *  Icon Drawing (1bpp bitmap)
 *=============================================================================*/

void ui_draw_icon(int16_t x, int16_t y, const uint8_t *bitmap,
                  int16_t w, int16_t h, ui_color_t color)
{
    if (!bitmap) return;

    int16_t y_lo = ui_max(y, g_target.y);
    int16_t y_hi = ui_min(y + h, g_target.y + g_target.h);

    for (int16_t row = y_lo; row < y_hi; row++) {
        int16_t ry = row - y;  /* row index within icon */
        uint16_t byte_idx = ry * ((w + 7) / 8);
        for (int16_t col = 0; col < w; col++) {
            if (bitmap[byte_idx + col / 8] & (0x80 >> (col % 8))) {
                lb_pixel(x + col, color);
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
    int16_t lo = 0, hi = (int16_t)font->glyph_count - 1;
    while (lo <= hi) {
        int16_t mid = (lo + hi) / 2;
        if (font->glyphs[mid].unicode == unicode) return &font->glyphs[mid];
        if (font->glyphs[mid].unicode < unicode) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

/* Draw a single glyph into the line buffer (current target row only) */
static void draw_glyph(int16_t x, int16_t y, const ui_glyph_t *glyph,
                       const ui_font_t *font, ui_color_t color, ui_color_t bg)
{
    int16_t gx = x + glyph->x_offset;
    int16_t gy = y + glyph->y_offset;
    uint8_t bpp = font->bpp;
    uint8_t mask = (1 << bpp) - 1;

    /* Only process rows within target y range */
    int16_t y_lo = ui_max(gy, g_target.y);
    int16_t y_hi = ui_min(gy + glyph->height, g_target.y + g_target.h);

    for (int16_t row = y_lo; row < y_hi; row++) {
        uint16_t row_idx = row - gy;
        uint32_t bit_idx = (uint32_t)row_idx * bpp * glyph->width;

        if (bg != UI_COLOR_TRANSPARENT) {
            /* Fill background first */
            int16_t x1 = ui_max(gx, g_target.x);
            int16_t x2 = ui_min(gx + glyph->width, g_target.x + g_target.w);
            for (int16_t col = x1; col < x2; col++) {
                g_line_buf[col] = bg;
            }
            /* Blend foreground pixels */
            for (int16_t col = 0; col < glyph->width; col++) {
                uint32_t byte_idx = (bit_idx + (uint32_t)col * bpp) / 8;
                uint32_t shift = 8 - bpp - ((bit_idx + (uint32_t)col * bpp) % 8);
                uint8_t alpha = (glyph->bitmap[byte_idx] >> shift) & mask;
                int16_t px = gx + col;
                if (px < g_target.x || px >= g_target.x + g_target.w) continue;
                if (bpp == 1) {
                    if (alpha) g_line_buf[px] = color;
                } else {
                    if (alpha > 0) {
                        uint8_t a8 = (uint16_t)alpha * 255 / mask;
                        g_line_buf[px] = ui_color_blend(color, bg, a8);
                    }
                }
            }
        } else {
            /* Transparent background: only draw foreground pixels */
            for (int16_t col = 0; col < glyph->width; col++) {
                uint32_t byte_idx = (bit_idx + (uint32_t)col * bpp) / 8;
                uint32_t shift = 8 - bpp - ((bit_idx + (uint32_t)col * bpp) % 8);
                uint8_t alpha = (glyph->bitmap[byte_idx] >> shift) & mask;
                if (alpha > 0) {
                    lb_pixel(gx + col, color);
                }
            }
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
            ui_draw_fill_rect(&box, color);
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
 *  Screen Operations (direct GRAM access)
 *=============================================================================*/

void ui_screen_clear(ui_color_t color)
{
    s_driver.clear(color);
}

void ui_full_refresh(void)
{
    s_driver.clear(UI_COLOR_BG_MAIN);
}
