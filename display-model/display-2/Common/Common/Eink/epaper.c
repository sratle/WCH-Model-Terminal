/********************************** (C) COPYRIGHT *******************************
* File Name          : epaper.c
* Author             : E-ink Model Team
* Description        : JD79686AB 648x480 B/W e-paper driver.
*
* Init sequence writes full register set + two LUT sets:
*   - PSR:  BWR=0 mode (B/W + Red register), REG_EN=1 (use register LUT)
*   - PWR:  internal DC/DC, VGH=20V, VGL=-20V, VSH=15V, VSL=-15V
*   - FAST LUT: 20-frame single-phase drive (~400ms @ 50Hz = 2.5FPS)
*   - CLEAR LUT: 15-frame all-white clear + 20-frame drive (~700ms)
*
* The default OTP LUT has ~188 frames (~3.76s @ 50Hz) which is far too
* slow for interactive UI.  The fast register LUT trades contrast for
* speed: 20 frames gives fuller black drive (avoiding faint blacks).
*
* Gradual graying fix: POF (power-off) is sent after EVERY refresh so VCOM
* discharges to GND (via TDY 0x88 0x06).  This is the datasheet's intended
* flow (AUTO 0xE9 + 0xA5 = PON→DRF→POF).  Without POF, VCOM stays at
* -VCM_DC between refreshes, creating a continuous DC bias on non-refreshed
* pixels (which during PDRF follow the VCOM LUT only) — this causes the
* whole-screen graying observed during cursor movement.  Additionally, full
* refreshes (Epaper_Update) and every 20th partial refresh use the CLEAR
* LUT — a 2-phase waveform that first drives ALL pixels to white (VSL),
* clearing any residual, then drives to the target state.
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
 *  LUT registers for BWR=0 mode — ABSOLUTE value LUT (not transition-based)
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
 *  Level encoding (from datasheet):
 *    LUTC  (R20H): 00=-VCM_DC, 01=VSH+VCM_DC, 10=VSL+VCM_DC, 11=Floating(Hi-Z)
 *    LUTR/W/B (R22H-R24H): 00=GND, 01=VSH, 10=VSL, 11=VSHR
 *
 *  IMPORTANT: For this panel, VSL drives WHITE and VSH drives BLACK.
 *  This is confirmed by the datasheet's built-in LUT (Group 4, P19):
 *    LUTW = 0x80 → VSL (White LUT uses VSL to drive white)
 *    LUTB = 0x10 → VSH (Black LUT uses VSH to drive black)
 *  This is the OPPOSITE of standard e-paper convention.
 *
 *  NEVER use LUTC=Floating(0xFF): Hi-Z VCOM drifts uncontrolled and drives
 *  all pixels to one state (instant all-black/all-white).
 *==========================================================================*/

#define FAST_LUT_FRAMES           20   /* drive phase (~400ms @ 50Hz) */
#define CLEAR_LUT_PHASE1_FRAMES   15   /* all-white clear (~300ms)    */
#define CLEAR_LUT_PHASE2_FRAMES   20   /* normal drive  (~400ms)      */

/* ---- FAST LUT: single-group 20-frame drive (for partial refreshes) ----
 *   LUTC:  -VCM_DC (0x00) — VCOM reference, must stay controlled
 *   LUTR:  GND (0x00) — no red drive
 *   LUTW:  VSL (0xAA) — white pixels → VSL → white
 *   LUTB:  VSH (0x55) — black pixels → VSH → black
 *   LUTWW: GND (0x00) — unused */
static const uint8_t LUT_FAST_C[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_WW[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_R[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_W[42] = {
    0xAA, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_B[42] = {
    0x55, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};

/* ---- CLEAR LUT: 2-phase waveform for full refreshes ----
 *   Phase 1 (15 frames): Drive ALL pixels to white to clear residual charge
 *     LUTW: VSL (0xAA) — white pixels stay white
 *     LUTB: VSL (0xAA) — black pixels driven to white (clears residual)
 *   Phase 2 (20 frames): Normal drive to target state
 *     LUTW: VSL (0xAA) — white pixels stay white
 *     LUTB: VSH (0x55) — black pixels driven to black
 *   LUTC: -VCM_DC (0x00) both phases; LUTR/LUTWW: GND both phases
 *   Total: 35 frames @ 50Hz = 700ms (brief white flash, then content) */
static const uint8_t LUT_CLEAR_C[42] = {
    0x00, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x00, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_WW[42] = {
    0x00, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x00, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_R[42] = {
    0x00, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x00, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_W[42] = {
    0xAA, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0xAA, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
    0,0,0,0,0,0,  0,0,0,0,0,0,
};
/* LUTB: VSL(0xAA) in phase 1 (clear to white), VSH(0x55) in phase 2 (drive black) */
static const uint8_t LUT_CLEAR_B[42] = {
    0xAA, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x55, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
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
 *  Partial refresh counter — forces a clearing full refresh after
 *  PARTIAL_REFRESH_CLEAR_INTERVAL partials to eliminate the residual
 *  charge that accumulates as whole-screen graying.
 *==========================================================================*/

#define PARTIAL_REFRESH_CLEAR_INTERVAL  20
static uint16_t s_partial_count = 0;

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

/* Power off the panel — sends POF so VCOM discharges to GND (via TDY 0x88 0x06).
 * This eliminates the DC bias on non-refreshed pixels between partial
 * refreshes, which is the root cause of whole-screen gradual graying.
 * The datasheet's intended refresh flow is PON→DRF/PDRF→POF (see AUTO 0xE9).
 * Register and SRAM data are preserved across POF. */
static void epaper_power_off(void)
{
    Epaper_Hw_WriteCmd(0x02);               /* POF */
    epd_wait_busy_counted(1000);
    s_epd_powered_on = false;
}

/* Write all 5 LUT registers (R20H-R24H) with the given 42-byte arrays. */
static void epaper_write_lut(const uint8_t *lut_c,  const uint8_t *lut_ww,
                             const uint8_t *lut_r,  const uint8_t *lut_w,
                             const uint8_t *lut_b)
{
    Epaper_Hw_WriteCmd(0x20); Epaper_Hw_WriteDataBuf(lut_c,  42);
    Epaper_Hw_WriteCmd(0x21); Epaper_Hw_WriteDataBuf(lut_ww, 42);
    Epaper_Hw_WriteCmd(0x22); Epaper_Hw_WriteDataBuf(lut_r,  42);
    Epaper_Hw_WriteCmd(0x23); Epaper_Hw_WriteDataBuf(lut_w,  42);
    Epaper_Hw_WriteCmd(0x24); Epaper_Hw_WriteDataBuf(lut_b,  42);
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
 *          Custom 20-frame FAST LUT gives ~400ms per partial refresh.
 *          CLEAR LUT (15+20 frames) used for full refreshes to clear
 *          residual charge and prevent gradual graying.
 *
 *          In BWR=0 mode:
 *          - DTM1/PDTM1 = B/W channel (bit=1=black, matches MiniUI)
 *          - DTM2/PDTM2 = Red channel (0x00 = no red)
 *          - LUTR/LUTW/LUTB drive pixels based on absolute pixel value
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
     *   Absolute LUT: IC drives pixels based on its value in PDTM1.
     *   Init loads FAST LUT (20 frames @ 50Hz = 400ms per partial refresh).
     *   Epaper_Update() and periodic full refreshes swap in CLEAR LUT
     *   temporarily, then restore FAST LUT here. */
    epaper_write_lut(LUT_FAST_C, LUT_FAST_WW, LUT_FAST_R,
                     LUT_FAST_W, LUT_FAST_B);

    s_epd_powered_on = false;
    s_partial_count = 0;

    printf("[epd] init done: BWR=0, REG_EN=1, fast LUT (%d frames @ 50Hz = %dms), clear every %d partials\r\n",
           FAST_LUT_FRAMES, FAST_LUT_FRAMES * 20,
           PARTIAL_REFRESH_CLEAR_INTERVAL);
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
 * @brief   PON -> DRF (full refresh) with CLEAR LUT -> POF.
 *          The CLEAR LUT is a 2-phase waveform: phase 1 drives all pixels
 *          to white (clearing residual charge), phase 2 drives to target.
 *          This eliminates the gradual graying that accumulates from
 *          repeated fast partial refreshes.
 *
 *          POF is sent after the refresh so VCOM discharges to GND (via
 *          TDY 0x88 0x06).  This is the datasheet's intended refresh flow
 *          (AUTO 0xE9 + 0xA5 = PON→DRF→POF) and eliminates the DC bias
 *          on non-refreshed pixels that causes whole-screen graying.
 *          Registers and SRAM are preserved across POF; the next refresh
 *          re-sends PON (~100-200ms overhead).
 */
void Epaper_Update(void)
{
    epd_time_reset();
    uint32_t t_before_pon = epd_time_mark();
    epaper_ensure_power_on();
    uint32_t t_after_pon = epd_time_mark();

    /* Swap in CLEAR LUT for this full refresh (clears residual), then
     * restore FAST LUT so subsequent partial refreshes stay fast. */
    epaper_write_lut(LUT_CLEAR_C, LUT_CLEAR_WW, LUT_CLEAR_R,
                     LUT_CLEAR_W, LUT_CLEAR_B);

    Epaper_Hw_WriteCmd(0x12);  /* DRF */
    int ret = epd_wait_busy_counted(10000);
    uint32_t t_after_drf = epd_time_mark();

    epaper_write_lut(LUT_FAST_C, LUT_FAST_WW, LUT_FAST_R,
                     LUT_FAST_W, LUT_FAST_B);

    /* POF: discharge VCOM to GND to eliminate DC bias between refreshes. */
    epaper_power_off();
    uint32_t t_total = epd_time_mark();

    /* A clearing full refresh resets the partial counter. */
    s_partial_count = 0;

    printf("[epd] update(full/clear %d+%dF): pon=%lums drf=%lums pof=%lums(total) ret=%d\r\n",
           CLEAR_LUT_PHASE1_FRAMES, CLEAR_LUT_PHASE2_FRAMES,
           t_after_pon - t_before_pon,
           t_after_drf - t_after_pon,
           t_total - t_after_drf, ret);
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
 *            -> POF(R02h) [wait BUSY]   (discharge VCOM to GND)
 *
 *          NOTE: Datasheet mentions "must send command 11H" after data
 *          transmission, but sending 0x11 causes PDRF to NOT trigger
 *          (BUSY never goes LOW, refresh completes in 0ms with no actual
 *          screen update).  The vendor reference code (EPD.c) also does
 *          NOT send 0x11.  CS toggling per byte is sufficient to signal
 *          end of data transmission.
 *
 *          POF is sent after each partial refresh so VCOM discharges to
 *          GND (via TDY 0x88 0x06).  This is the datasheet's intended
 *          refresh flow (AUTO 0xE9 + 0xA5 = PON→DRF→POF).  Without POF,
 *          VCOM stays at -VCM_DC between refreshes, creating a continuous
 *          DC bias on non-refreshed pixels (which during PDRF follow the
 *          VCOM LUT only) — this is the root cause of the whole-screen
 *          graying observed during cursor movement.  POF eliminates the
 *          inter-refresh DC bias; registers and SRAM are preserved, so the
 *          next refresh simply re-sends PON (~100-200ms overhead).
 *
 *          Data is sent directly (no inversion): MiniUI bit=1=black matches
 *          panel bit=1=black (CDI DDX=00, no polarity inversion).
 *
 *          DFV_EN=0 in PDRF: unchanged pixels within the region follow the
 *          data LUT (same as changed pixels), which is correct for BWR=0
 *          absolute LUT mode.
 *
 *          With the 20-frame fast LUT, each partial refresh takes ~400ms
 *          (2.5 FPS) regardless of region size.
 *
 *          GRAYING FIX: Every PARTIAL_REFRESH_CLEAR_INTERVAL partials, we
 *          force a full clearing refresh (DRF + CLEAR LUT) using the full
 *          new_frame to eliminate residual charge accumulation.  The caller
 *          then updates its old buffer for the dirty region only, which is
 *          safe because non-dirty regions already match between old/new.
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
    (void)old_frame;  /* Absolute LUT: IC uses PDTM1 data directly */

    /* ---- Periodic clearing full refresh ----
     * After PARTIAL_REFRESH_CLEAR_INTERVAL partials, the fast LUT's
     * incomplete drive has accumulated enough residual to cause visible
     * graying.  Force a full refresh with the CLEAR LUT (2-phase: all-white
     * clear + normal drive) using the full new_frame buffer.  This resets
     * the panel state and the partial counter. */
    if (s_partial_count >= PARTIAL_REFRESH_CLEAR_INTERVAL) {
        s_partial_count = 0;

        /* Wait for IC idle before switching to full-refresh path */
        while (Epd_Is_Busy()) Delay_Ms(5);

        epd_time_reset();
        uint32_t t_before_pon = epd_time_mark();
        epaper_ensure_power_on();
        uint32_t t_after_pon = epd_time_mark();

        /* Write full new frame to DTM1/DTM2 (full-frame display refresh) */
        Epaper_DisplayImageDiff(old_frame, new_frame);

        /* CLEAR LUT: phase 1 clears all to white, phase 2 drives to target */
        epaper_write_lut(LUT_CLEAR_C, LUT_CLEAR_WW, LUT_CLEAR_R,
                         LUT_CLEAR_W, LUT_CLEAR_B);

        Epaper_Hw_WriteCmd(0x12);  /* DRF — full refresh */
        int ret = epd_wait_busy_counted(10000);
        uint32_t t_after_drf = epd_time_mark();

        /* Restore FAST LUT for subsequent partial refreshes */
        epaper_write_lut(LUT_FAST_C, LUT_FAST_WW, LUT_FAST_R,
                         LUT_FAST_W, LUT_FAST_B);

        /* POF: discharge VCOM to GND to eliminate DC bias between refreshes. */
        epaper_power_off();
        uint32_t t_total = epd_time_mark();

        printf("[epd] partial->full(clear after %d): (%d,%d,%d,%d) pon=%lums drf=%lums pof=%lums ret=%d\r\n",
               PARTIAL_REFRESH_CLEAR_INTERVAL, x, y, w, h,
               t_after_pon - t_before_pon,
               t_after_drf - t_after_pon,
               t_total - t_after_drf, ret);
        return;
    }

    s_partial_count++;

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
    uint32_t t_after_pdrf = epd_time_mark();

    /* POF: discharge VCOM to GND to eliminate DC bias between refreshes.
     * This is the key fix for the whole-screen graying observed during
     * cursor movement — without POF, VCOM stays at -VCM_DC and continuously
     * biases non-refreshed pixels (which follow VCOM LUT only during PDRF). */
    epaper_power_off();
    uint32_t t_total = epd_time_mark();

    printf("[epd] partial(%d,%d,%d,%d): pon=%lums pdrf=%lums pof=%lums ret=%d\r\n",
           x, y, w, h,
           t_after_pon - t_before_pon,
           t_after_pdrf - t_after_pon,
           t_total - t_after_pdrf,
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
