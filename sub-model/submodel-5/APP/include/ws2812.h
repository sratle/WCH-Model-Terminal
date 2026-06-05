/*********************************************************************
 * File Name          : ws2812.h
 * Description        : WS2812 LED driver using GPIO bit-banging.
 *                      PB14 as data output, NOP-based timing.
 *********************************************************************/

#ifndef __WS2812_H
#define __WS2812_H

#include "CH58x_common.h"
#include "color.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================== */
/*  Configuration                                                       */
/* ==================================================================== */
#define WS2812_LED_COUNT        49      /* 7x7 matrix */
#define WS2812_ROWS             7
#define WS2812_COLS             7

/* ==================================================================== */
/*  API Functions                                                       */
/* ==================================================================== */

/**
 * @brief  Init PB14 as push-pull output, reset WS2812, send all-zero frame.
 */
void WS2812_Init(void);

/**
 * @brief  Set one LED color (RGB888). Converted to GRB on send.
 */
void WS2812_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Set all LEDs from RGB888 array.
 */
void WS2812_SetAllPixels(const rgb888_t *colors);

/**
 * @brief  Set all LEDs to same color.
 */
void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Send LED buffer to WS2812 chain via GPIO bit-bang.
 *         IRQ disabled per-LED (~24µs), re-enabled between LEDs
 *         to allow UART0 ISR to drain FIFO.
 */
void WS2812_Refresh(void);

/**
 * @brief  Clear all LEDs and send immediately.
 */
void WS2812_Clear(void);

/**
 * @brief  Get internal LED buffer (for direct manipulation).
 */
rgb888_t *WS2812_GetBuffer(void);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812_H */
