/**
 * @file    epaper.h
 * @brief   JD79686AB 648×480 B/W e-paper driver – high-level API.
 *
 * Based on manufacturer STM32 reference code (中景园电子).
 * Module has OTP pre-programmed LUT – no register writes needed.
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

/* Partial refresh buffer size */
#define EPD_PARTIAL_BUF_SIZE  4096

/* ---- Pixel colour constants (matching manufacturer: WHITE=0x00, BLACK=0xFF) ---- */
#define EPD_WHITE   0x00
#define EPD_BLACK   0xFF

/* ---- API ---- */

/**
 * @brief  Initialise: HW reset + WaitBusy. Module uses OTP LUT.
 */
void Epaper_Init(void);

/**
 * @brief  Full-screen refresh with 1bpp image data.
 * @param  image  EPD_FRAME_SIZE bytes. bit=0 white, bit=1 black.
 *                NULL to clear white.
 */
void Epaper_DisplayFull(const uint8_t *image);

/**
 * @brief  Full-screen clear to white.
 */
void Epaper_ClearWhite(void);

/**
 * @brief  Trigger display update (PON → DRF) after writing DTM1/DTM2.
 */
void Epaper_Update(void);

/**
 * @brief  Enter deep sleep (POF → WaitBusy → DSLP).
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
