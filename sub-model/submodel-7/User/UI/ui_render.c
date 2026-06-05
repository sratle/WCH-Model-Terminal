/**
 * @file ui_render.c
 * @brief UI 渲染层实现：帧缓冲操作 + 绘图原语 + 文字/图标渲染
 */
#include "ui_render.h"
#include "../ST7305/st7305.h"
#include <string.h>

/* External LCD driver instance (defined in app.c) */
extern struct st7305_stu g_lcd;

/*=============================================================================
 *  Init / Refresh
 *=============================================================================*/

void UI_Init(void)
{
    /* Framebuffer is managed by g_lcd.full_buffer, no extra init needed */
}

void UI_Clear(uint8_t color)
{
    if (g_lcd.full_buffer)
        memset(g_lcd.full_buffer, color ? 0xFF : 0x00, FULL_BUFFER_LENGTH);
    st7305_clear(&g_lcd, color);
}

void UI_Refresh(void)
{
    st7305_buf_refresh(&g_lcd);
}

/*=============================================================================
 *  Drawing Primitives
 *=============================================================================*/

void UI_DrawPixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= UI_SCREEN_WIDTH || y < 0 || y >= UI_SCREEN_HEIGHT)
        return;
    st7305_buf_draw_pix(&g_lcd, (uint16_t)x, (uint16_t)y, color);
}

void UI_DrawHLine(int16_t x, int16_t y, int16_t w, uint8_t color)
{
    int16_t i;
    if (y < 0 || y >= UI_SCREEN_HEIGHT)
        return;
    for (i = 0; i < w; i++)
    {
        int16_t px = x + i;
        if (px >= 0 && px < UI_SCREEN_WIDTH)
            st7305_buf_draw_pix(&g_lcd, (uint16_t)px, (uint16_t)y, color);
    }
}

void UI_DrawVLine(int16_t x, int16_t y, int16_t h, uint8_t color)
{
    int16_t i;
    if (x < 0 || x >= UI_SCREEN_WIDTH)
        return;
    for (i = 0; i < h; i++)
    {
        int16_t py = y + i;
        if (py >= 0 && py < UI_SCREEN_HEIGHT)
            st7305_buf_draw_pix(&g_lcd, (uint16_t)x, (uint16_t)py, color);
    }
}

void UI_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    int16_t ix, iy;
    int16_t x0 = (x < 0) ? 0 : x;
    int16_t y0 = (y < 0) ? 0 : y;
    int16_t x1 = (x + w > UI_SCREEN_WIDTH) ? UI_SCREEN_WIDTH : (x + w);
    int16_t y1 = (y + h > UI_SCREEN_HEIGHT) ? UI_SCREEN_HEIGHT : (y + h);

    for (iy = y0; iy < y1; iy++)
    {
        for (ix = x0; ix < x1; ix++)
        {
            st7305_buf_draw_pix(&g_lcd, (uint16_t)ix, (uint16_t)iy, color);
        }
    }
}

void UI_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color)
{
    UI_DrawHLine(x, y, w, color);                   /* Top */
    UI_DrawHLine(x, y + h - 1, w, color);           /* Bottom */
    UI_DrawVLine(x, y, h, color);                   /* Left */
    UI_DrawVLine(x + w - 1, y, h, color);           /* Right */
}

/*=============================================================================
 *  Text Rendering (1bpp glyph bitmap)
 *=============================================================================*/

/**
 * Find a glyph in the font by unicode code point.
 * Returns NULL if not found.
 */
static const ui_glyph_t *UI_FindGlyph(const ui_font_t *font, uint16_t code)
{
    uint16_t i;
    for (i = 0; i < font->glyph_count; i++)
    {
        if (font->glyphs[i].unicode == code)
            return &font->glyphs[i];
    }
    return NULL;
}

int16_t UI_DrawChar(int16_t x, int16_t y, char ch, uint8_t color,
                    const ui_font_t *font)
{
    const ui_glyph_t *g;
    int16_t gx, gy;
    uint16_t bx, by;
    uint16_t bytes_per_row;
    uint8_t bit;

    if (font == NULL)
        return 0;

    g = UI_FindGlyph(font, (uint16_t)(uint8_t)ch);
    if (g == NULL || g->bitmap == NULL)
    {
        /* Try space character as fallback */
        g = UI_FindGlyph(font, 32);
        if (g == NULL)
            return 0;
    }

    /* Glyph position */
    gx = x + g->x_offset;
    gy = y + g->y_offset;

    /* 1bpp bitmap: bits packed MSB-first, rows padded to byte boundary */
    bytes_per_row = (g->width + 7) / 8;

    for (by = 0; by < g->height; by++)
    {
        for (bx = 0; bx < g->width; bx++)
        {
            uint16_t byte_idx = by * bytes_per_row + (bx / 8);
            bit = 0x80 >> (bx % 8);

            if (g->bitmap[byte_idx] & bit)
            {
                int16_t px = gx + (int16_t)bx;
                int16_t py = gy + (int16_t)by;
                if (px >= 0 && px < UI_SCREEN_WIDTH &&
                    py >= 0 && py < UI_SCREEN_HEIGHT)
                {
                    st7305_buf_draw_pix(&g_lcd, (uint16_t)px, (uint16_t)py, color);
                }
            }
        }
    }

    return g->advance;
}

int16_t UI_DrawString(int16_t x, int16_t y, const char *str, uint8_t color,
                      const ui_font_t *font)
{
    int16_t cursor_x = x;

    if (str == NULL || font == NULL)
        return 0;

    while (*str)
    {
        int16_t adv = UI_DrawChar(cursor_x, y, *str, color, font);
        cursor_x += adv;
        str++;
    }

    return cursor_x - x;
}

int16_t UI_GetStringWidth(const char *str, const ui_font_t *font)
{
    int16_t width = 0;
    const ui_glyph_t *g;

    if (str == NULL || font == NULL)
        return 0;

    while (*str)
    {
        g = UI_FindGlyph(font, (uint16_t)(uint8_t)*str);
        if (g != NULL)
            width += g->advance;
        str++;
    }

    return width;
}

/*=============================================================================
 *  Icon / Bitmap Rendering
 *=============================================================================*/

void UI_DrawIcon(int16_t x, int16_t y, const uint8_t *bitmap,
                 int16_t w, int16_t h, uint8_t color)
{
    int16_t bx, by;
    uint16_t bytes_per_row;
    uint8_t bit;

    if (bitmap == NULL)
        return;

    bytes_per_row = (w + 7) / 8;

    for (by = 0; by < h; by++)
    {
        for (bx = 0; bx < w; bx++)
        {
            uint16_t byte_idx = by * bytes_per_row + (bx / 8);
            bit = 0x80 >> (bx % 8);

            if (bitmap[byte_idx] & bit)
            {
                int16_t px = x + bx;
                int16_t py = y + by;
                if (px >= 0 && px < UI_SCREEN_WIDTH &&
                    py >= 0 && py < UI_SCREEN_HEIGHT)
                {
                    st7305_buf_draw_pix(&g_lcd, (uint16_t)px, (uint16_t)py, color);
                }
            }
        }
    }
}

void UI_DrawBitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                   const uint8_t *data)
{
    int16_t bx, by;
    uint16_t bytes_per_row;
    uint8_t bit;

    if (data == NULL)
        return;

    bytes_per_row = (w + 7) / 8;

    for (by = 0; by < h; by++)
    {
        for (bx = 0; bx < w; bx++)
        {
            uint16_t byte_idx = by * bytes_per_row + (bx / 8);
            bit = 0x80 >> (bx % 8);

            /* 1 = black pixel, 0 = white pixel */
            uint8_t color = (data[byte_idx] & bit) ? UI_COLOR_BLACK : UI_COLOR_WHITE;

            int16_t px = x + bx;
            int16_t py = y + by;
            if (px >= 0 && px < UI_SCREEN_WIDTH &&
                py >= 0 && py < UI_SCREEN_HEIGHT)
            {
                st7305_buf_draw_pix(&g_lcd, (uint16_t)px, (uint16_t)py, color);
            }
        }
    }
}
