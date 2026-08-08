/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_rgb.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : WS2812 (XL-5050RGBC) addressable RGB LED strip driver.
*                      9 LEDs daisy-chained on PD0, GRB order, 800 kHz
*                      bit-banged protocol (interrupt-safe per LED).
*
*                      Typical use:
*                          RGB_Init();
*                          RGB_SetPixel(0, 255, 0, 0);   // LED0 = red
*                          RGB_SetPixel(1, 0, 0, 255);   // LED1 = blue
*                          RGB_Refresh();                // push to strip
********************************************************************************/
#ifndef __BSP_RGB_H
#define __BSP_RGB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Configuration
 *=============================================================================*/

/* Number of WS2812 LEDs on the strip (LED1..LED9 on the schematic) */
#define RGB_LED_COUNT       9

/*=============================================================================
 *  Types
 *=============================================================================*/

/* One pixel, 8 bits per channel (order in memory is R,G,B - the driver
 * converts to the GRB wire order in RGB_Refresh) */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      RGB_Init
 *
 * @brief   Configure PD0 as push-pull output and clear the strip.
 *
 * @return  none
 */
void RGB_Init(void);

/*********************************************************************
 * @fn      RGB_SetPixel
 *
 * @brief   Set one LED's color in the shadow buffer.
 *          Call RGB_Refresh() to make it visible.
 *
 * @param   index - LED index, 0 .. RGB_LED_COUNT-1
 * @param   r,g,b - color channels, 0-255
 *
 * @return  none
 */
void RGB_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/*********************************************************************
 * @fn      RGB_SetPixelColor
 *
 * @brief   Set one LED from a packed 0xRRGGBB value.
 *
 * @param   index - LED index
 * @param   color - packed RGB value
 *
 * @return  none
 */
void RGB_SetPixelColor(uint8_t index, uint32_t color);

/*********************************************************************
 * @fn      RGB_SetAll
 *
 * @brief   Fill all LEDs with the same color (shadow buffer only).
 *
 * @param   r,g,b - color channels, 0-255
 *
 * @return  none
 */
void RGB_SetAll(uint8_t r, uint8_t g, uint8_t b);

/*********************************************************************
 * @fn      RGB_Clear
 *
 * @brief   Turn all LEDs off (shadow buffer only; call RGB_Refresh).
 *
 * @return  none
 */
void RGB_Clear(void);

/*********************************************************************
 * @fn      RGB_SetBrightness
 *
 * @brief   Set the global brightness scaler applied in RGB_Refresh().
 *
 * @param   brightness - 0 (off) .. 255 (full). Default 255.
 *
 * @return  none
 */
void RGB_SetBrightness(uint8_t brightness);

/*********************************************************************
 * @fn      RGB_GetBuffer
 *
 * @brief   Access the shadow buffer directly (RGB_LED_COUNT entries).
 *          Call RGB_Refresh() after modifying it.
 *
 * @return  Pointer to the pixel buffer.
 */
rgb_color_t *RGB_GetBuffer(void);

/*********************************************************************
 * @fn      RGB_Refresh
 *
 * @brief   Shift the whole shadow buffer out to the strip.
 *          Protocol timing blocks ~300 us; global interrupts are masked
 *          per LED (~30 us windows) so the UART ISR keeps running
 *          between LEDs.
 *
 * @return  none
 */
void RGB_Refresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_RGB_H */
