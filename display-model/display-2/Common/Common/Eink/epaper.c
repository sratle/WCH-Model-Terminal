/********************************** (C) COPYRIGHT *******************************
* File Name          : epaper.c
* Author             : E-ink Model Team
* Description        : JD79686AB 648x480 B/W e-paper driver.
*
* Init sequence writes full register set + custom fast LUT:
*   - PSR:  BW mode (BWR=1), REG_EN=1 (use register LUT)
*   - PWR:  internal DC/DC, VGH=20V, VGL=-20V, VSH=15V, VSL=-15V
*   - LUT:  10-frame fast waveform (~200ms @ 50Hz = 5FPS)
*
* The default OTP LUT has ~188 frames (~3.76s @ 50Hz) which is far too
* slow for interactive UI.  The custom register LUT trades contrast for
* speed: 10 frames is enough for a legible B/W transition while keeping
* refresh time under 500ms (>= 2 FPS).
*
* Pixel format (panel side): 1bpp, MSB = leftmost pixel, bit=1 = white,
* bit=0 = black. The MiniUI renderer produces bit=1 = black, so we invert
* data before sending it to the panel.
********************************************************************************/
#include "epaper.h"
#include <stdbool.h>
#include <stdint.h>

/* Extern from MiniUI for timing measurement (avoids header dependency) */
extern uint32_t ui_get_real_ms(void);

/*============================================================================
 *  Fast LUT for partial / full refresh
 *
 *  LUT format: 7 groups x 6 bytes = 42 bytes per LUT register.
 *  Each group:  [levels(1)] [frames_L1(1)] [frames_L2(1)] [frames_L3(1)]
 *               [frames_L4(1)] [repeat(1)]
 *
 *  Level encoding (datasheet section 3.1.14 - 3.1.18):
 *    LUTC  (R20H): 00=-VCM_DC  01=VSH+VCM_DC  10=VSL+VCM_DC  11=Floating
 *    LUTW* (R21-24): 00=GND    01=VSH         10=VSL         11=VSHR
 *
 *  Waveform design (10 frames @ 50Hz = 200ms):
 *    LUTC  : VCOM = -VCM_DC (hold reference for all frames)
 *    LUTWW : Source = GND  (unchanged white pixels: no drive, held by VCOM)
 *    LUTBB : Source = GND  (unchanged black pixels: no drive, held by VCOM)
 *    LUTBW : Source = VSH  (black->white transition: drive positive)
 *    LUTWB : Source = VSL  (white->black transition: drive negative)
 *
 *  With DFV_EN=1 in PDRF, unchanged pixels (old==new) follow the VCOM LUT
 *  (hold), and only changed pixels are actively driven.  This eliminates
 *  ghosting/flicker on static areas.
 *==========================================================================*/

#define FAST_LUT_FRAMES  10

/* LUTC (R20H) - VCOM LUT: -VCM_DC for 10 frames (hold) */
static const uint8_t LUT_FAST_C[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTWW (R21H) - White->White: GND (hold) */
static const uint8_t LUT_FAST_WW[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTBW (R22H) - Black->White: VSH (drive to white) */
static const uint8_t LUT_FAST_BW[42] = {
    0x40, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,  /* 0x40 = L1:VSH */
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTWB (R23H) - White->Black: VSL (drive to black) */
static const uint8_t LUT_FAST_WB[42] = {
    0x80, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,  /* 0x80 = L1:VSL */
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTBB (R24H) - Black->Black: GND (hold) */
static const uint8_t LUT_FAST_BB[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};

/*============================================================================
 *  Power state tracking — avoids redundant PON commands between
 *  consecutive partial refreshes.  The panel stays powered on; only
 *  the first refresh after init/sleep sends PON.
 *==========================================================================*/

static bool s_epd_powered_on = false;

/*============================================================================
 *  Internal helpers
 *==========================================================================*/

/* Send the 8-byte region header (x, y, w, h) for PDTM1/PDTM2/PDRF commands.
 * X and W must be multiples of 8 (enforced by mask 0xF8). */
static void epaper_send_region_header(int16_t x, int16_t y, int16_t w, int16_t h)
{
    Epaper_Hw_WriteData((x >> 8) & 0x03);
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);
}

/* Write a TDY register (command 0xF8 + addr + value).
 * TDY registers are test-mode registers that must be set even on
 * OTP-programmed modules (they are NOT stored in OTP). */
static void epaper_write_tdy(uint8_t addr, uint8_t val)
{
    Epaper_Hw_WriteCmd(0xF8);
    Epaper_Hw_WriteData(addr);
    Epaper_Hw_WriteData(val);
}

/* Ensure the panel is powered on.  Skips PON if already powered. */
static void epaper_ensure_power_on(void)
{
    if (s_epd_powered_on) return;
    Epaper_Hw_WriteCmd(0x04);               /* PON */
    Epaper_Hw_WaitBusy(5000);
    s_epd_powered_on = true;
}

/*============================================================================
 *  Public API
 *==========================================================================*/

/*********************************************************************
 * @fn      Epaper_Init
 *
 * @brief   Full initialization: HW reset + TDY registers + panel
 *          registers (PSR/PWR/BTST/OSC/TRES/VDCS/PWS/TCON) + fast LUT.
 *          Uses register LUT (REG_EN=1) with a 10-frame waveform for
 *          fast refresh (~200ms per update).
 */
void Epaper_Init(void)
{
    Epaper_Hw_Init();
    printf("[epd_hw] gpio init ok, BUSY=0x%08lX\r\n", (uint32_t)EPD_BUSY_READ());
    Epaper_Hw_Reset();
    printf("[epd_hw] reset done, BUSY=0x%08lX\r\n", (uint32_t)EPD_BUSY_READ());
    int ret = Epaper_Hw_WaitBusy(2000);
    printf("[epd_hw] wait busy after reset: %d, BUSY=0x%08lX\r\n",
           ret, (uint32_t)EPD_BUSY_READ());

    /* ---- TDY commands (not stored in OTP, must write every init) ---- */
    Epaper_Hw_WriteCmd(0x08); Epaper_Hw_WriteData(0x00);  /* TDY dummy */
    epaper_write_tdy(0x60, 0xA5);  /* TDY key (unlock) */
    epaper_write_tdy(0x93, 0x18);  /* Stop-pulse mechanism off */
    epaper_write_tdy(0x73, 0x05);  /* Vcom driving adjust */
    epaper_write_tdy(0x92, 0x08);  /* VGL pull GND */
    epaper_write_tdy(0xA8, 0x3A);  /* r_partial_all_gate_en */
    epaper_write_tdy(0x88, 0x06);  /* Power-off Vcom discharge to GND */

    /* ---- Panel registers ---- */

    /* PSR (R00H): BW mode + register LUT
     *   Bit 7-6 RES[1:0]  = 10
     *   Bit 5   REG_EN    = 1  (use register LUT, not OTP)
     *   Bit 4   BWR       = 1  (B/W only mode — required for DFV_EN)
     *   Bit 3   UD        = 1  (scan up)
     *   Bit 2   SHL       = 1  (shift right)
     *   Bit 1   SHD_N     = 1  (booster on)
     *   Bit 0   RST_N     = 1  (no effect)
     */
    Epaper_Hw_WriteCmd(0x00);
    Epaper_Hw_WriteData(0xBF);

    /* PWR (R01H): power setting
     *   VDS_EN=1, VDG_EN=1 (internal DC/DC for source and gate)
     *   VGHL_LV=000 (VGH=+20V, VGL=-20V)
     *   VSH=0x3F (+15V), VSL=0x3F (-15V), VSHR=0x28 (5V)
     */
    Epaper_Hw_WriteCmd(0x01);
    Epaper_Hw_WriteData(0x03);
    Epaper_Hw_WriteData(0x00);
    Epaper_Hw_WriteData(0x3F);
    Epaper_Hw_WriteData(0x3F);
    Epaper_Hw_WriteData(0x28);

    /* BTST (R06H): booster soft start */
    Epaper_Hw_WriteCmd(0x06);
    Epaper_Hw_WriteData(0x57);
    Epaper_Hw_WriteData(0x63);
    Epaper_Hw_WriteData(0x31);

    /* OSC (R30H): frame rate = 50Hz (M=7, N=4) */
    Epaper_Hw_WriteCmd(0x30);
    Epaper_Hw_WriteData(0x3C);

    /* CDI (R50H) */
    Epaper_Hw_WriteCmd(0x50);
    Epaper_Hw_WriteData(0x57);

    /* TRES (R61H): 648 x 480 resolution */
    Epaper_Hw_WriteCmd(0x61);
    Epaper_Hw_WriteData(0x02);
    Epaper_Hw_WriteData(0x88);
    Epaper_Hw_WriteData(0x01);
    Epaper_Hw_WriteData(0xE0);

    /* VDCS (R82H): VCOM DC = -2.3V */
    Epaper_Hw_WriteCmd(0x82);
    Epaper_Hw_WriteData(0x2C);

    /* PWS (RE8H): power saving */
    Epaper_Hw_WriteCmd(0xE8);
    Epaper_Hw_WriteData(0x40);

    /* TCON (R60H): TCON setting */
    Epaper_Hw_WriteCmd(0x60);
    Epaper_Hw_WriteData(0x04);

    /* ---- Fast LUT registers (R20H - R24H) ---- */
    Epaper_Hw_WriteCmd(0x20);
    Epaper_Hw_WriteDataBuf(LUT_FAST_C, 42);
    Epaper_Hw_WriteCmd(0x21);
    Epaper_Hw_WriteDataBuf(LUT_FAST_WW, 42);
    Epaper_Hw_WriteCmd(0x22);
    Epaper_Hw_WriteDataBuf(LUT_FAST_BW, 42);
    Epaper_Hw_WriteCmd(0x23);
    Epaper_Hw_WriteDataBuf(LUT_FAST_WB, 42);
    Epaper_Hw_WriteCmd(0x24);
    Epaper_Hw_WriteDataBuf(LUT_FAST_BB, 42);

    s_epd_powered_on = false;  /* Panel not powered yet until first PON */

    printf("[epd] init done: BW mode, REG_EN=1, fast LUT (%d frames @ 50Hz = %dms)\r\n",
           FAST_LUT_FRAMES, FAST_LUT_FRAMES * 20);
}

/*********************************************************************
 * @fn      Epaper_ClearWhite
 *
 * @brief   Clear to white.
 *          Panel data: DTM1 = black (0x00), DTM2 = white (0xFF).
 */
void Epaper_ClearWhite(void)
{
    uint16_t i, j;

    Epaper_Hw_WriteCmd(0x10);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0x00);    /* old = black (panel: 0x00) */
    Epaper_Hw_WriteCmd(0x11);  /* DSP: data stop */

    Epaper_Hw_WriteCmd(0x13);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0xFF);    /* new = white (panel: 0xFF) */
    Epaper_Hw_WriteCmd(0x11);  /* DSP: data stop */
}

/*********************************************************************
 * @fn      Epaper_DisplayImage
 *
 * @brief   Display image data (same data for both DTM1 and DTM2).
 *          Invert frame data to match panel mapping.
 */
void Epaper_DisplayImage(const uint8_t *image)
{
    uint32_t i;

    Epaper_Hw_WriteCmd(0x10);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~image[i]);
    Epaper_Hw_WriteCmd(0x11);  /* DSP: data stop */

    Epaper_Hw_WriteCmd(0x13);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~image[i]);
    Epaper_Hw_WriteCmd(0x11);  /* DSP: data stop */
}

/*********************************************************************
 * @fn      Epaper_Update
 *
 * @brief   PON -> WaitBusy -> DRF -> WaitBusy.
 *          Uses the fast register LUT (10 frames ~ 200ms).
 *          Does NOT send POF — panel stays powered for fast consecutive
 *          updates (matches vendor reference code).
 */
void Epaper_Update(void)
{
    int ret;
    uint32_t t0 = ui_get_real_ms();
    epaper_ensure_power_on();
    uint32_t t_pon = ui_get_real_ms() - t0;
    Epaper_Hw_WriteCmd(0x12);
    ret = Epaper_Hw_WaitBusy(10000);
    uint32_t t_total = ui_get_real_ms() - t0;
    printf("[epd] update(full): pon=%lums drf_wait=%d total=%lums\r\n",
           t_pon, ret, t_total);
}

/*********************************************************************
 * @fn      Epaper_PartialRefresh
 *
 * @brief   Partial refresh using PDTM1 + PDTM2 + PDRF.
 *          Reads rectangular region data directly from full frame buffers.
 *
 *          Datasheet display flow (section 2.1):
 *            PDTM1(R14h) + PDTM2(R15h)
 *            -> PON(R04h) [wait BUSY]   (skipped if already powered)
 *            -> PDRF(R16h) [wait BUSY]
 *
 *          POF is NOT sent after each partial refresh — the panel stays
 *          powered between refreshes for fast consecutive updates.  POF is
 *          only sent in Epaper_Sleep(). This matches the vendor reference
 *          code (EPD_Update does PON + DRF only, no POF).
 *
 *          Data is inverted (~) before sending because:
 *            - Frame buffer: bit=1=black, bit=0=white (UI convention)
 *            - Panel:        bit=1=white, bit=0=black (panel convention)
 *
 *          DFV_EN=1 in PDRF: unchanged pixels (old==new) follow VCOM LUT
 *          instead of data LUT, preventing ghosting on static areas.
 *
 *          With the 10-frame fast LUT, each partial refresh takes ~200ms
 *          (5 FPS) regardless of region size.
 *
 * @param  x          X origin (must be multiple of 8)
 * @param  y          Y origin
 * @param  w          Width (must be multiple of 8)
 * @param  h          Height
 * @param  old_frame  Full old frame buffer (EPD_FRAME_SIZE bytes, UI format)
 * @param  new_frame  Full new frame buffer (EPD_FRAME_SIZE bytes, UI format)
 */
void Epaper_PartialRefresh(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *old_frame, const uint8_t *new_frame)
{
    uint16_t row_bytes = w / 8;
    int16_t  start_byte = x / 8;
    int16_t  row, col;
    uint32_t t0, t_data, t_pon, t_refresh;

    t0 = ui_get_real_ms();

    /* PDTM1: OLD data (inverted to match panel format) */
    Epaper_Hw_WriteCmd(0x14);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &old_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        for (col = 0; col < row_bytes; col++) {
            Epaper_Hw_WriteData(~src[col]);
        }
    }
    /* DSP: signal data transmission complete (datasheet: "must send 11H") */
    Epaper_Hw_WriteCmd(0x11);

    /* PDTM2: NEW data (inverted to match panel format) */
    Epaper_Hw_WriteCmd(0x15);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &new_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        for (col = 0; col < row_bytes; col++) {
            Epaper_Hw_WriteData(~src[col]);
        }
    }
    /* DSP: signal data transmission complete */
    Epaper_Hw_WriteCmd(0x11);

    t_data = ui_get_real_ms() - t0;

    /* PON: Power ON (only if not already powered) */
    epaper_ensure_power_on();
    t_pon = ui_get_real_ms() - t0;

    /* PDRF: trigger partial refresh with DFV_EN=1 */
    Epaper_Hw_WriteCmd(0x16);
    Epaper_Hw_WriteData(((1 << 7) | ((x >> 8) & 0x03)));  /* DFV_EN=1 + x[9:8] */
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);
    Epaper_Hw_WaitBusy(3000);

    t_refresh = ui_get_real_ms() - t0;
    printf("[epd] partial(%d,%d,%d,%d) data=%lums pon=%lums total=%lums\r\n",
           x, y, w, h, t_data, t_pon, t_refresh);
}

/*********************************************************************
 * @fn      Epaper_DisplayImageDiff
 *
 * @brief   Full refresh with separate old/new images.
 *          Invert both old and new data to match panel mapping.
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image)
{
    uint32_t i;

    printf("[epd] diff: dtm1 start\r\n");
    Epaper_Hw_WriteCmd(0x10);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~old_image[i]);
    Epaper_Hw_WriteCmd(0x11);  /* DSP: data stop */
    printf("[epd] diff: dtm2 start\r\n");
    Epaper_Hw_WriteCmd(0x13);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~new_image[i]);
    Epaper_Hw_WriteCmd(0x11);  /* DSP: data stop */
    printf("[epd] diff: done\r\n");
}

/*********************************************************************
 * @fn      Epaper_Sleep
 *
 * @brief   POF then DSLP (deep sleep).
 *          POF powers off the panel; s_epd_powered_on is cleared so the
 *          next refresh will re-send PON.
 */
void Epaper_Sleep(void)
{
    Epaper_Hw_WriteCmd(0x02);
    Epaper_Hw_WaitBusy(3000);
    s_epd_powered_on = false;

    Epaper_Hw_WriteCmd(0x07);
    Epaper_Hw_WriteData(0xA5);
    Delay_Ms(100);
}

/*********************************************************************
 * @fn      Epaper_WakeUp
 *
 * @brief   Wake from deep sleep: HW reset + re-init.
 */
void Epaper_WakeUp(void)
{
    Epaper_Init();
}
