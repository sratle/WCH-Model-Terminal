/**
 * @file    epaper.h
 * @brief   JD79686AB 648x480 B/W e-paper driver - high-level API.
 *
 * V2.0: transition-based waveform (BWR=1, BW mode) with hardware
 * differential partial refresh (DFV_EN=1):
 *   - DTM1/PDTM1 = OLD data, DTM2/PDTM2 = NEW data
 *   - LUTWW/LUTBW/LUTWB/LUTBB selected per (New,Old) pixel pair
 *   - Temperature-compensated frame counts (internal sensor, <10°C: x2)
 *   - POF after every refresh (VCOM discharge, anti-graying, verified)
 *
 * Pixel format (both panel and UI/renderer): 1bpp, MSB = leftmost pixel,
 * bit=1 -> black, bit=0 -> white.  No inversion anywhere in the chain.
 * Each byte represents 8 horizontal pixels (648 / 8 = 81 bytes per row).
 * Partial-refresh X and W must be multiples of 8.
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
 * @brief  Initialise: HW reset + register sequence + LUT load + temp read.
 */
void Epaper_Init(void);

/**
 * @brief  Display image (same data for DTM1 and DTM2). No refresh trigger.
 * @param  image  EPD_FRAME_SIZE bytes. bit=0 white, bit=1 black (UI format).
 */
void Epaper_DisplayImage(const uint8_t *image);

/**
 * @brief  Load an all-white frame into SRAM (no refresh trigger).
 */
void Epaper_ClearWhite(void);

/**
 * @brief  Trigger a full-screen refresh (CLEAR waveform) on the data
 *         already in SRAM: PON -> DRF -> POF.
 */
void Epaper_Update(void);

/**
 * @brief  Full-screen refresh with explicit old/new frames:
 *         writes DTM1=old / DTM2=new, then PON -> DRF -> POF.
 */
void Epaper_UpdateFull(const uint8_t *old_frame, const uint8_t *new_frame);

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
 *         PDTM1=OLD region, PDTM2=NEW region, PDRF with DFV_EN=1
 *         (pixels with New==Old are not re-driven).
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
 * @brief  Load separate old/new full frames into SRAM (no refresh trigger).
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image);

/**
 * @brief  Panel temperature (°C) measured at the last Epaper_Init().
 */
int8_t Epaper_GetTemperatureC(void);

/**
 * @brief  Set VCOM DC level (R82H, 7-bit). Tune to trim gray bias.
 */
void Epaper_SetVdcs(uint8_t vdcs);

/**
 * @brief  Write a TDY register (debug tuning).
 */
void Epaper_WriteTdy(uint8_t addr, uint8_t val);

/* ---- Live waveform tuning (used by the UART2 debug CLI) ---- */
void Epaper_SetFastFrames(uint8_t frames);
void Epaper_SetMaintFrames(uint8_t frames);
void Epaper_SetClearFrames(uint8_t shake, uint8_t erase, uint8_t drive);
void Epaper_SetDfv(uint8_t en);
void Epaper_SetPofAfterRefresh(uint8_t en);
void Epaper_SetFrameScale(uint8_t mode);   /* 0=auto, 1=force x1, 2=force x2 */
int8_t Epaper_RefreshTemperature(void);    /* re-read TSC, returns °C */
void Epaper_DumpTune(void);

#ifdef __cplusplus
}
#endif

#endif /* __EPAPER_H */
