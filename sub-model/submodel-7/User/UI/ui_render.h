/**
 * @file ui_render.h
 * @brief Submodel-7 UI 渲染层：1bpp 帧缓冲 + 绘图原语 + 文字/图标渲染
 *        屏幕分辨率 122x250，黑白单色
 */
#ifndef __UI_RENDER_H__
#define __UI_RENDER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/*=============================================================================
 *  Screen Constants
 *=============================================================================*/

#define UI_SCREEN_WIDTH     122
#define UI_SCREEN_HEIGHT    250

/* Color constants (1bpp: 0=white, 1=black) */
#define UI_COLOR_WHITE      0
#define UI_COLOR_BLACK      1

/*=============================================================================
 *  Font Types (compatible with MiniUI font format, 1bpp only)
 *=============================================================================*/

/** Glyph descriptor for a single character */
typedef struct {
    uint16_t unicode;       /* Unicode code point */
    uint8_t  width;         /* Glyph width in pixels */
    uint8_t  height;        /* Glyph height in pixels */
    int8_t   x_offset;      /* X offset from cursor */
    int8_t   y_offset;      /* Y offset from cursor (baseline) */
    uint8_t  advance;       /* Cursor advance after this glyph */
    const uint8_t *bitmap;  /* Glyph bitmap data (1bpp) */
} ui_glyph_t;

/** Font descriptor */
typedef struct {
    const ui_glyph_t *glyphs;   /* Array of glyph descriptors */
    uint16_t glyph_count;       /* Number of glyphs */
    uint8_t  height;            /* Font height (max) */
    int8_t   baseline;          /* Baseline offset from top */
    uint8_t  bpp;               /* Bits per pixel: always 1 for submodel-7 */
} ui_font_t;

/*=============================================================================
 *  Init / Refresh
 *=============================================================================*/

/** Initialize UI render layer (sets up framebuffer pointer) */
void UI_Init(void);

/** Clear entire framebuffer to color, then refresh screen */
void UI_Clear(uint8_t color);

/** Flush framebuffer to ST7305 display */
void UI_Refresh(void);

/*=============================================================================
 *  Drawing Primitives
 *=============================================================================*/

/** Draw a single pixel */
void UI_DrawPixel(int16_t x, int16_t y, uint8_t color);

/** Draw horizontal line */
void UI_DrawHLine(int16_t x, int16_t y, int16_t w, uint8_t color);

/** Draw vertical line */
void UI_DrawVLine(int16_t x, int16_t y, int16_t h, uint8_t color);

/** Fill a rectangle */
void UI_FillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

/** Draw rectangle outline */
void UI_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);

/*=============================================================================
 *  Text Rendering
 *=============================================================================*/

/** Draw a single character, returns advance width */
int16_t UI_DrawChar(int16_t x, int16_t y, char ch, uint8_t color,
                    const ui_font_t *font);

/** Draw a null-terminated string, returns total advance width */
int16_t UI_DrawString(int16_t x, int16_t y, const char *str, uint8_t color,
                      const ui_font_t *font);

/** Calculate string pixel width */
int16_t UI_GetStringWidth(const char *str, const ui_font_t *font);

/*=============================================================================
 *  Icon / Bitmap Rendering
 *=============================================================================*/

/** Draw a 1bpp icon bitmap (MSB-first, row-major) */
void UI_DrawIcon(int16_t x, int16_t y, const uint8_t *bitmap,
                 int16_t w, int16_t h, uint8_t color);

/** Draw raw 1bpp bitmap data (row-major, MSB-first per row byte)
 *  Used for bulk image data from Core */
void UI_DrawBitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                   const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __UI_RENDER_H__ */
