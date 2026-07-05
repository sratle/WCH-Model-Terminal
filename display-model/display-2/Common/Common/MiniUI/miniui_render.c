/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_render.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI 1bpp compositing rendering engine for E-ink.
*                      All drawing primitives write to a 1bpp line buffer.
*                      The page manager composites widgets row-by-row and
*                      flushes to a full frame buffer. After compositing,
*                      dirty regions are sent to e-paper via partial refresh.
********************************************************************************/
#include "miniui_render.h"
#include "../Eink/epaper.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Frame Buffers
 *
 *  Two full-frame buffers: one for "old" data (currently on screen),
 *  one for "new" data (being composited). After refresh, they swap.
 *  Each buffer: 648 * 480 / 8 = 38880 bytes.
 *  Total: 77760 bytes, fits in 128K RAM.
 *=============================================================================*/

static uint8_t g_frame_new[EPD_FRAME_SIZE];  /* New frame being composited */
static uint8_t g_frame_old[EPD_FRAME_SIZE];  /* Old frame (on screen) */

/*=============================================================================
 *  1bpp Line Buffer (for compositing)
 *
 *  One full row: 648/8 = 81 bytes. We batch UI_COMPOSE_BATCH rows.
 *  81 * 8 = 648 bytes.
 *=============================================================================*/

#define COMPOSE_BATCH  UI_COMPOSE_BATCH
#define LINE_BUF_BYTES  UI_SCREEN_ROW_BYTES  /* 81 bytes per row */

static uint8_t g_compose_buf[LINE_BUF_BYTES * COMPOSE_BATCH]; /* 648 bytes */

/*=============================================================================
 *  Compositing Target
 *=============================================================================*/

static ui_rect_t g_target = {0, 0, UI_SCREEN_WIDTH, 1};

#define TARGET_STACK_DEPTH 4
static ui_rect_t g_target_stack[TARGET_STACK_DEPTH];
static int8_t    g_target_sp = 0;

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

static inline bool in_target_y(int16_t y)
{
    return y >= g_target.y && y < g_target.y + g_target.h;
}

/*=============================================================================
 *  1bpp Line Buffer Operations
 *
 *  Pixel format: bit=1 → black, bit=0 → white (matches EPD_BLACK/EPD_WHITE)
 *  MSB = leftmost pixel in each byte.
 *=============================================================================*/

/* Set a pixel in compose_buf at (x, y) */
static inline void lb_pixel(int16_t x, int16_t y, ui_color_t color)
{
    if (x < g_target.x || x >= g_target.x + g_target.w) return;
    int16_t row_idx = y - g_target.y;
    if (row_idx < 0 || row_idx >= COMPOSE_BATCH) return;
    uint16_t byte_idx = (uint16_t)row_idx * LINE_BUF_BYTES + (x / 8);
    uint8_t bit_mask = 0x80 >> (x % 8);
    if (color == UI_COLOR_BLACK)
        g_compose_buf[byte_idx] |= bit_mask;
    else
        g_compose_buf[byte_idx] &= ~bit_mask;
}

/* Fill a horizontal span in compose_buf for row y */
static void lb_fill(int16_t x, int16_t y, int16_t w, ui_color_t color)
{
    int16_t x1 = ui_max(x, g_target.x);
    int16_t x2 = ui_min(x + w, g_target.x + g_target.w);
    if (x2 <= x1) return;
    int16_t row_idx = y - g_target.y;
    if (row_idx < 0 || row_idx >= COMPOSE_BATCH) return;

    uint8_t *row = &g_compose_buf[(uint16_t)row_idx * LINE_BUF_BYTES];
    uint8_t fill_val = (color == UI_COLOR_BLACK) ? 0xFF : 0x00;

    /* Fast path: fill whole bytes */
    int16_t start_byte = x1 / 8;
    int16_t end_byte = (x2 - 1) / 8;

    if (start_byte == end_byte) {
        /* All pixels within one byte */
        uint8_t mask = 0;
        for (int16_t i = x1; i < x2; i++)
            mask |= (0x80 >> (i % 8));
        if (color == UI_COLOR_BLACK)
            row[start_byte] |= mask;
        else
            row[start_byte] &= ~mask;
    } else {
        /* Handle first partial byte */
        if (x1 % 8 != 0) {
            uint8_t mask = 0xFF >> (x1 % 8);
            if (color == UI_COLOR_BLACK)
                row[start_byte] |= mask;
            else
                row[start_byte] &= ~mask;
            start_byte++;
        }
        /* Handle last partial byte */
        if ((x2 % 8) != 0) {
            uint8_t mask = ~(0xFF >> (x2 % 8));
            if (color == UI_COLOR_BLACK)
                row[end_byte] |= mask;
            else
                row[end_byte] &= ~mask;
            end_byte--;
        }
        /* Fill whole bytes in between */
        for (int16_t b = start_byte; b <= end_byte; b++)
            row[b] = fill_val;
    }
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void ui_render_init(void)
{
    printf("[ui_render_init] e-ink 1bpp compositing engine v1.0\r\n");
    g_target.x = 0;
    g_target.y = 0;
    g_target.w = UI_SCREEN_WIDTH;
    g_target.h = 1;
    g_target_sp = 0;
    /* Initialize both frame buffers to white (all 0x00) */
    memset(g_frame_new, 0, sizeof(g_frame_new));
    memset(g_frame_old, 0, sizeof(g_frame_old));
    memset(g_compose_buf, 0, sizeof(g_compose_buf));
    printf("[ui_render_init] done\r\n");
}

/*=============================================================================
 *  Compositing Engine API
 *=============================================================================*/

void ui_render_begin_rect(const ui_rect_t *rect)
{
    if (rect) g_target = *rect;
    g_target_sp = 0;
    /* Clear compose buffer to white for the target rows */
    memset(g_compose_buf, 0, sizeof(g_compose_buf));
}

void ui_render_get_target(ui_rect_t *rect)
{
    if (rect) *rect = g_target;
}

void ui_render_push_target(const ui_rect_t *clip)
{
    if (!clip) return;
    if (g_target_sp < TARGET_STACK_DEPTH) {
        g_target_stack[g_target_sp++] = g_target;
    }
    int16_t x1 = g_target.x > clip->x ? g_target.x : clip->x;
    int16_t y1 = g_target.y > clip->y ? g_target.y : clip->y;
    int16_t x2 = (g_target.x + g_target.w) < (clip->x + clip->w)
                 ? (g_target.x + g_target.w) : (clip->x + clip->w);
    int16_t y2 = (g_target.y + g_target.h) < (clip->y + clip->h)
                 ? (g_target.y + g_target.h) : (clip->y + clip->h);
    if (x2 <= x1 || y2 <= y1) {
        g_target.w = 0;
        g_target.h = 0;
    } else {
        g_target.x = x1; g_target.y = y1;
        g_target.w = x2 - x1; g_target.h = y2 - y1;
    }
}

void ui_render_pop_target(void)
{
    if (g_target_sp > 0) {
        g_target = g_target_stack[--g_target_sp];
    }
}

uint8_t* ui_render_get_line_buf(void)
{
    return g_compose_buf;
}

void ui_render_fill_line_buf(int16_t x_start, int16_t width, ui_color_t color)
{
    for (int16_t y = g_target.y; y < g_target.y + g_target.h; y++) {
        lb_fill(x_start, y, width, color);
    }
}

void ui_render_set_line_pixel(int16_t x, ui_color_t color)
{
    lb_pixel(x, g_target.y, color);
}

ui_color_t ui_render_get_line_pixel(int16_t x)
{
    if (x < 0 || x >= UI_SCREEN_WIDTH) return UI_COLOR_WHITE;
    uint16_t byte_idx = x / 8;
    uint8_t bit_mask = 0x80 >> (x % 8);
    return (g_compose_buf[byte_idx] & bit_mask) ? UI_COLOR_BLACK : UI_COLOR_WHITE;
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

    /* Copy from compose_buf to frame buffer.
     * We need to merge the composited region into the full frame buffer.
     * For each row, copy the relevant bytes from compose_buf to frame_new. */
    for (int16_t r = 0; r < row_count; r++) {
        int16_t src_row = y_start + r - g_target.y;
        if (src_row < 0 || src_row >= COMPOSE_BATCH) continue;

        int16_t y = y_start + r;
        uint8_t *src = &g_compose_buf[(uint16_t)src_row * LINE_BUF_BYTES];
        uint8_t *dst = &g_frame_new[(uint32_t)y * UI_SCREEN_ROW_BYTES];

        /* Align x_start and width to byte boundaries for the frame buffer */
        int16_t x1_byte = x_start / 8;
        int16_t x2_byte = (x_start + width + 7) / 8;

        /* Handle partial first byte */
        if (x_start % 8 != 0) {
            uint8_t mask = 0xFF >> (x_start % 8);
            dst[x1_byte] = (dst[x1_byte] & ~mask) | (src[x1_byte] & mask);
            x1_byte++;
        }

        /* Handle partial last byte */
        int16_t last_byte = (x_start + width - 1) / 8;
        if (x2_byte > x1_byte && (x_start + width) % 8 != 0 && last_byte >= x1_byte) {
            uint8_t mask = ~(0xFF >> ((x_start + width) % 8));
            dst[last_byte] = (dst[last_byte] & ~mask) | (src[last_byte] & mask);
            x2_byte = last_byte;
        }

        /* Copy whole bytes */
        for (int16_t b = x1_byte; b < x2_byte; b++) {
            dst[b] = src[b];
        }
    }
}

/*=============================================================================
 *  Pixel & Line Drawing
 *=============================================================================*/

void ui_draw_pixel(int16_t x, int16_t y, ui_color_t color)
{
    if (in_target_y(y)) lb_pixel(x, y, color);
}

void ui_draw_hline(int16_t x, int16_t y, int16_t w, ui_color_t color)
{
    if (!in_target_y(y) || w <= 0) return;
    lb_fill(x, y, w, color);
}

void ui_draw_vline(int16_t x, int16_t y, int16_t h, ui_color_t color)
{
    if (h <= 0) return;
    int16_t y1 = ui_max(y, g_target.y);
    int16_t y2 = ui_min(y + h, g_target.y + g_target.h);
    for (int16_t row = y1; row < y2; row++) {
        lb_pixel(x, row, color);
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
        if (in_target_y(y1)) lb_pixel(x1, y1, color);
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
    for (int16_t y = r.y; y < r.y + r.h; y++) {
        lb_fill(r.x, y, r.w, color);
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
 *=============================================================================*/

static void fill_circle_quad_row(int16_t cx, int16_t cy, int16_t r,
                                  ui_color_t color, uint8_t q)
{
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

        switch (q) {
        case 0: if (y <= cy) lb_fill(cx - dx, y, dx + 1, color); break;
        case 1: if (y <= cy) lb_fill(cx, y, dx + 1, color); break;
        case 2: if (y >= cy) lb_fill(cx - dx, y, dx + 1, color); break;
        case 3: if (y >= cy) lb_fill(cx, y, dx + 1, color); break;
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

    ui_rect_t center = { rect->x, rect->y + r, rect->w, rect->h - 2 * r };
    ui_draw_fill_rect(&center, color);

    ui_rect_t top = { rect->x + r, rect->y, rect->w - 2 * r, r };
    ui_draw_fill_rect(&top, color);
    ui_rect_t bot = { rect->x + r, rect->y + rect->h - r, rect->w - 2 * r, r };
    ui_draw_fill_rect(&bot, color);

    fill_circle_quad_row(rect->x + r, rect->y + r, r, color, 0);
    fill_circle_quad_row(rect->x + rect->w - r - 1, rect->y + r, r, color, 1);
    fill_circle_quad_row(rect->x + r, rect->y + rect->h - r - 1, r, color, 2);
    fill_circle_quad_row(rect->x + rect->w - r - 1, rect->y + rect->h - r - 1, r, color, 3);
}

void ui_draw_round_rect_border(const ui_rect_t *rect, int16_t radius, ui_color_t color, int16_t thickness)
{
    if (thickness <= 0) return;
    int16_t r = radius;
    if (r > rect->w / 2) r = rect->w / 2;
    if (r > rect->h / 2) r = rect->h / 2;

    ui_draw_hline(rect->x + r, rect->y, rect->w - 2 * r, color);
    ui_draw_hline(rect->x + r, rect->y + rect->h - 1, rect->w - 2 * r, color);
    ui_draw_vline(rect->x, rect->y + r, rect->h - 2 * r, color);
    ui_draw_vline(rect->x + rect->w - 1, rect->y + r, rect->h - 2 * r, color);

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
            int16_t px_sy = (q < 2) ? cy[q] - y : cy[q] + y;
            ui_draw_pixel(sx, px_sy, color);
            ui_draw_pixel((q & 1) ? cx[q] + y : cx[q] - y,
                          (q < 2) ? cy[q] - x : cy[q] + x, color);
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
        lb_fill(cx - dx, y, 2 * dx + 1, color);
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
        int16_t ry = row - y;
        uint16_t byte_idx = ry * ((w + 7) / 8);
        for (int16_t col = 0; col < w; col++) {
            if (bitmap[byte_idx + col / 8] & (0x80 >> (col % 8))) {
                lb_pixel(x + col, row, color);
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
 *  Text Drawing (1bpp only for E-ink)
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

static void draw_glyph(int16_t x, int16_t y, const ui_glyph_t *glyph,
                       const ui_font_t *font, ui_color_t color, ui_color_t bg)
{
    int16_t gx = x + glyph->x_offset;
    int16_t gy = y + glyph->y_offset;

    int16_t y_lo = ui_max(gy, g_target.y);
    int16_t y_hi = ui_min(gy + glyph->height, g_target.y + g_target.h);

    for (int16_t row = y_lo; row < y_hi; row++) {
        uint16_t row_idx = row - gy;
        uint16_t bit_idx = row_idx * glyph->width;  /* 1bpp */

        if (bg != UI_COLOR_TRANSPARENT) {
            /* Fill background first for this glyph row */
            int16_t x1 = ui_max(gx, g_target.x);
            int16_t x2 = ui_min(gx + glyph->width, g_target.x + g_target.w);
            if (x2 > x1) {
                int16_t src_row = row - g_target.y;
                if (src_row >= 0 && src_row < COMPOSE_BATCH) {
                    uint8_t *line = &g_compose_buf[(uint16_t)src_row * LINE_BUF_BYTES];
                    for (int16_t col = x1; col < x2; col++) {
                        uint16_t byte_idx = col / 8;
                        uint8_t bit_mask = 0x80 >> (col % 8);
                        if (bg == UI_COLOR_BLACK)
                            line[byte_idx] |= bit_mask;
                        else
                            line[byte_idx] &= ~bit_mask;
                    }
                }
            }
        }

        /* Draw foreground pixels */
        for (int16_t col = 0; col < glyph->width; col++) {
            uint16_t bmp_byte = (bit_idx + col) / 8;
            uint8_t bmp_mask = 0x80 >> ((bit_idx + col) % 8);
            if (glyph->bitmap[bmp_byte] & bmp_mask) {
                int16_t px = gx + col;
                if (px >= g_target.x && px < g_target.x + g_target.w) {
                    int16_t src_row = row - g_target.y;
                    if (src_row >= 0 && src_row < COMPOSE_BATCH) {
                        uint16_t byte_idx = (uint16_t)src_row * LINE_BUF_BYTES + (px / 8);
                        uint8_t bit_mask = 0x80 >> (px % 8);
                        if (color == UI_COLOR_BLACK)
                            g_compose_buf[byte_idx] |= bit_mask;
                        else
                            g_compose_buf[byte_idx] &= ~bit_mask;
                    }
                }
            }
        }
    }
}

void ui_draw_text(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color)
{
    ui_draw_text_bg(x, y, text, font, color, UI_COLOR_TRANSPARENT);
}

void ui_draw_text_bg(int16_t x, int16_t y, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg)
{
    if (!text || !font) return;
    int16_t cursor_x = x;
    while (*text) {
        uint8_t ch = (uint8_t)*text++;
        const ui_glyph_t *glyph = find_glyph(font, ch);
        if (!glyph) {
            cursor_x += font->height / 2;
            continue;
        }
        draw_glyph(cursor_x, y, glyph, font, color, bg);
        cursor_x += glyph->advance;
    }
}

void ui_draw_text_in_rect(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, uint8_t align)
{
    ui_draw_text_in_rect_bg(rect, text, font, color, UI_COLOR_TRANSPARENT, align);
}

void ui_draw_text_in_rect_bg(const ui_rect_t *rect, const char *text, const ui_font_t *font, ui_color_t color, ui_color_t bg, uint8_t align)
{
    if (!text || !font || !rect) return;
    int16_t text_w = ui_text_width(text, font);
    int16_t x = rect->x;
    int16_t y = rect->y + (rect->h - font->height) / 2;

    switch (align) {
    case 0: /* left */   x = rect->x + 4; break;
    case 1: /* center */ x = rect->x + (rect->w - text_w) / 2; break;
    case 2: /* right */  x = rect->x + rect->w - text_w - 4; break;
    default: break;
    }

    ui_draw_text_bg(x, y, text, font, color, bg);
}

int16_t ui_text_width(const char *text, const ui_font_t *font)
{
    if (!text || !font) return 0;
    int16_t w = 0;
    while (*text) {
        const ui_glyph_t *glyph = find_glyph(font, (uint8_t)*text);
        if (glyph) w += glyph->advance;
        else w += font->height / 2;
        text++;
    }
    return w;
}

/*=============================================================================
 *  Touch Cursor Drawing
 *
 *  Draws a small arrow cursor at (x, y) using pixel-level drawing.
 *  Cursor shape: 8x11 pixel arrow pointing top-left, transparent background.
 *  White outline ensures visibility on both light and dark backgrounds.
 *=============================================================================*/

static const uint8_t s_cursor_bitmap[] = {
    0x80,  /* row 0:  #....... */
    0xC0,  /* row 1:  ##...... */
    0xE0,  /* row 2:  ###..... */
    0xF0,  /* row 3:  ####.... */
    0xF8,  /* row 4:  #####... */
    0xFC,  /* row 5:  ######.. */
    0xF0,  /* row 6:  ####.... */
    0xD8,  /* row 7:  ##.##... */
    0x8C,  /* row 8:  #...##.. */
    0x06,  /* row 9:  .....##. */
    0x02,  /* row 10: ......#. */
};

#define CURSOR_BITMAP_W  8
#define CURSOR_BITMAP_H  11

void ui_draw_touch_cursor(int16_t x, int16_t y)
{
    ui_color_t outline = UI_COLOR_WHITE;
    ui_color_t fill    = UI_COLOR_BLACK;

    /* Pass 1: white outline — draw each set pixel shifted by ±1 */
    for (int16_t row = 0; row < CURSOR_BITMAP_H; row++) {
        uint8_t b = s_cursor_bitmap[row];
        for (int16_t col = 0; col < CURSOR_BITMAP_W; col++) {
            if (b & (0x80 >> col)) {
                for (int16_t dy = -1; dy <= 1; dy++) {
                    for (int16_t dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int16_t px = x + col + dx;
                        int16_t py = y + row + dy;
                        /* Only draw outline if neighbor is NOT part of cursor */
                        int16_t nr = row + dy;
                        int16_t nc = col + dx;
                        bool neighbor_set = (nr >= 0 && nr < CURSOR_BITMAP_H &&
                                             nc >= 0 && nc < CURSOR_BITMAP_W &&
                                             (s_cursor_bitmap[nr] & (0x80 >> nc)));
                        if (!neighbor_set) {
                            ui_draw_pixel(px, py, outline);
                        }
                    }
                }
            }
        }
    }

    /* Pass 2: black fill */
    for (int16_t row = 0; row < CURSOR_BITMAP_H; row++) {
        int16_t py = y + row;
        uint8_t b = s_cursor_bitmap[row];
        for (int16_t col = 0; col < CURSOR_BITMAP_W; col++) {
            if (b & (0x80 >> col)) {
                ui_draw_pixel(x + col, py, fill);
            }
        }
    }
}

/*=============================================================================
 *  E-ink Refresh Operations
 *=============================================================================*/

void ui_render_flush_to_display(void)
{
    /* Compare new and old frame buffers to find the dirty region.
     * For simplicity, we do a full refresh if there are any differences.
     * TODO: optimize to find bounding box of changes for partial refresh. */
    uint32_t black_pixels = 0;
    for (uint32_t i = 0; i < EPD_FRAME_SIZE; i++) {
        uint8_t v = g_frame_new[i];
        uint8_t c = 0;
        while (v) { c++; v &= v - 1; }
        black_pixels += c;
    }
    printf("[render] flushing %lu black pixels (old vs new %d)\r\n",
           black_pixels, memcmp(g_frame_old, g_frame_new, EPD_FRAME_SIZE) != 0);
    Epaper_DisplayImageDiff(g_frame_old, g_frame_new);
    Epaper_Update();

    /* Swap buffers: new becomes old */
    memcpy(g_frame_old, g_frame_new, EPD_FRAME_SIZE);
}

void ui_screen_clear(ui_color_t color)
{
    uint8_t fill = (color == UI_COLOR_BLACK) ? 0xFF : 0x00;
    memset(g_frame_new, fill, sizeof(g_frame_new));
}

uint8_t* ui_render_get_frame_buf(void)
{
    return g_frame_new;
}

uint8_t* ui_render_get_old_buf(void)
{
    return g_frame_old;
}

void ui_render_swap_buffers(void)
{
    memcpy(g_frame_old, g_frame_new, EPD_FRAME_SIZE);
}
