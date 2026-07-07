/**
 * @file    epaper.h
 * @brief   JD79686AB 648x480 B/W e-paper driver - high-level API.
 *
 * Simplified driver based on vendor reference code:
 *   - Init: HW reset + wait BUSY
 *   - Update: PON + DRF + POF (POF discharges VCOM to GND to prevent graying)
 *   - Panel uses power-on default configuration
 *
 * Pixel format (panel): 1bpp, bit=1 -> white, bit=0 -> black.
 * Pixel format (UI/renderer): 1bpp, bit=1 -> black, bit=0 -> white.
 * The driver inverts data automatically when sending to the panel.
 * Each byte represents 8 horizontal pixels (MSB = leftmost).
 * Width must be a multiple of 8 (648 / 8 = 81 bytes per row).
 */
#ifndef __EPAPER_H
#define __EPAPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "epaper_hw.h"
#include <string.h>

/* ---- Display geometry ---- */
#define EPD_WIDTH           648
#define EPD_HEIGHT          480
#define EPD_ROW_BYTES       (EPD_WIDTH / 8)   /* 81 bytes per row */
#define EPD_FRAME_SIZE      (EPD_ROW_BYTES * EPD_HEIGHT) /* 38880 bytes */

/* ---- Pixel colour constants (UI/renderer side) ---- */
#define EPD_WHITE   0x00
#define EPD_BLACK   0xFF

/* ---- API ---- */

/**
 * @brief  Initialise: HW reset + wait for BUSY high.
 */
void Epaper_Init(void);

/**
 * @brief  Display image (same data for DTM1 and DTM2).
 * @param  image  EPD_FRAME_SIZE bytes. bit=0 white, bit=1 black (UI format).
 */
void Epaper_DisplayImage(const uint8_t *image);

/**
 * @brief  Full-screen clear to white.
 */
void Epaper_ClearWhite(void);

/**
 * @brief  Trigger display update (PON -> DRF).
 */
void Epaper_Update(void);

/**
 * @brief  Enter deep sleep (POF + DSLP).
 */
void Epaper_Sleep(void);

/**
 * @brief  Wake from deep sleep (HW reset + re-init).
 */
void Epaper_WakeUp(void);

/**
 * @brief  Partial refresh: update a rectangular region.
 *         Uses PDTM1 + PDTM2 + PDRF commands per datasheet display flow.
 *         Reads region data directly from full frame buffers (no extraction
 *         buffer needed), inverts data to match panel pixel format.
 *         X and W must be multiples of 8.
 * @param  x          X origin (must be multiple of 8)
 * @param  y          Y origin
 * @param  w          Width (must be multiple of 8)
 * @param  h          Height
 * @param  old_frame  Full old frame buffer (EPD_FRAME_SIZE bytes, UI format)
 * @param  new_frame  Full new frame buffer (EPD_FRAME_SIZE bytes, UI format)
 */
void Epaper_PartialRefresh(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *old_frame, const uint8_t *new_frame);

/**
 * @brief  Full refresh with separate old/new images.
 * @param  old_image  OLD data (EPD_FRAME_SIZE bytes), UI format
 * @param  new_image  NEW data (EPD_FRAME_SIZE bytes), UI format
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image);

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_H */
