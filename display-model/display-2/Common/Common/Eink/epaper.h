/**
 * @file    epaper.h
 * @brief   JD79686AB 648×480 B/W e-paper driver – high-level API.
 *
 * Based on Arduino OKRA0583BNF686F0 sample code (After OTP Model).
 * Uses OTP LUT, no register LUT tables needed.
 *
 * Pixel format: 1bpp, bit=0 → white, bit=1 → black.
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

/* ---- Pixel colour constants ---- */
#define EPD_WHITE   0x00
#define EPD_BLACK   0xFF

/* ---- API ---- */

/**
 * @brief  Initialise: HW reset + OTP LUT mode init.
 */
void Epaper_Init(void);

/**
 * @brief  Display image (same data for DTM1 and DTM2, Arduino-style).
 * @param  image  EPD_FRAME_SIZE bytes. bit=0 white, bit=1 black.
 */
void Epaper_DisplayImage(const uint8_t *image);

/**
 * @brief  Full-screen clear to white (DTM1=0xFF, DTM2=0x00).
 */
void Epaper_ClearWhite(void);

/**
 * @brief  Trigger display update (PON → DRF → POF).
 */
void Epaper_Update(void);

/**
 * @brief  Enter deep sleep (PON + DSLP).
 */
void Epaper_Sleep(void);

/**
 * @brief  Wake from deep sleep (HW reset + re-init).
 */
void Epaper_WakeUp(void);

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_H */
