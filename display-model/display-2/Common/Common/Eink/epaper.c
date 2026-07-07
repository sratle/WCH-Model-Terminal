/********************************** (C) COPYRIGHT *******************************
* File Name          : epaper.c
* Author             : E-ink Model Team
* Description        : JD79686AB 648x480 B/W e-paper driver.
*
* Init sequence writes full register set + custom fast LUT:
*   - PSR:  BWR=0 mode (B/W + Red register), REG_EN=1 (use register LUT)
*   - PWR:  internal DC/DC, VGH=20V, VGL=-20V, VSH=15V, VSL=-15V
*   - LUT:  20-frame drive + 5-frame data-release (~500ms @ 50Hz = 2FPS)
*
* The default OTP LUT has ~188 frames (~3.76s @ 50Hz) which is far too
* slow for interactive UI.  The custom register LUT trades contrast for
* speed: 20 frames gives fuller black drive (avoiding faint blacks).
* A 5-frame data-release phase (VCOM stays at -VCM_DC, data lines to GND)
* reduces post-refresh pixel bias from ±15V to +VCM_DC, slowing the gradual
* graying caused by DC charge accumulation (since POF is not sent, the
* panel stays powered on between refreshes).
*
* Pixel format (panel side): 1bpp, MSB = leftmost pixel, bit=1 = BLACK,
* bit=0 = WHITE (empirically determined, opposite of datasheet claim).
* The MiniUI renderer produces bit=1 = black, which matches the panel
* directly — NO data inversion is needed.
********************************************************************************/
#include "epaper.h"
#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 *  Timing measurement — Delay_Ms based (10ms granularity)
 *
 *  NOTE: ui_get_real_ms() is broken (SysTick->CNT / (SystemCoreClock/1000)
 *  always returns 0 because CNT < SystemCoreClock/1000).  We use a
 *  local busy-wait counter instead.
 *==========================================================================*/
static uint32_t s_epd_time_ms = 0;

static void epd_time_reset(void) { s_epd_time_ms = 0; }

static uint32_t epd_time_mark(void)
{
    /* Sample time by checking SysTick->CNT delta — but since that's
     * unreliable, we just return the accumulated counter.  The actual
     * timing is done in epd_wait_busy_counted(). */
    return s_epd_time_ms;
}

/* Wait for BUSY=idle, counting elapsed time in 10ms increments. */
static int epd_wait_busy_counted(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (Epd_Is_Busy())
    {
        if (elapsed >= timeout_ms)
        {
            printf("[epd_hw] BUSY timeout (%lu ms)\r\n", timeout_ms);
            return -1;
        }
        Delay_Ms(10);
        elapsed += 10;
    }
    s_epd_time_ms += elapsed;
    return 0;
}

/*============================================================================
 *  Fast LUT for BWR=0 mode — ABSOLUTE value LUT (not transition-based)
 *
 *  In BWR=0 mode with REG_EN=1, LUT registers are ABSOLUTE (not transition):
 *    R20H = LUTC  (VCOM LUT)
 *    R21H = LUTWW (unused in BWR mode)
 *    R22H = LUTR  (Red pixel LUT — absolute)
 *    R23H = LUTW  (White LUT — applies to data 00)
 *    R24H = LUTB  (Black LUT — applies to data 01)
 *
 *  CDI register (R50H=0x47) has DDX=00 (no data polarity inversion):
 *    bit=0 → data 00 → LUTW (White LUT)
 *    bit=1 → data 01 → LUTB (Black LUT)
 *
 *  MiniUI format: bit=1=black, bit=0=white (matches DDX=00 mapping directly)
 *
 *  Level encoding (from datasheet R23H/R24H description):
 *    00=GND  01=VSH  10=VSL  11=VSHR
 *
 *  IMPORTANT: For this panel, VSL drives WHITE and VSH drives BLACK.
 *  This is confirmed by the datasheet's built-in LUT (Group 4, P19):
 *    LUTW = 0x80 → VSL (White LUT uses VSL to drive white)
 *    LUTB = 0x10 → VSH (Black LUT uses VSH to drive black)
 *  This is the OPPOSITE of standard e-paper convention.
 *
 *  Waveform (20 drive + 5 discharge = 25 frames @ 50Hz = 500ms = 2FPS):
 *    Group 1 (drive, 20 frames):
 *      LUTC:  VCOM = -VCM_DC (hold reference)
 *      LUTR:  GND (no red pixels)
 *      LUTW:  VSL (0xAA) → drives bit=0 (white) pixels to VSL → white
 *      LUTB:  VSH (0x55) → drives bit=1 (black) pixels to VSH → black
 *    Group 2 (data release, 5 frames):
 *      LUTC:  -VCM_DC (0x00) → VCOM stays controlled (NOT Floating)
 *      LUTR/W/B: GND (0x00) → pixel electrodes to 0V
 *      (Releases data-line drive; VCOM must NOT float or its voltage
 *       drifts uncontrolled and drives all pixels to one state)
 *==========================================================================*/

#define FAST_LUT_FRAMES          20
#define FAST_LUT_DISCHARGE_FRAMES 5

/* LUTC (R20H) - VCOM LUT
 *   LUTC level encoding (datasheet R20H):
 *     00 = -VCM_DC, 01 = VSH+VCM_DC, 10 = VSL+VCM_DC, 11 = Floating (Hi-Z)
 *   Group 1: -VCM_DC for FAST_LUT_FRAMES (drive phase)
 *   Group 2: -VCM_DC (0x00) for FAST_LUT_DISCHARGE_FRAMES (data release)
 *            VCOM stays controlled at -VCM_DC.  DO NOT use Floating (0xFF):
 *            Hi-Z VCOM drifts uncontrolled and drives all pixels to one
 *            state (instant all-black/all-white).  The data lines going to
 *            GND in group 2 of LUTR/W/B reduces the post-refresh pixel bias
 *            from ±15V (VSL/VSH) down to +VCM_DC (~1.5V), slowing the
 *            gradual graying that would otherwise accumulate. */
static const uint8_t LUT_FAST_C[42] = {
    0x00, FAST_LUT_FRAMES,          0x00, 0x00, 0x00, 0x01,
    0x00, FAST_LUT_DISCHARGE_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTWW (R21H) - unused in BWR mode, set to GND + GND discharge */
static const uint8_t LUT_FAST_WW[42] = {
    0x00, FAST_LUT_FRAMES,          0x00, 0x00, 0x00, 0x01,
    0x00, FAST_LUT_DISCHARGE_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTR (R22H) - Red: GND (no red drive) + GND discharge */
static const uint8_t LUT_FAST_R[42] = {
    0x00, FAST_LUT_FRAMES,          0x00, 0x00, 0x00, 0x01,
    0x00, FAST_LUT_DISCHARGE_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTW (R23H) - White LUT: VSL (0xAA = 10 10 10 10 = all VSL)
 *   Group 1: VSL drive for FAST_LUT_FRAMES
 *   Group 2: GND (0x00) discharge for FAST_LUT_DISCHARGE_FRAMES
 *   DDX=00: data 00 → LUTW → applies to bit=0 (white) pixels → VSL → white
 *   (VSL drives white on this panel, confirmed by datasheet built-in LUT) */
static const uint8_t LUT_FAST_W[42] = {
    0xAA, FAST_LUT_FRAMES,          0x00, 0x00, 0x00, 0x01,
    0x00, FAST_LUT_DISCHARGE_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* LUTB (R24H) - Black LUT: VSH (0x55 = 01 01 01 01 = all VSH)
 *   Group 1: VSH drive for FAST_LUT_FRAMES
 *   Group 2: GND (0x00) discharge for FAST_LUT_DISCHARGE_FRAMES
 *   DDX=00: data 01 → LUTB → applies to bit=1 (black) pixels → VSH → black
 *   (VSH drives black on this panel, confirmed by datasheet built-in LUT) */
static const uint8_t LUT_FAST_B[42] = {
    0x55, FAST_LUT_FRAMES,          0x00, 0x00, 0x00, 0x01,
    0x00, FAST_LUT_DISCHARGE_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
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
    epd_wait_busy_counted(5000);
    s_epd_powered_on = true;
}

/*============================================================================
 *  Public API
 *==========================================================================*/

/*********************************************************************
 * @fn      Epaper_Init
 *
 * @brief   Initialization with BWR=0 + register LUT for fast refresh.
 *
 *          BWR=0 (B/W/Red mode) + REG_EN=1 (register LUT).
 *          Custom 10-frame LUT gives ~200ms per refresh (5FPS).
 *
 *          In BWR=0 mode:
 *          - DTM1/PDTM1 = B/W channel (bit=1=white)
 *          - DTM2/PDTM2 = Red channel (0x00 = no red)
 *          - LUTR/LUTW/LUTB drive pixels based on current value
 *          - DFV_EN in PDRF has no effect (BWR=0)
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

    /* ---- PSR (R00H): BWR=0 + REG_EN=1 ----
     *   0xAF = RES:10, REG_EN:1, BWR:0, UD:1, SHL:1, SHD_N:1, RST_N:1
     *   BWR=0    → B/W/Red mode (panel default, works with OTP)
     *   REG_EN=1 → use register LUT (not OTP)
     */
    Epaper_Hw_WriteCmd(0x00);
    Epaper_Hw_WriteData(0xAF);

    /* ---- PWR (R01H) ---- */
    Epaper_Hw_WriteCmd(0x01);
    Epaper_Hw_WriteData(0x03); Epaper_Hw_WriteData(0x00);
    Epaper_Hw_WriteData(0x3F); Epaper_Hw_WriteData(0x3F);
    Epaper_Hw_WriteData(0x28);

    /* ---- BTST (R06H) ---- */
    Epaper_Hw_WriteCmd(0x06);
    Epaper_Hw_WriteData(0x57); Epaper_Hw_WriteData(0x63); Epaper_Hw_WriteData(0x31);

    /* ---- OSC (R30H): 50Hz ---- */
    Epaper_Hw_WriteCmd(0x30); Epaper_Hw_WriteData(0x3C);

    /* ---- CDI (R50H): VBD=01, DDX=00, CDI=0111 ----
     *   DDX=00: no data polarity inversion
     *   bit=0 → data 00 → LUTW (White LUT → VSL → white)
     *   bit=1 → data 01 → LUTB (Black LUT → VSH → black)
     *   This matches MiniUI convention (bit=0=white, bit=1=black) directly. */
    Epaper_Hw_WriteCmd(0x50); Epaper_Hw_WriteData(0x47);

    /* ---- TRES (R61H): 648x480 ---- */
    Epaper_Hw_WriteCmd(0x61);
    Epaper_Hw_WriteData(0x02); Epaper_Hw_WriteData(0x88);
    Epaper_Hw_WriteData(0x01); Epaper_Hw_WriteData(0xE0);

    /* ---- VDCS (R82H) ---- */
    Epaper_Hw_WriteCmd(0x82); Epaper_Hw_WriteData(0x2C);

    /* ---- PWS (RE8H) ---- */
    Epaper_Hw_WriteCmd(0xE8); Epaper_Hw_WriteData(0x40);

    /* ---- TCON (R60H) ---- */
    Epaper_Hw_WriteCmd(0x60); Epaper_Hw_WriteData(0x04);

    /* ---- Fast LUT registers (R20H-R24H) ----
     *   Absolute LUT: IC drives pixels based on their value in PDTM1
     *   Group 1 drives (20 frames) + Group 2 data-release (5 frames)
     *   = 25 frames @ 50Hz = 500ms per refresh (2 FPS) */
    Epaper_Hw_WriteCmd(0x20); Epaper_Hw_WriteDataBuf(LUT_FAST_C, 42);
    Epaper_Hw_WriteCmd(0x21); Epaper_Hw_WriteDataBuf(LUT_FAST_WW, 42);
    Epaper_Hw_WriteCmd(0x22); Epaper_Hw_WriteDataBuf(LUT_FAST_R, 42);
    Epaper_Hw_WriteCmd(0x23); Epaper_Hw_WriteDataBuf(LUT_FAST_W, 42);
    Epaper_Hw_WriteCmd(0x24); Epaper_Hw_WriteDataBuf(LUT_FAST_B, 42);

    s_epd_powered_on = false;

    printf("[epd] init done: BWR=0, REG_EN=1, fast LUT (%d+%d frames @ 50Hz = %dms)\r\n",
           FAST_LUT_FRAMES, FAST_LUT_DISCHARGE_FRAMES,
           (FAST_LUT_FRAMES + FAST_LUT_DISCHARGE_FRAMES) * 20);
}

/*********************************************************************
 * @fn      Epaper_ClearWhite
 *
 * @brief   Clear to white.
 *          Panel format: bit=1=BLACK, bit=0=WHITE.
 *          DTM1=0x00 (all bit=0 = all white), Red=0x00 (no red) → white.
 */
void Epaper_ClearWhite(void)
{
    uint16_t i, j;

    Epaper_Hw_WriteCmd(0x10);  /* DTM1 = BW channel */
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0x00);    /* BW = 0x00 = all white (bit=0=white) */

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 = Red channel */
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0x00);    /* Red = none */
}

/*********************************************************************
 * @fn      Epaper_DisplayImage
 *
 * @brief   Display image data.
 *          Panel format: bit=1=BLACK, bit=0=WHITE (same as MiniUI).
 *          No inversion needed — send data directly.
 */
void Epaper_DisplayImage(const uint8_t *image)
{
    uint32_t i;

    Epaper_Hw_WriteCmd(0x10);  /* DTM1 = BW channel */
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(image[i]);

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 = Red channel */
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(0x00);  /* no red */
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
    epd_time_reset();
    uint32_t t_before_pon = epd_time_mark();
    epaper_ensure_power_on();
    uint32_t t_after_pon = epd_time_mark();

    Epaper_Hw_WriteCmd(0x12);  /* DRF */
    int ret = epd_wait_busy_counted(10000);
    uint32_t t_total = epd_time_mark();

    printf("[epd] update(full): pon=%lums drf=%lums(total) ret=%d\r\n",
           t_after_pon - t_before_pon, t_total - t_after_pon, ret);
}

/*********************************************************************
 * @fn      Epaper_PartialRefresh
 *
 * @brief   Partial refresh using PDTM1 + PDTM2 + PDRF.
 *          Reads rectangular region data directly from full frame buffers.
 *
 *          Display flow (verified working — NO DSP/0x11 command):
 *            PDTM1(R14h) + PDTM2(R15h)
 *            -> PON(R04h) [wait BUSY]   (skipped if already powered)
 *            -> PDRF(R16h) [wait BUSY]
 *
 *          NOTE: Datasheet mentions "must send command 11H" after data
 *          transmission, but sending 0x11 causes PDRF to NOT trigger
 *          (BUSY never goes LOW, refresh completes in 0ms with no actual
 *          screen update).  The vendor reference code (EPD.c) also does
 *          NOT send 0x11.  CS toggling per byte is sufficient to signal
 *          end of data transmission.
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
    (void)old_frame;  /* Transition LUT: IC uses internal SRAM as old reference */

    uint16_t row_bytes = w / 8;
    int16_t  start_byte = x / 8;
    int16_t  row, col;

    /* Wait for IC to be truly idle before sending new PDRF.
     * Without this, consecutive PDRFs may be silently dropped (0ms refresh). */
    while (Epd_Is_Busy()) Delay_Ms(5);

    epd_time_reset();

    /* PDTM1: B/W channel (MiniUI bit=1=black matches panel bit=1=black, no inversion) */
    Epaper_Hw_WriteCmd(0x14);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &new_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        for (col = 0; col < row_bytes; col++) {
            Epaper_Hw_WriteData(src[col]);
        }
    }

    /* PDTM2: Red channel (0x00 = no red) */
    Epaper_Hw_WriteCmd(0x15);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        for (col = 0; col < row_bytes; col++) {
            Epaper_Hw_WriteData(0x00);
        }
    }

    /* PON: Power ON (only if not already powered) */
    uint32_t t_before_pon = epd_time_mark();
    epaper_ensure_power_on();
    uint32_t t_after_pon = epd_time_mark();

    /* PDRF: trigger partial refresh */
    Epaper_Hw_WriteCmd(0x16);
    Epaper_Hw_WriteData((x >> 8) & 0x03);
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);

    /* Wait for BUSY to go LOW (refresh started), then back HIGH (done) */
    int ret = epd_wait_busy_counted(3000);
    uint32_t t_total = epd_time_mark();

    printf("[epd] partial(%d,%d,%d,%d): pon=%lums pdrf=%lums ret=%d\r\n",
           x, y, w, h,
           t_after_pon - t_before_pon,
           t_total - t_after_pon,
           ret);
}

/*********************************************************************
 * @fn      Epaper_DisplayImageDiff
 *
 * @brief   Full refresh — display new_image.
 *          BWR=0 + absolute LUT: DTM1=BW channel, DTM2=Red (none).
 *          MiniUI bit=1=black matches panel bit=1=black, no inversion.
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image)
{
    uint32_t i;

    (void)old_image;

    Epaper_Hw_WriteCmd(0x10);  /* DTM1 = BW channel */
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(new_image[i]);

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 = Red channel */
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(0x00);  /* no red */
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
