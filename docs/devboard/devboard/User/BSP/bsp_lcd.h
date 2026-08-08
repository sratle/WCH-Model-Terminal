/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_lcd.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : ST7789P3 TFT LCD driver (4-wire SPI, no framebuffer).
*                      Panel : T200H7-C14-05, 2.0" IPS 240x320, RGB565.
*                      Wiring: SCK=PA5  MOSI=PA7  CS=PA4  DC=PA3  RES=PA8
*                      Usage : landscape 320x240 (MADCTL MV|MX).
*
*                      Typical use:
*                          LCD_Init();
*                          LCD_Fill(LCD_BLACK);
*                          LCD_DrawRect(10, 10, 100, 60, LCD_RED);
*                          LCD_DrawText(20, 20, "WCH-DevBoard", LCD_WHITE);
********************************************************************************/
#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "bsp_font.h"
#include "font_montserrat_16.h"

/*=============================================================================
 *  Panel Geometry (compile-time configuration)
 *=============================================================================*/

/* Logical screen size AFTER rotation (landscape).
 * The T200H7-C14-05 module / ST7789P3 controller is natively 240x320,
 * so landscape is 320x240. If a different glass is fitted, only these
 * two defines (and possibly the offsets below) need to change. */
#define LCD_WIDTH           320
#define LCD_HEIGHT          240

/* GRAM address offsets for the visible area (usually 0 for this module) */
#define LCD_XSTART          0
#define LCD_YSTART          0

/* Rotation: 0 = portrait 240x320, 1 = landscape 320x240 (default),
 *           2 = portrait inverted, 3 = landscape inverted.
 * NOTE: for rotations 0/2 swap LCD_WIDTH/LCD_HEIGHT above. */
#define LCD_ROTATION        1

/* Color filter order of the panel: 1 = BGR, 0 = RGB.
 * Verified on the actual board: this module is RGB order (with BGR=1 the
 * red/blue channels were swapped: navy showed as dark red, the red/blue
 * test bars appeared reversed). */
#define LCD_MADCTL_BGR      0

/* SPI1 clock prescaler: 4 -> 36 MHz @144 MHz sysclk (safe bring-up speed,
 * proven margin for the FPC wiring). Try 2 (72 MHz) for faster full-screen
 * updates once the display is confirmed working. */
#define LCD_SPI_PRESCALER   SPI_BaudRatePrescaler_4

/*=============================================================================
 *  RGB565 Color Definitions
 *=============================================================================*/

#define LCD_BLACK           0x0000
#define LCD_WHITE           0xFFFF
#define LCD_RED             0xF800
#define LCD_GREEN           0x07E0
#define LCD_BLUE            0x001F
#define LCD_YELLOW          0xFFE0
#define LCD_CYAN            0x07FF
#define LCD_MAGENTA         0xF81F
#define LCD_ORANGE          0xFD20
#define LCD_GRAY            0x8410
#define LCD_DARKGRAY        0x4208
#define LCD_NAVY            0x000F
#define LCD_DARKGREEN       0x03E0

/* Pack R/G/B (0-255 each) into an RGB565 pixel */
#define LCD_RGB565(r,g,b)   (uint16_t)((((r) & 0xF8) << 8) | \
                                       (((g) & 0xFC) << 3) | ((b) >> 3))

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_Init
 *
 * @brief   Initialize SPI1, control GPIOs and the ST7789P3 controller.
 *          Must be called once before any drawing function.
 *          Requires Delay_Init() (from debug.h) to be called first.
 *
 * @return  none
 */
void LCD_Init(void);

/*********************************************************************
 * @fn      LCD_Fill
 *
 * @brief   Fill the whole screen with a single color.
 *
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_Fill(uint16_t color);

/*********************************************************************
 * @fn      LCD_FillRect
 *
 * @brief   Fill a rectangle (clipped to the screen).
 *
 * @param   x,y   - top-left corner
 * @param   w,h   - size in pixels
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawPixel
 *
 * @brief   Draw a single pixel (bounds-checked).
 *
 * @param   x,y   - pixel coordinate
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawRect
 *
 * @brief   Draw a rectangle outline (1 pixel wide).
 *
 * @param   x,y   - top-left corner
 * @param   w,h   - size in pixels
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawLine
 *
 * @brief   Draw a line between two points (Bresenham).
 *
 * @param   x0,y0 - start point
 * @param   x1,y1 - end point
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawCircle
 *
 * @brief   Draw a circle outline (midpoint algorithm).
 *
 * @param   x0,y0 - center
 * @param   r     - radius in pixels
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

/*********************************************************************
 * @fn      LCD_FillCircle
 *
 * @brief   Draw a filled circle (span based, no floating point).
 *
 * @param   x0,y0 - center
 * @param   r     - radius in pixels
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawRoundRect
 *
 * @brief   Draw a rectangle outline with rounded corners.
 *
 * @param   x,y   - top-left corner
 * @param   w,h   - size in pixels
 * @param   r     - corner radius (0 = plain rectangle)
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t r, uint16_t color);

/*********************************************************************
 * @fn      LCD_FillRoundRect
 *
 * @brief   Draw a filled rectangle with rounded corners.
 *
 * @param   x,y   - top-left corner
 * @param   w,h   - size in pixels
 * @param   r     - corner radius (0 = plain filled rectangle)
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t r, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawTriangle
 *
 * @brief   Draw a triangle outline (three edges).
 *
 * @param   x0,y0 / x1,y1 / x2,y2 - vertices
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);

/*********************************************************************
 * @fn      LCD_FillTriangle
 *
 * @brief   Draw a filled triangle (scanline fill, no floating point).
 *
 * @param   x0,y0 / x1,y1 / x2,y2 - vertices
 * @param   color - RGB565 color
 *
 * @return  none
 */
void LCD_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);

/*=============================================================================
 *  Bitmap / Image API
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_DrawImage
 *
 * @brief   Blit an RGB565 image (2 bytes/pixel, row-major, high byte
 *          first - the same byte order the panel expects).
 *          Clipped to the screen; rows are streamed line by line.
 *
 * @param   x,y  - top-left destination corner
 * @param   w,h  - image size in pixels
 * @param   data - w*h RGB565 pixels
 *
 * @return  none
 */
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   const uint16_t *data);

/*********************************************************************
 * @fn      LCD_DrawMonoBitmap
 *
 * @brief   Draw a 1bpp bitmap (row-major, MSB first - same packing as
 *          the font glyphs). Set bits use fg, clear bits use bg.
 *
 * @param   x,y  - top-left corner
 * @param   w,h  - bitmap size in pixels
 * @param   bmp  - ceil(w*h/8) bytes of pixel data
 * @param   fg   - color for set bits
 * @param   bg   - color for clear bits
 *
 * @return  none
 */
void LCD_DrawMonoBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const uint8_t *bmp, uint16_t fg, uint16_t bg);

/*********************************************************************
 * @fn      LCD_DrawMonoBitmapTR
 *
 * @brief   Draw a 1bpp bitmap with transparent background: only set
 *          bits are painted (with fg). Ideal for icons over content.
 *
 * @param   x,y  - top-left corner
 * @param   w,h  - bitmap size in pixels
 * @param   bmp  - ceil(w*h/8) bytes of pixel data
 * @param   fg   - color for set bits
 *
 * @return  none
 */
void LCD_DrawMonoBitmapTR(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          const uint8_t *bmp, uint16_t fg);

/*=============================================================================
 *  Text API (bitmap fonts, see bsp_font.h)
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_SetFont
 *
 * @brief   Select the font used by LCD_DrawText().
 *          Default after LCD_Init() is font_montserrat_16.
 *
 * @param   font - font descriptor, NULL resets to the default font
 *
 * @return  none
 */
void LCD_SetFont(const lcd_font_t *font);

/*********************************************************************
 * @fn      LCD_GetFont
 *
 * @brief   Query the currently active font.
 *
 * @return  Current font descriptor (never NULL after LCD_Init).
 */
const lcd_font_t *LCD_GetFont(void);

/*********************************************************************
 * @fn      LCD_FontHeight
 *
 * @brief   Line height of the current font in pixels. Useful to stack
 *          text lines: y += LCD_FontHeight().
 *
 * @return  Height in pixels.
 */
uint16_t LCD_FontHeight(void);

/*********************************************************************
 * @fn      LCD_DrawChar
 *
 * @brief   Draw a single character, transparent background.
 *
 * @param   x,y   - top-left corner of the character cell
 * @param   ch    - character to draw
 * @param   color - foreground RGB565 color
 *
 * @return  Character advance width in pixels (0 = unknown char).
 */
uint16_t LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawCharBG
 *
 * @brief   Draw a single character with an opaque background.
 *
 * @param   x,y - top-left corner of the character cell
 * @param   ch  - character to draw
 * @param   fg  - foreground RGB565 color
 * @param   bg  - background RGB565 color
 *
 * @return  Character advance width in pixels (0 = unknown char).
 */
uint16_t LCD_DrawCharBG(uint16_t x, uint16_t y, char ch,
                        uint16_t fg, uint16_t bg);

/*********************************************************************
 * @fn      LCD_DrawText
 *
 * @brief   Draw a NUL-terminated ASCII string with the current font,
 *          transparent background (only foreground pixels are written).
 *
 * @param   x,y    - top-left corner of the text line
 * @param   text   - string to draw
 * @param   color  - foreground RGB565 color
 *
 * @return  none
 */
void LCD_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color);

/*********************************************************************
 * @fn      LCD_DrawTextBG
 *
 * @brief   Draw a string with an opaque background (faster than the
 *          transparent variant: pixels are streamed row by row).
 *
 * @param   x,y    - top-left corner of the text line
 * @param   text   - string to draw
 * @param   fg     - foreground RGB565 color
 * @param   bg     - background RGB565 color
 *
 * @return  none
 */
void LCD_DrawTextBG(uint16_t x, uint16_t y, const char *text,
                    uint16_t fg, uint16_t bg);

/*********************************************************************
 * @fn      LCD_TextWidth
 *
 * @brief   Compute the pixel width of a string with the current font.
 *          Useful for centering: x = (LCD_WIDTH - w) / 2.
 *
 * @param   text - string to measure
 *
 * @return  Width in pixels.
 */
uint16_t LCD_TextWidth(const char *text);

/*********************************************************************
 * @fn      LCD_DrawTextCenter
 *
 * @brief   Draw a string horizontally centered on the screen,
 *          transparent background.
 *
 * @param   y     - top of the text line
 * @param   text  - string to draw
 * @param   color - foreground RGB565 color
 *
 * @return  none
 */
void LCD_DrawTextCenter(uint16_t y, const char *text, uint16_t color);

/*=============================================================================
 *  Display Control API
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_DisplayOn / LCD_DisplayOff
 *
 * @brief   Turn the panel scanning on/off. Contents of GRAM are kept,
 *          so the image reappears after LCD_DisplayOn().
 *          The backlight is hardwired on this board and stays lit.
 *
 * @return  none
 */
void LCD_DisplayOn(void);
void LCD_DisplayOff(void);

/*********************************************************************
 * @fn      LCD_InvertColors
 *
 * @brief   Enable/disable display inversion (INVON / INVOFF).
 *          NOTE: this IPS panel needs inversion ON for correct colors;
 *          use only for visual effects (e.g. alert flashing).
 *
 * @param   enable - 0: normal, 1: inverted
 *
 * @return  none
 */
void LCD_InvertColors(uint8_t enable);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LCD_H */
