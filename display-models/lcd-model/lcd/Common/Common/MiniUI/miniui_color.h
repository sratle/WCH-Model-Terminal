/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_color.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI color definitions and Macaron palette.
*                      RGB565 format optimized for embedded displays.
********************************************************************************/
#ifndef __MINIUI_COLOR_H
#define __MINIUI_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"

/*=============================================================================
 *  RGB565 Color Macros
 *=============================================================================*/

/* Convert 8-bit R/G/B to RGB565 */
#define UI_RGB(r, g, b) \
    ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

/* Convert hex color to RGB565 */
#define UI_HEX(c) UI_RGB(((c) >> 16) & 0xFF, ((c) >> 8) & 0xFF, (c) & 0xFF)

/*=============================================================================
 *  Basic Colors
 *=============================================================================*/

#define UI_COLOR_BLACK       UI_RGB(0x00, 0x00, 0x00)
#define UI_COLOR_WHITE       UI_RGB(0xFF, 0xFF, 0xFF)
#define UI_COLOR_RED         UI_RGB(0xFF, 0x00, 0x00)
#define UI_COLOR_GREEN       UI_RGB(0x00, 0xFF, 0x00)
#define UI_COLOR_BLUE        UI_RGB(0x00, 0x00, 0xFF)
#define UI_COLOR_YELLOW      UI_RGB(0xFF, 0xFF, 0x00)
#define UI_COLOR_CYAN        UI_RGB(0x00, 0xFF, 0xFF)
#define UI_COLOR_MAGENTA     UI_RGB(0xFF, 0x00, 0xFF)
#define UI_COLOR_GRAY        UI_RGB(0x80, 0x80, 0x80)
#define UI_COLOR_DARK_GRAY   UI_RGB(0x40, 0x40, 0x40)
#define UI_COLOR_LIGHT_GRAY  UI_RGB(0xC0, 0xC0, 0xC0)

/*=============================================================================
 *  Macaron Palette (Soft Pastel Colors)
 *=============================================================================*/

/* Primary theme color - Mint Green */
#define UI_COLOR_PRIMARY     UI_HEX(0x7EC8C8)  /* #7EC8C8 */

/* Secondary colors */
#define UI_COLOR_SECONDARY   UI_HEX(0xB8E0D2)  /* #B8E0D2 - Light mint */
#define UI_COLOR_ACCENT      UI_HEX(0xF5B7B1)  /* #F5B7B1 - Soft pink */
#define UI_COLOR_WARNING     UI_HEX(0xFAD7A0)  /* #FAD7A0 - Soft yellow */
#define UI_COLOR_DANGER      UI_HEX(0xE74C3C)  /* #E74C3C - Red (for alerts) */

/* Background colors */
#define UI_COLOR_BG_MAIN     UI_HEX(0xF5F5F5)  /* #F5F5F5 - Main background */
#define UI_COLOR_BG_CARD     UI_HEX(0xFFFFFF)  /* #FFFFFF - Card background */
#define UI_COLOR_BG_SIDEBAR  UI_HEX(0xF5F5F5)  /* #F5F5F5 - Sidebar background */

/* Text colors */
#define UI_COLOR_TEXT_PRIMARY   UI_HEX(0x4A4A4A)  /* #4A4A4A - Primary text */
#define UI_COLOR_TEXT_SECONDARY UI_HEX(0x9E9E9E)  /* #9E9E9E - Secondary text */
#define UI_COLOR_TEXT_DISABLED  UI_HEX(0xBDBDBD)  /* #BDBDBD - Disabled text */

/* Status indicator colors */
#define UI_COLOR_STATUS_ON      UI_HEX(0x7EC8C8)  /* Online - Mint green */
#define UI_COLOR_STATUS_OFF     UI_HEX(0xE0E0E0)  /* Offline - Light gray */
#define UI_COLOR_STATUS_WARN    UI_HEX(0xFAD7A0)  /* Warning - Soft yellow */

/*=============================================================================
 *  Color Utility Functions
 *=============================================================================*/

/* Blend two colors with alpha (0-255) */
static inline ui_color_t ui_color_blend(ui_color_t fg, ui_color_t bg, uint8_t alpha)
{
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;
    
    uint32_t r1 = (fg >> 11) & 0x1F;
    uint32_t g1 = (fg >> 5) & 0x3F;
    uint32_t b1 = fg & 0x1F;
    
    uint32_t r2 = (bg >> 11) & 0x1F;
    uint32_t g2 = (bg >> 5) & 0x3F;
    uint32_t b2 = bg & 0x1F;
    
    uint32_t r = (r1 * alpha + r2 * (255 - alpha)) / 255;
    uint32_t g = (g1 * alpha + g2 * (255 - alpha)) / 255;
    uint32_t b = (b1 * alpha + b2 * (255 - alpha)) / 255;
    
    return (r << 11) | (g << 5) | b;
}

/* Darken a color by percentage (0-100) */
static inline ui_color_t ui_color_darken(ui_color_t color, uint8_t pct)
{
    uint32_t r = ((color >> 11) & 0x1F) * (100 - pct) / 100;
    uint32_t g = ((color >> 5) & 0x3F) * (100 - pct) / 100;
    uint32_t b = (color & 0x1F) * (100 - pct) / 100;
    return (r << 11) | (g << 5) | b;
}

/* Lighten a color by percentage (0-100) */
static inline ui_color_t ui_color_lighten(ui_color_t color, uint8_t pct)
{
    uint32_t r = ((color >> 11) & 0x1F);
    uint32_t g = ((color >> 5) & 0x3F);
    uint32_t b = (color & 0x1F);
    
    r = r + (31 - r) * pct / 100;
    g = g + (63 - g) * pct / 100;
    b = b + (31 - b) * pct / 100;
    
    return (r << 11) | (g << 5) | b;
}

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_COLOR_H */
