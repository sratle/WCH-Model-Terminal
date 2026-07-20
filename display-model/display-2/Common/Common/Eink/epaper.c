/********************************** (C) COPYRIGHT *******************************
* File Name          : epaper.c
* Author             : E-ink Model Team
* Description        : JD79686AB 648x480 B/W e-paper driver.
*
*  V2.0 — transition-based waveform (BWR=1, BW mode):
*
*  In B/W mode the IC selects one of four LUTs per pixel from the
*  (New, Old) data pair (CDI DDX=0 mapping):
*      00 -> LUTWW   (white -> white)
*      01 -> LUTBW   (black -> white)
*      10 -> LUTWB   (white -> black)
*      11 -> LUTBB   (black -> black)
*  DTM1/PDTM1 carry OLD data, DTM2/PDTM2 carry NEW data.
*
*  FAST LUT (partial refresh, ~FAST_LUT_FRAMES @ 50 Hz):
*    transitions (W->B / B->W) get the full drive, while W->W and B->B
*    get a short FAST_LUT_MAINT_FRAMES maintenance pulse.  PDRF runs
*    with DFV_EN=0 so unchanged pixels DO receive that maintenance
*    pulse — this keeps the background white instead of decaying to
*    gray (pure DFV_EN=1 differential mode was too weak and caused
*    whole-screen gray drift; EPD_DFV_EN=1 re-enables it if wanted).
*
*  CLEAR LUT (full refresh), 3 phases: shake (all pixels toward BLACK,
*  loosens particle hysteresis) -> erase (all toward WHITE) -> drive to
*  target.  Used by Epaper_Update()/Epaper_UpdateFull() and every
*  PARTIAL_REFRESH_CLEAR_INTERVAL partial refreshes; the boot sequence
*  runs it twice for a deep clean (power-cycle ghosting otherwise
*  survives a single clear).
*
*  POF is still issued after EVERY refresh so VCOM discharges to GND
*  (via TDY 0x88 0x06); this was the verified fix for the whole-screen
*  gradual graying and is kept.  With DFV_EN=1 the inter-refresh DC bias
*  matters much less, so dropping the per-refresh POF is a future
*  (hardware-test-required) latency optimisation.
*
*  Temperature compensation (JD79686AB §2.5.3): the internal sensor is
*  read at init.  Below LUT_COLD_THRESHOLD_C the frame counts are doubled
*  and the "2 frame off" ending is applied (TDY 0x92 0x00 = VGL HiZ,
*  POF wait extended to 5 s).  Cold waveforms need on-hardware fine
*  tuning; the scaling here is a conservative baseline.
*
*  Panel pixel format: 1bpp, MSB = leftmost pixel, bit=1 = BLACK,
*  bit=0 = WHITE (empirically determined, matches MiniUI directly).
********************************************************************************/
#include "epaper.h"
#include "../platform_time.h"
#include <stdbool.h>
#include <stdint.h>

/*============================================================================
 *  Tunables
 *==========================================================================*/

#define FAST_LUT_FRAMES           15   /* transition drive (~300ms @ 50Hz) */
/* Maintenance drive for unchanged (W->W / B->B) pixels.
 * MUST equal FAST_LUT_FRAMES on this module: a shorter pulse (3f was
 * tried) leaves pixels under-driven, so each refresh's relaxation error
 * (POF discharge / particle drift-back) is not corrected and accumulates
 * into stacking ghost images.  Full drive self-corrects every refresh —
 * and costs no extra time (refresh length is set by the longest LUT). */
#define FAST_LUT_MAINT_FRAMES     FAST_LUT_FRAMES

/* CLEAR waveform (full refresh), 3 phases:
 *   shake: drive ALL pixels toward BLACK first.  This is the classic
 *          e-ink "shake" phase that loosens particle hysteresis — without
 *          it, strongly-held black pixels refuse to move back to white
 *          and the previous page's image bounces back as a ghost.
 *   erase: drive ALL pixels toward WHITE (erases the shaken state).
 *   drive: drive each transition to its target state. */
#define CLEAR_SHAKE_FRAMES        12   /* black shake (~240ms)  */
#define CLEAR_LUT_PHASE1_FRAMES   20   /* white erase (~400ms)  */
#define CLEAR_LUT_PHASE2_FRAMES   20   /* target drive (~400ms) */

/* After this many partial refreshes, force a full CLEAR refresh. */
#define PARTIAL_REFRESH_CLEAR_INTERVAL  50

/* Below this temperature: double frame counts, VGL HiZ ending, 5s POF */
#define LUT_COLD_THRESHOLD_C      10

/* Set 0 to fall back to the legacy absolute (BWR=0) waveform. */
#define EPD_BW_TRANSITION         1

/* Set 1 for pure differential refresh (New==Old pixels follow VCOM only).
 * Lowest pixel stress but white background decays toward gray; keep 0 so
 * unchanged pixels receive the full maintenance drive instead. */
#define EPD_DFV_EN                0

/* Send POF (VCOM discharge) after every refresh — the verified fix for
 * whole-screen gradual graying.  Set 0 ONLY for A/B debugging of refresh
 * artifacts (ghost / drift-back experiments). */
#define EPD_POF_AFTER_REFRESH     1

/*============================================================================
 *  Power state tracking — avoids redundant PON commands between
 *  consecutive refreshes.
 *==========================================================================*/

static bool     s_epd_powered_on = false;
static uint16_t s_partial_count  = 0;
static int8_t   s_temp_c         = 25;   /* last measured panel temperature */
static uint8_t  s_frame_scale    = 1;    /* 2 when cold */
static uint8_t  s_scale_mode     = 1;    /* 0=auto(TSC), 1=force x1, 2=force x2.
                                          * Default 1 (force x1): the TSC
                                          * readback returns 0x00 on this
                                          * module (MISO read unsupported),
                                          * so auto mode misfires the cold
                                          * scaling.  Tuned on hardware:
                                          * x1 frames + vdcs=0x00 gives an
                                          * acceptable ghost level. */
static uint8_t  s_vdcs           = 0x00; /* VCOM DC level (R82H) — tuned on
                                          * hardware: 0x00 gives the lightest
                                          * inverted-ghost on this module */

/* Runtime-tunable waveform parameters (defaults from the #defines above;
 * adjustable live via the UART2 debug CLI for on-hardware LUT tuning) */
static uint8_t  s_fast_frames    = FAST_LUT_FRAMES;
static uint8_t  s_maint_frames   = FAST_LUT_MAINT_FRAMES;
static uint8_t  s_shake_frames   = CLEAR_SHAKE_FRAMES;
static uint8_t  s_erase_frames   = CLEAR_LUT_PHASE1_FRAMES;
static uint8_t  s_drive_frames   = CLEAR_LUT_PHASE2_FRAMES;
static uint8_t  s_dfv_en         = EPD_DFV_EN;
static uint8_t  s_pof_after      = EPD_POF_AFTER_REFRESH;

/*============================================================================
 *  LUT construction helpers
 *
 *  One LUT stage = 6 bytes: { level_sel, f1, f2, f3, f4, repeat }
 *    level_sel packs four 2-bit phase levels, phase 1 in bits [7:6].
 *    Source LUT levels: 00=GND, 01=VSH, 10=VSL, 11=VSHR
 *    LUTC levels:       00=-VCM_DC, 01=VSH+VCM_DC, 10=VSL+VCM_DC, 11=Hi-Z
 *
 *  Panel convention (verified on hardware): VSL drives WHITE, VSH drives
 *  BLACK — the opposite of standard e-paper convention.
 *==========================================================================*/

#define EPD_LVL_GND    0x00   /* phase1 = GND (no source drive) */
#define EPD_LVL_VSH    0x40   /* phase1 = VSH (drive BLACK)     */
#define EPD_LVL_VSL    0x80   /* phase1 = VSL (drive WHITE)     */

#define LUT_REG_COUNT   42    /* 7 stages x 6 bytes (BW mode)   */

/* Fill one stage inside a 42-byte LUT register image. */
static void lut_set_stage(uint8_t *lut, uint8_t stage, uint8_t level,
                          uint8_t f1, uint8_t f2, uint8_t f3, uint8_t f4,
                          uint8_t repeat)
{
    uint8_t *s = &lut[(uint16_t)stage * 6];
    uint8_t sc = s_frame_scale;
    s[0] = level;
    s[1] = f1 ? (uint8_t)(f1 * sc) : 0;
    s[2] = f2 ? (uint8_t)(f2 * sc) : 0;
    s[3] = f3 ? (uint8_t)(f3 * sc) : 0;
    s[4] = f4 ? (uint8_t)(f4 * sc) : 0;
    s[5] = repeat;
}

/* Runtime-built LUT images */
static uint8_t s_lut_fast_c[LUT_REG_COUNT];
static uint8_t s_lut_fast_ww[LUT_REG_COUNT];
static uint8_t s_lut_fast_bw[LUT_REG_COUNT];
static uint8_t s_lut_fast_wb[LUT_REG_COUNT];
static uint8_t s_lut_fast_bb[LUT_REG_COUNT];

static uint8_t s_lut_clear_c[LUT_REG_COUNT];
static uint8_t s_lut_clear_ww[LUT_REG_COUNT];
static uint8_t s_lut_clear_bw[LUT_REG_COUNT];
static uint8_t s_lut_clear_wb[LUT_REG_COUNT];
static uint8_t s_lut_clear_bb[LUT_REG_COUNT];

/* Rebuild both LUT sets (call after temperature / scale changes). */
static void epaper_build_luts(void)
{
    memset(s_lut_fast_c,  0, LUT_REG_COUNT);
    memset(s_lut_fast_ww, 0, LUT_REG_COUNT);
    memset(s_lut_fast_bw, 0, LUT_REG_COUNT);
    memset(s_lut_fast_wb, 0, LUT_REG_COUNT);
    memset(s_lut_fast_bb, 0, LUT_REG_COUNT);
    memset(s_lut_clear_c,  0, LUT_REG_COUNT);
    memset(s_lut_clear_ww, 0, LUT_REG_COUNT);
    memset(s_lut_clear_bw, 0, LUT_REG_COUNT);
    memset(s_lut_clear_wb, 0, LUT_REG_COUNT);
    memset(s_lut_clear_bb, 0, LUT_REG_COUNT);

    /* ---- FAST: full-strength drive on ALL pixels in the region ----
     * Transitions and unchanged pixels alike get the full drive toward
     * their target colour (W->W and B->W use VSL=WHITE, W->B and B->B
     * use VSH=BLACK).  This is the previously proven drive strength:
     * every refresh fully re-establishes the pixel state, so per-refresh
     * relaxation errors cannot accumulate into stacking ghosts. */
    lut_set_stage(s_lut_fast_c,  0, EPD_LVL_GND, s_fast_frames, 0,0,0, 1);
    lut_set_stage(s_lut_fast_ww, 0, EPD_LVL_VSL, s_maint_frames, 0,0,0, 1); /* W->W: hold white */
    lut_set_stage(s_lut_fast_bw, 0, EPD_LVL_VSL, s_fast_frames, 0,0,0, 1);  /* B->W: to white   */
    lut_set_stage(s_lut_fast_wb, 0, EPD_LVL_VSH, s_fast_frames, 0,0,0, 1);  /* W->B: to black   */
    lut_set_stage(s_lut_fast_bb, 0, EPD_LVL_VSH, s_maint_frames, 0,0,0, 1); /* B->B: hold black */

    /* ---- CLEAR stage 0: black shake (loosen hysteresis) ----
     * Driving everything toward BLACK first releases strongly-held
     * particles; this is what kills the "previous page bounces back"
     * ghost that a direct white erase cannot fully remove. */
    lut_set_stage(s_lut_clear_c,  0, EPD_LVL_GND, s_shake_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_ww, 0, EPD_LVL_VSH, s_shake_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_bw, 0, EPD_LVL_VSH, s_shake_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_wb, 0, EPD_LVL_VSH, s_shake_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_bb, 0, EPD_LVL_VSH, s_shake_frames, 0,0,0, 1);

    /* ---- CLEAR stage 1: white erase (all pixels toward white) ---- */
    lut_set_stage(s_lut_clear_c,  1, EPD_LVL_GND, s_erase_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_ww, 1, EPD_LVL_VSL, s_erase_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_bw, 1, EPD_LVL_VSL, s_erase_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_wb, 1, EPD_LVL_VSL, s_erase_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_bb, 1, EPD_LVL_VSL, s_erase_frames, 0,0,0, 1);

    /* ---- CLEAR stage 2: drive to target state ---- */
    lut_set_stage(s_lut_clear_c,  2, EPD_LVL_GND, s_drive_frames, 0,0,0, 1);
    lut_set_stage(s_lut_clear_ww, 2, EPD_LVL_VSL, s_drive_frames, 0,0,0, 1); /* stay white */
    lut_set_stage(s_lut_clear_bw, 2, EPD_LVL_VSL, s_drive_frames, 0,0,0, 1); /* to white   */
    lut_set_stage(s_lut_clear_wb, 2, EPD_LVL_VSH, s_drive_frames, 0,0,0, 1); /* to black   */
    lut_set_stage(s_lut_clear_bb, 2, EPD_LVL_VSH, s_drive_frames, 0,0,0, 1); /* back black */
}

/*============================================================================
 *  Legacy absolute (BWR=0) LUT set — fallback when EPD_BW_TRANSITION == 0
 *==========================================================================*/
#if !EPD_BW_TRANSITION
static const uint8_t LUT_FAST_C[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_WW[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_R[42] = {
    0x00, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_W[42] = {
    0xAA, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_FAST_B[42] = {
    0x55, FAST_LUT_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_C[42] = {
    0x00, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x00, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_WW[42] = {
    0x00, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x00, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_R[42] = {
    0x00, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x00, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_W[42] = {
    0xAA, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0xAA, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
static const uint8_t LUT_CLEAR_B[42] = {
    0xAA, CLEAR_LUT_PHASE1_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0x55, CLEAR_LUT_PHASE2_FRAMES, 0x00, 0x00, 0x00, 0x01,
    0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,  0,0,0,0,0,0,
};
#endif /* !EPD_BW_TRANSITION */

/*============================================================================
 *  Internal helpers
 *==========================================================================*/

/* Send the 8-byte region header (x, y, w, h) for PDTM1/PDTM2 commands.
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

/* Power off the panel — sends POF so VCOM discharges to GND (via TDY 0x88
 * 0x06).  Cold panels need a longer discharge wait (datasheet: 5s). */
static void epaper_power_off(void)
{
    Epaper_Hw_WriteCmd(0x02);               /* POF */
    Epaper_Hw_WaitBusy(s_frame_scale > 1 ? 5000 : 1000);
    s_epd_powered_on = false;
}

/* Write all 5 LUT registers (R20H-R24H) with the given 42-byte images.
 * BW mode order: LUTC / LUTWW / LUTBW / LUTWB / LUTBB. */
static void epaper_write_lut(const uint8_t *lut_c,  const uint8_t *lut_ww,
                             const uint8_t *lut_bw, const uint8_t *lut_wb,
                             const uint8_t *lut_bb)
{
    Epaper_Hw_WriteCmd(0x20); Epaper_Hw_WriteDataBuf(lut_c,  42);
    Epaper_Hw_WriteCmd(0x21); Epaper_Hw_WriteDataBuf(lut_ww, 42);
    Epaper_Hw_WriteCmd(0x22); Epaper_Hw_WriteDataBuf(lut_bw, 42);
    Epaper_Hw_WriteCmd(0x23); Epaper_Hw_WriteDataBuf(lut_wb, 42);
    Epaper_Hw_WriteCmd(0x24); Epaper_Hw_WriteDataBuf(lut_bb, 42);
}

static void epaper_load_fast_lut(void)
{
#if EPD_BW_TRANSITION
    epaper_write_lut(s_lut_fast_c, s_lut_fast_ww, s_lut_fast_bw,
                     s_lut_fast_wb, s_lut_fast_bb);
#else
    epaper_write_lut(LUT_FAST_C, LUT_FAST_WW, LUT_FAST_R,
                     LUT_FAST_W, LUT_FAST_B);
#endif
}

static void epaper_load_clear_lut(void)
{
#if EPD_BW_TRANSITION
    epaper_write_lut(s_lut_clear_c, s_lut_clear_ww, s_lut_clear_bw,
                     s_lut_clear_wb, s_lut_clear_bb);
#else
    epaper_write_lut(LUT_CLEAR_C, LUT_CLEAR_WW, LUT_CLEAR_R,
                     LUT_CLEAR_W, LUT_CLEAR_B);
#endif
}

/* Read the internal temperature sensor (R40H TSC), signed degrees C. */
static int8_t epaper_read_temperature(void)
{
    uint8_t buf[2] = {0, 0};
    Epaper_Hw_ReadReg(0x40, buf, 2);
    return (int8_t)buf[0];
}

/*============================================================================
 *  Public API
 *==========================================================================*/

/*********************************************************************
 * @fn      Epaper_Init
 *
 * @brief   Initialization.
 *          Transition mode (default): BWR=1 + REG_EN=1, FAST transition
 *          LUT, DFV_EN differential partial refresh.
 *          Fallback (EPD_BW_TRANSITION=0): BWR=0 absolute LUT.
 */
void Epaper_Init(void)
{
    Epaper_Hw_Init();
    printf("[epd_hw] gpio init ok, BUSY=0x%08lX\r\n", (uint32_t)EPD_BUSY_READ());
    Epaper_Hw_Reset();
    int ret = Epaper_Hw_WaitBusy(2000);
    printf("[epd_hw] reset done, wait=%d\r\n", ret);

    /* ---- TDY commands (not stored in OTP, must write every init) ---- */
    Epaper_Hw_WriteCmd(0x08); Epaper_Hw_WriteData(0x00);  /* TDY dummy */
    epaper_write_tdy(0x60, 0xA5);  /* TDY key (unlock) */
    epaper_write_tdy(0x93, 0x18);  /* Stop-pulse mechanism off */
    epaper_write_tdy(0x73, 0x05);  /* Vcom driving adjust */

    /* ---- Temperature read & cold-weather adaptation ----
     * NOTE: on some modules the MISO read path does not work and TSC
     * reads back 0x00, which would wrongly trigger the cold scaling.
     * `scale` CLI command (Epaper_SetFrameScale) can force x1/x2. */
    s_temp_c = epaper_read_temperature();
    if (s_scale_mode == 1)      s_frame_scale = 1;
    else if (s_scale_mode == 2) s_frame_scale = 2;
    else s_frame_scale = (s_temp_c < LUT_COLD_THRESHOLD_C) ? 2 : 1;
    if (s_frame_scale > 1) {
        /* Datasheet §2.5.3-B (<10°C): VGL Hi-Z ending instead of pull-GND */
        epaper_write_tdy(0x92, 0x00);
    } else {
        epaper_write_tdy(0x92, 0x08);  /* VGL pull GND */
    }
    epaper_write_tdy(0xA8, 0x3A);  /* r_partial_all_gate_en */
    epaper_write_tdy(0x88, 0x06);  /* Power-off Vcom discharge to GND */

    /* ---- PSR (R00H) ----
     *   Transition: 0xBF = RES:10, REG_EN:1, BWR:1, UD:1, SHL:1, SHD_N:1, RST_N:1
     *   Fallback:   0xAF = same but BWR:0 (B/W/Red register mode)
     */
    Epaper_Hw_WriteCmd(0x00);
#if EPD_BW_TRANSITION
    Epaper_Hw_WriteData(0xBF);
#else
    Epaper_Hw_WriteData(0xAF);
#endif

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
     *   DDX=00 (no data polarity inversion).  In BW mode this maps
     *   (New,Old): 00->LUTWW, 01->LUTBW, 10->LUTWB, 11->LUTBB with
     *   MiniUI bit=1=black matching panel bit=1=black directly. */
    Epaper_Hw_WriteCmd(0x50); Epaper_Hw_WriteData(0x47);

    /* ---- TRES (R61H): 648x480 ---- */
    Epaper_Hw_WriteCmd(0x61);
    Epaper_Hw_WriteData(0x02); Epaper_Hw_WriteData(0x88);
    Epaper_Hw_WriteData(0x01); Epaper_Hw_WriteData(0xE0);

    /* ---- VDCS (R82H) ---- */
    Epaper_Hw_WriteCmd(0x82); Epaper_Hw_WriteData(s_vdcs);

    /* ---- PWS (RE8H) ---- */
    Epaper_Hw_WriteCmd(0xE8); Epaper_Hw_WriteData(0x40);

    /* ---- TCON (R60H) ---- */
    Epaper_Hw_WriteCmd(0x60); Epaper_Hw_WriteData(0x04);

    /* ---- Build and load the FAST LUT ---- */
    epaper_build_luts();
    epaper_load_fast_lut();

    s_epd_powered_on = false;
    s_partial_count = 0;

    printf("[epd] init done: %s, temp=%dC scale=x%d, fast=%df clear=%d+%d+%df, clear-every-%d\r\n",
#if EPD_BW_TRANSITION
           "BW transition (BWR=1)",
#else
           "BWR=0 absolute",
#endif
           (int)s_temp_c, (int)s_frame_scale,
           s_fast_frames * s_frame_scale,
           s_shake_frames * s_frame_scale,
           s_erase_frames * s_frame_scale,
           s_drive_frames * s_frame_scale,
           PARTIAL_REFRESH_CLEAR_INTERVAL);
}

/*********************************************************************
 * @fn      Epaper_GetTemperatureC
 *
 * @brief   Temperature (°C) measured at the last Epaper_Init().
 */
int8_t Epaper_GetTemperatureC(void)
{
    return s_temp_c;
}

/*********************************************************************
 * @fn      Epaper_SetVdcs
 *
 * @brief   Set the VCOM DC level (R82H).  Adjust to trim gray bias on the
 *          physical module; persist via settings if needed.
 */
void Epaper_SetVdcs(uint8_t vdcs)
{
    s_vdcs = vdcs & 0x7F;
    Epaper_Hw_WriteCmd(0x82);
    Epaper_Hw_WriteData(s_vdcs);
}

/*********************************************************************
 * @fn      Epaper_WriteTdy
 *
 * @brief   Write a TDY (test-mode) register — exposed for on-hardware
 *          waveform tuning via the debug CLI.
 */
void Epaper_WriteTdy(uint8_t addr, uint8_t val)
{
    epaper_write_tdy(addr, val);
}

/* Rebuild LUTs from the current runtime parameters and reload FAST LUT */
static void epaper_retune(void)
{
    epaper_build_luts();
    epaper_load_fast_lut();
}

/*********************************************************************
 * @fn      Epaper_SetFastFrames / Epaper_SetMaintFrames
 *
 * @brief   Tune FAST (partial refresh) frame counts live.
 */
void Epaper_SetFastFrames(uint8_t frames)
{
    if (frames < 1) frames = 1;
    if (frames > 60) frames = 60;
    s_fast_frames = frames;
    epaper_retune();
}

void Epaper_SetMaintFrames(uint8_t frames)
{
    if (frames > 60) frames = 60;
    s_maint_frames = frames;
    epaper_retune();
}

/*********************************************************************
 * @fn      Epaper_SetClearFrames
 *
 * @brief   Tune CLEAR waveform phases live (shake / erase / drive).
 */
void Epaper_SetClearFrames(uint8_t shake, uint8_t erase, uint8_t drive)
{
    if (shake > 60) shake = 60;
    if (erase < 1)  erase = 1;
    if (erase > 60) erase = 60;
    if (drive < 1)  drive = 1;
    if (drive > 60) drive = 60;
    s_shake_frames = shake;
    s_erase_frames = erase;
    s_drive_frames = drive;
    epaper_retune();
}

/*********************************************************************
 * @fn      Epaper_SetDfv / Epaper_SetPofAfterRefresh
 *
 * @brief   Toggle DFV_EN (differential refresh) and per-refresh POF live.
 */
void Epaper_SetDfv(uint8_t en)
{
    s_dfv_en = en ? 1 : 0;
}

void Epaper_SetPofAfterRefresh(uint8_t en)
{
    s_pof_after = en ? 1 : 0;
}

/*********************************************************************
 * @fn      Epaper_SetFrameScale
 *
 * @brief   Frame-count scaling mode: 0=auto(from TSC), 1=force x1,
 *          2=force x2.  Useful when the temperature readback is broken
 *          (some modules cannot read TSC over MISO) and the cold
 *          scaling misfires.
 */
void Epaper_SetFrameScale(uint8_t mode)
{
    if (mode > 2) mode = 0;
    s_scale_mode = mode;
    if (mode == 1)      s_frame_scale = 1;
    else if (mode == 2) s_frame_scale = 2;
    else                s_frame_scale = (s_temp_c < LUT_COLD_THRESHOLD_C) ? 2 : 1;
    epaper_retune();
}

/*********************************************************************
 * @fn      Epaper_RefreshTemperature
 *
 * @brief   Re-read the panel temperature sensor and re-apply auto scaling.
 * @return  Temperature in °C (0x00 often means "read unsupported").
 */
int8_t Epaper_RefreshTemperature(void)
{
    s_temp_c = epaper_read_temperature();
    if (s_scale_mode == 0) {
        s_frame_scale = (s_temp_c < LUT_COLD_THRESHOLD_C) ? 2 : 1;
        epaper_retune();
    }
    return s_temp_c;
}

/*********************************************************************
 * @fn      Epaper_DumpTune
 *
 * @brief   Print the current waveform tuning values (debug CLI).
 */
void Epaper_DumpTune(void)
{
    printf("[epd] tune: vdcs=0x%02X temp=%dC scale=x%d(mode=%d) fast=%d maint=%d "
           "clear=%d+%d+%d dfv=%d pof=%d\r\n",
           s_vdcs, (int)s_temp_c, (int)s_frame_scale, (int)s_scale_mode,
           s_fast_frames * s_frame_scale, s_maint_frames * s_frame_scale,
           s_shake_frames * s_frame_scale, s_erase_frames * s_frame_scale,
           s_drive_frames * s_frame_scale,
           (int)s_dfv_en, (int)s_pof_after);
}

/*********************************************************************
 * @fn      Epaper_ClearWhite
 *
 * @brief   Load an all-white frame into SRAM (no refresh trigger).
 *          BW mode: DTM1 = OLD = 0x00, DTM2 = NEW = 0x00.
 */
void Epaper_ClearWhite(void)
{
    Epaper_Hw_WriteCmd(0x10);  /* DTM1 */
    Epaper_Hw_WriteDataFill(0x00, EPD_FRAME_SIZE);

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 */
    Epaper_Hw_WriteDataFill(0x00, EPD_FRAME_SIZE);
}

/*********************************************************************
 * @fn      Epaper_DisplayImage
 *
 * @brief   Load a full frame as both OLD and NEW (no refresh trigger).
 */
void Epaper_DisplayImage(const uint8_t *image)
{
    Epaper_Hw_WriteCmd(0x10);  /* DTM1 = OLD */
    Epaper_Hw_WriteDataBuf(image, EPD_FRAME_SIZE);

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 = NEW */
    Epaper_Hw_WriteDataBuf(image, EPD_FRAME_SIZE);
}

/*********************************************************************
 * @fn      Epaper_DisplayImageDiff
 *
 * @brief   Load OLD and NEW full frames into SRAM (no refresh trigger).
 *          Transition mode: DTM1 = old_image, DTM2 = new_image.
 *          Fallback mode:     DTM1 = new_image, DTM2 = zeros (red).
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image)
{
#if EPD_BW_TRANSITION
    Epaper_Hw_WriteCmd(0x10);  /* DTM1 = OLD */
    Epaper_Hw_WriteDataBuf(old_image, EPD_FRAME_SIZE);

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 = NEW */
    Epaper_Hw_WriteDataBuf(new_image, EPD_FRAME_SIZE);
#else
    (void)old_image;
    Epaper_Hw_WriteCmd(0x10);  /* DTM1 = BW channel */
    Epaper_Hw_WriteDataBuf(new_image, EPD_FRAME_SIZE);

    Epaper_Hw_WriteCmd(0x13);  /* DTM2 = Red channel */
    Epaper_Hw_WriteDataFill(0x00, EPD_FRAME_SIZE);
#endif
}

/*********************************************************************
 * @fn      Epaper_Update
 *
 * @brief   Trigger a full-screen refresh on the data already in SRAM:
 *            PON -> CLEAR LUT -> DRF -> POF (FAST LUT restored after).
 *          Phase 1 of the CLEAR LUT drives all pixels toward white
 *          (erasing residual charge), phase 2 drives to the target.
 */
void Epaper_Update(void)
{
    uint32_t t0 = Platform_Millis();

    epaper_ensure_power_on();
    uint32_t t1 = Platform_Millis();

    epaper_load_clear_lut();

    Epaper_Hw_WriteCmd(0x12);  /* DRF */
    int ret = Epaper_Hw_WaitBusy(15000);
    uint32_t t2 = Platform_Millis();

    epaper_load_fast_lut();

    if (s_pof_after) epaper_power_off();
    uint32_t t3 = Platform_Millis();

    s_partial_count = 0;

    printf("[epd] update(clear %d+%d+%dF): pon=%lums drf=%lums pof=%lums ret=%d\r\n",
           s_shake_frames * s_frame_scale,
           s_erase_frames * s_frame_scale,
           s_drive_frames * s_frame_scale,
           t1 - t0, t2 - t1, t3 - t2, ret);
}

/*********************************************************************
 * @fn      Epaper_UpdateFull
 *
 * @brief   Full-screen refresh with the CLEAR waveform:
 *            DTM1=old / DTM2=new -> PON -> CLEAR LUT -> DRF -> POF
 *          Phase 1 of the CLEAR LUT drives all pixels toward white
 *          (erasing residual charge), phase 2 drives to the target.
 *          The FAST LUT is restored afterwards for partial refreshes.
 */
void Epaper_UpdateFull(const uint8_t *old_frame, const uint8_t *new_frame)
{
    uint32_t t0 = Platform_Millis();

    epaper_ensure_power_on();
    uint32_t t1 = Platform_Millis();

    Epaper_DisplayImageDiff(old_frame, new_frame);
    uint32_t t2 = Platform_Millis();

    epaper_load_clear_lut();

    Epaper_Hw_WriteCmd(0x12);  /* DRF */
    int ret = Epaper_Hw_WaitBusy(15000);
    uint32_t t3 = Platform_Millis();

    epaper_load_fast_lut();

    if (s_pof_after) epaper_power_off();
    uint32_t t4 = Platform_Millis();

    s_partial_count = 0;

    printf("[epd] update full(clear %d+%d+%dF): pon=%lums data=%lums drf=%lums pof=%lums ret=%d\r\n",
           s_shake_frames * s_frame_scale,
           s_erase_frames * s_frame_scale,
           s_drive_frames * s_frame_scale,
           t1 - t0, t2 - t1, t3 - t2, t4 - t3, ret);
}

/*********************************************************************
 * @fn      Epaper_PartialRefresh
 *
 * @brief   Partial refresh of a rectangular region.
 *
 *  Transition mode flow (verified against JD79686AB §3.1.11-13):
 *    PON (if needed)
 *    PDTM1(R14h) + OLD region data
 *    PDTM2(R15h) + NEW region data
 *    PDRF(R16h) with DFV_EN=1  -> pixels with New==Old follow VCOM LUT
 *                                 (not re-driven): hardware differential
 *    POF                        -> VCOM discharge to GND
 *
 *  Datasheet mentions sending 11H (DSP) after data transmission, but
 *  sending 0x11 makes PDRF a no-op on this module (verified), and the
 *  vendor reference code does not send it either.  CS toggling per
 *  transfer is sufficient.
 *
 *  Every PARTIAL_REFRESH_CLEAR_INTERVAL partials, a full CLEAR refresh
 *  is performed instead to eliminate accumulated residual charge.
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
    /* ---- Periodic clearing full refresh ---- */
    if (s_partial_count >= PARTIAL_REFRESH_CLEAR_INTERVAL) {
        s_partial_count = 0;

        /* Wait for IC idle before switching to the full-refresh path */
        while (Epd_Is_Busy()) Delay_Ms(5);

        printf("[epd] partial->full(clear after %d partials)\r\n",
               PARTIAL_REFRESH_CLEAR_INTERVAL);
        Epaper_UpdateFull(old_frame, new_frame);
        return;
    }

    s_partial_count++;

    uint16_t row_bytes = w / 8;
    int16_t  start_byte = x / 8;
    int16_t  row;

    /* Wait for IC to be truly idle before sending new PDRF.
     * Without this, consecutive PDRFs may be silently dropped. */
    while (Epd_Is_Busy()) Delay_Ms(5);

    uint32_t t0 = Platform_Millis();

    /* PON first: after the previous POF, SRAM cannot accept new data. */
    epaper_ensure_power_on();
    uint32_t t1 = Platform_Millis();

#if EPD_BW_TRANSITION
    /* PDTM1: OLD region data */
    Epaper_Hw_WriteCmd(0x14);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &old_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        Epaper_Hw_WriteDataBuf(src, row_bytes);
    }

    /* PDTM2: NEW region data */
    Epaper_Hw_WriteCmd(0x15);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &new_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        Epaper_Hw_WriteDataBuf(src, row_bytes);
    }
#else
    (void)old_frame;
    /* PDTM1: B/W channel (absolute) */
    Epaper_Hw_WriteCmd(0x14);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &new_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        Epaper_Hw_WriteDataBuf(src, row_bytes);
    }

    /* PDTM2: Red channel (0x00 = no red) */
    Epaper_Hw_WriteCmd(0x15);
    epaper_send_region_header(x, y, w, h);
    Epaper_Hw_WriteDataFill(0x00, (uint32_t)row_bytes * h);
#endif

    /* PDRF: trigger partial refresh.
     * 1st parameter bit7 = DFV_EN (runtime-tunable via debug CLI).
     * DFV_EN=0 (default): unchanged pixels follow their data LUTs and get
     *   the full maintenance drive — required on this module (weak drive
     *   lets ghosts accumulate).
     * DFV_EN=1: unchanged pixels follow VCOM only (lowest stress). */
    Epaper_Hw_WriteCmd(0x16);
#if EPD_BW_TRANSITION
    Epaper_Hw_WriteData((s_dfv_en ? 0x80 : 0x00) | ((x >> 8) & 0x03));
#else
    Epaper_Hw_WriteData((x >> 8) & 0x03);            /* DFV_EN=0 */
#endif
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);

    int ret = Epaper_Hw_WaitBusy(5000);
    uint32_t t2 = Platform_Millis();

    /* POF: discharge VCOM to GND (verified graying fix; runtime-tunable) */
    if (s_pof_after) epaper_power_off();
    uint32_t t3 = Platform_Millis();

    printf("[epd] partial(%d,%d,%d,%d): pon=%lums data+pdrf=%lums pof=%lums ret=%d\r\n",
           x, y, w, h, t1 - t0, t2 - t1, t3 - t2, ret);
}

/*********************************************************************
 * @fn      Epaper_Sleep
 *
 * @brief   POF then DSLP (deep sleep).
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
