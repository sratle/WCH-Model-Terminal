/*********************************************************************
 * File Name          : ws2812.h
 * Description        : WS2812 LED driver using SPI0 Master MOSI.
 *                      Encodes 24-bit GRB data into SPI bit patterns.
 *                      3 SPI bits = 1 WS2812 bit:
 *                        '1' -> 110  '0' -> 100
 *                      SPI clock = 2.5 MHz -> 1 WS2812 bit = 1.2 us
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

/* SPI encoding: 10 SPI bits per WS2812 bit, 24 bits per LED */
#define WS2812_SPI_BYTES_PER_LED    30      /* 24 * 10 / 8 */
#define WS2812_RESET_BYTES          60      /* ~64us at 7.5MHz, > 50us reset */

/* Total SPI buffer size for one frame */
#define WS2812_SPI_BUF_SIZE         (WS2812_LED_COUNT * WS2812_SPI_BYTES_PER_LED + WS2812_RESET_BYTES)

/* ==================================================================== */
/*  API Functions                                                       */
/* ==================================================================== */

/**
 * @brief  Initialize SPI0 Master for WS2812 output.
 *         Uses SPI0 remapped pins: PB12(SCK), PB14(MOSI).
 *         Configures 2.5 MHz clock, Mode 0, MSB first.
 */
void WS2812_Init(void);

/**
 * @brief  Set the color of a single LED by index (0-48).
 *         Color is in RGB888 format; driver converts to GRB for WS2812.
 * @param  index - LED index (0 to WS2812_LED_COUNT-1)
 * @param  r, g, b - RGB888 color values
 */
void WS2812_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Set all LEDs from an array of RGB888 colors.
 * @param  colors - Array of rgb888_t, length = WS2812_LED_COUNT
 */
void WS2812_SetAllPixels(const rgb888_t *colors);

/**
 * @brief  Set all LEDs to the same color.
 */
void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  Transmit the current LED buffer to WS2812 chain via SPI.
 *         This sends WS2812_SPI_BUF_SIZE bytes through SPI0 MOSI.
 *         Blocking call; takes ~1.5ms for 49 LEDs.
 */
void WS2812_Refresh(void);

/**
 * @brief  Turn off all LEDs and refresh immediately.
 */
void WS2812_Clear(void);

/**
 * @brief  Get the internal LED buffer (for direct manipulation).
 * @return Pointer to rgb888_t array of WS2812_LED_COUNT elements.
 */
rgb888_t *WS2812_GetBuffer(void);

#ifdef __cplusplus
}
#endif

#endif /* __WS2812_H */
