/********************************** (C) COPYRIGHT *******************************
* File Name          : touch_matrix.c
* Author             : E-ink Model Team
* Version            : V1.1.0
* Date               : 2026/07/05
* Description        : TTP229 dual-chip 4x8 touch matrix driver.
*
* Two TTP229-BSF chips form a 4-column x 8-row touch matrix:
*   TTP229-1 (SCL1=PB8, SDO1=PB9) -> TP0..TP15
*   TTP229-2 (SCL2=PB10, SDO2=PB11) -> TP16..TP31
*
* TTP229 serial protocol (16-key mode, adapted from submodel-4 reference):
*   - SCL idle LOW
*   - Wait for SDO HIGH (Data Valid signal)
*   - Clock 17 SCL pulses: D0=padding, D1..D16 = TP0..TP15
*   - After 17 shifts, padding overflows: raw[15]=TP0, raw[0]=TP15
*   - SDO active LOW for touched: raw bit=1 → untouched, bit=0 → touched
*   - Calibration: touched = (~raw) & baseline
*
* The driver uses relative displacement mode (touchpad-like): finger movement
* on the grid produces delta cursor positions that are accumulated and fed
* to ui_input_inject_touch(). The cursor is rendered as an overlay.
********************************************************************************/
#include "touch_matrix.h"
#include "../MiniUI/miniui_input.h"
#include "../MiniUI/miniui_page.h"
#include "ch32v30x.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Pin Control Macros
 *===========================================================================*/

#define TM_SCL1_LOW()   do { GPIOB->BCR  = TM_SCL1_BIT; } while(0)
#define TM_SCL1_HIGH()  do { GPIOB->BSHR = TM_SCL1_BIT; } while(0)

#define TM_SCL2_LOW()   do { GPIOB->BCR  = TM_SCL2_BIT; } while(0)
#define TM_SCL2_HIGH()  do { GPIOB->BSHR = TM_SCL2_BIT; } while(0)

/*=============================================================================
 *  TTP229 Timing Constants (from submodel-4 reference)
 *===========================================================================*/

#define TM_SCL_DELAY_US    10    /* SCL pulse half-period (Tw >= 10us) */
#define TM_DV_TIMEOUT      5000  /* DV wait timeout (~100us at 144MHz) */

/* Tap vs swipe threshold: if cursor moves less than this (in pixels) between
 * touch-down and touch-up, it's a tap (CLICK); otherwise it's a swipe (no event). */
#define TM_TAP_THRESHOLD   15

/*=============================================================================
 *  TP-to-Grid Mapping
 *
 *  Physical layout (from Display-2.md):
 *    Row 0: TP0  TP1  TP2  TP3
 *    Row 1: TP12 TP13 TP14 TP15
 *    Row 2: TP7  TP6  TP5  TP4
 *    Row 3: TP11 TP10 TP9  TP8
 *    Row 4: TP16 TP17 TP18 TP19
 *    Row 5: TP28 TP29 TP30 TP31
 *    Row 6: TP23 TP22 TP21 TP20
 *    Row 7: TP27 TP26 TP25 TP24
 *===========================================================================*/

static const int8_t tp_to_row[32] = {
    /* TP0-3  */ 0, 0, 0, 0,
    /* TP4-7  */ 2, 2, 2, 2,
    /* TP8-11 */ 3, 3, 3, 3,
    /* TP12-15*/ 1, 1, 1, 1,
    /* TP16-19*/ 4, 4, 4, 4,
    /* TP20-23*/ 6, 6, 6, 6,
    /* TP24-27*/ 7, 7, 7, 7,
    /* TP28-31*/ 5, 5, 5, 5
};

static const int8_t tp_to_col[32] = {
    /* TP0-3  */ 0, 1, 2, 3,
    /* TP4-7  */ 3, 2, 1, 0,
    /* TP8-11 */ 3, 2, 1, 0,
    /* TP12-15*/ 0, 1, 2, 3,
    /* TP16-19*/ 0, 1, 2, 3,
    /* TP20-23*/ 3, 2, 1, 0,
    /* TP24-27*/ 3, 2, 1, 0,
    /* TP28-31*/ 0, 1, 2, 3
};

/*=============================================================================
 *  Driver State
 *===========================================================================*/

typedef enum {
    TM_STATE_IDLE = 0,
    TM_STATE_TRACKING,
} tm_state_t;

static struct {
    int16_t sensitivity_x;
    int16_t sensitivity_y;

    /* Cursor position (screen coordinates) */
    int16_t cursor_x;
    int16_t cursor_y;
    bool   cursor_visible;

    /* Last centroid (grid coordinates, fixed-point: *10) */
    int16_t last_col_x10;
    int16_t last_row_x10;
    bool    has_last_pos;

    /* Touch state */
    tm_state_t state;
    uint8_t    touch_id;

    /* Raw state (32-bit bitmap: bit i = TPi touched) */
    uint32_t raw_bits;

    /* TTP229 calibration baselines (one per chip) */
    uint16_t baseline1;
    uint16_t baseline2;

    /* Cached raw readings (returned when DV not ready) */
    uint16_t cached1;
    uint16_t cached2;

    /* Cursor rendered position (for dirty region tracking) */
    int16_t  cursor_rendered_x;
    int16_t  cursor_rendered_y;

    /* Gesture tracking: cursor position at touch-down (for tap vs swipe) */
    int16_t  start_cursor_x;
    int16_t  start_cursor_y;

    /* Debug throttle */
    uint32_t last_debug_ms;
    uint8_t  debug_move_count;
} s_tm;

/*=============================================================================
 *  TTP229 Serial Read (adapted from submodel-4)
 *
 *  Reads 16-bit raw data from one TTP229 chip.
 *  Waits for DV (SDO HIGH), then clocks 17 SCL pulses.
 *  Result: raw[15] = TP0, raw[14] = TP1, ..., raw[0] = TP15
 *  SDO active LOW: bit=1 → untouched, bit=0 → touched
 *===========================================================================*/

static uint16_t ttp229_read_raw(uint8_t chip_id, uint16_t cached)
{
    uint32_t raw32 = 0;
    uint32_t timeout;
    uint8_t i;

    if (chip_id == 0) {
        /* Wait for DV: SDO1 goes HIGH = data valid */
        timeout = TM_DV_TIMEOUT;
        while (!(GPIOB->INDR & TM_SDO1_BIT)) {
            if (--timeout == 0) return cached;
        }
        /* Clock 17 SCL pulses (D0=padding, D1..D16=TP0..TP15) */
        for (i = 0; i < 17; i++) {
            TM_SCL1_HIGH();
            Delay_Us(TM_SCL_DELAY_US);
            raw32 <<= 1;
            if (GPIOB->INDR & TM_SDO1_BIT) raw32 |= 1;
            TM_SCL1_LOW();
            Delay_Us(TM_SCL_DELAY_US);
        }
    } else {
        /* Wait for DV: SDO2 goes HIGH = data valid */
        timeout = TM_DV_TIMEOUT;
        while (!(GPIOB->INDR & TM_SDO2_BIT)) {
            if (--timeout == 0) return cached;
        }
        /* Clock 17 SCL pulses */
        for (i = 0; i < 17; i++) {
            TM_SCL2_HIGH();
            Delay_Us(TM_SCL_DELAY_US);
            raw32 <<= 1;
            if (GPIOB->INDR & TM_SDO2_BIT) raw32 |= 1;
            TM_SCL2_LOW();
            Delay_Us(TM_SCL_DELAY_US);
        }
    }

    return (uint16_t)(raw32 & 0xFFFF);
}

/*=============================================================================
 *  Build 32-bit TP bitmap from two 16-bit raw readings
 *
 *  Chip 1: raw[15]=TP0, raw[0]=TP15  →  TPi = bit (15-i)
 *  Chip 2: raw[15]=TP16, raw[0]=TP31 →  TP(16+i) = bit (15-i)
 *===========================================================================*/

static uint32_t build_tp_bitmap(uint16_t raw1, uint16_t raw2)
{
    /* Apply calibration: touched = (~raw) & baseline */
    uint16_t touched1 = (uint16_t)((~raw1) & s_tm.baseline1);
    uint16_t touched2 = (uint16_t)((~raw2) & s_tm.baseline2);

    uint32_t bits = 0;
    for (uint8_t i = 0; i < 16; i++) {
        if (touched1 & (1 << (15 - i))) bits |= (1UL << i);
        if (touched2 & (1 << (15 - i))) bits |= (1UL << (16 + i));
    }
    return bits;
}

/*=============================================================================
 *  Centroid Calculation (fixed-point *10 for sub-grid precision)
 *===========================================================================*/

static bool tm_compute_centroid(uint32_t bits, int16_t *col, int16_t *row)
{
    int32_t sum_col = 0, sum_row = 0, count = 0;

    for (uint8_t i = 0; i < 32; i++) {
        if (bits & (1UL << i)) {
            sum_col += tp_to_col[i];
            sum_row += tp_to_row[i];
            count++;
        }
    }

    if (count == 0) return false;

    *col = (int16_t)((sum_col * 10) / count);
    *row = (int16_t)((sum_row * 10) / count);
    return true;
}

/*=============================================================================
 *  Public API
 *===========================================================================*/

void TouchMatrix_Init(void)
{
    GPIO_InitTypeDef gpio;

    printf("[touch_matrix] init: SCL1=PB8 SDO1=PB9 SCL2=PB10 SDO2=PB11\r\n");

    /* Enable GPIOB clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* SCL pins (PB8, PB10): push-pull output, idle LOW */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin   = TM_SCL1_BIT | TM_SCL2_BIT;
    GPIO_Init(GPIOB, &gpio);
    TM_SCL1_LOW();
    TM_SCL2_LOW();

    /* SDO pins (PB9, PB11): input floating (TTP229 drives SDO) */
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin  = TM_SDO1_BIT | TM_SDO2_BIT;
    GPIO_Init(GPIOB, &gpio);

    /* Initialize state */
    memset(&s_tm, 0, sizeof(s_tm));
    s_tm.sensitivity_x = TM_SENSITIVITY_X;
    s_tm.sensitivity_y = TM_SENSITIVITY_Y;
    s_tm.cursor_x = TM_SCREEN_W / 2;
    s_tm.cursor_y = TM_SCREEN_H / 2;
    s_tm.cursor_visible = true;
    s_tm.state = TM_STATE_IDLE;
    s_tm.touch_id = 0;
    s_tm.baseline1 = 0xFFFF;
    s_tm.baseline2 = 0xFFFF;
    s_tm.cached1 = 0xFFFF;
    s_tm.cached2 = 0xFFFF;
    s_tm.cursor_rendered_x = -1;
    s_tm.cursor_rendered_y = -1;

    printf("[touch_matrix] GPIO configured (SCL idle LOW, SDO floating)\r\n");
    printf("[touch_matrix] waiting for TTP229 power-on stabilization...\r\n");

    /* Wait for TTP229 power-on stabilization (~500ms) */
    Delay_Ms(500);

    /* Calibrate baselines */
    TouchMatrix_Calibrate();

    printf("[touch_matrix] ready, cursor=(%d,%d) sens=(%d,%d)\r\n",
           s_tm.cursor_x, s_tm.cursor_y, s_tm.sensitivity_x, s_tm.sensitivity_y);
}

void TouchMatrix_Calibrate(void)
{
    uint8_t attempt;

    printf("[touch_matrix] calibrating baseline...\r\n");

    /* Discard first few reads to let shift register stabilize */
    for (attempt = 0; attempt < 4; attempt++) {
        s_tm.cached1 = ttp229_read_raw(0, s_tm.cached1);
        s_tm.cached2 = ttp229_read_raw(1, s_tm.cached2);
        Delay_Ms(40);  /* Wait for TTP229 scan cycle (~32ms) */
    }

    /* Capture baseline from final reads */
    s_tm.baseline1 = ttp229_read_raw(0, s_tm.cached1);
    Delay_Ms(40);
    s_tm.baseline2 = ttp229_read_raw(1, s_tm.cached2);
    Delay_Ms(40);

    /* Second read for stability */
    s_tm.baseline1 = ttp229_read_raw(0, s_tm.baseline1);
    s_tm.baseline2 = ttp229_read_raw(1, s_tm.baseline2);

    s_tm.cached1 = s_tm.baseline1;
    s_tm.cached2 = s_tm.baseline2;

    printf("[touch_matrix] baseline: chip1=0x%04X chip2=0x%04X\r\n",
           s_tm.baseline1, s_tm.baseline2);
}

void TouchMatrix_SetSensitivity(int16_t sx, int16_t sy)
{
    if (sx > 0) s_tm.sensitivity_x = sx;
    if (sy > 0) s_tm.sensitivity_y = sy;
}

int16_t  TouchMatrix_GetCursorX(void)     { return s_tm.cursor_x; }
int16_t  TouchMatrix_GetCursorY(void)     { return s_tm.cursor_y; }
bool     TouchMatrix_IsTouched(void)      { return s_tm.state != TM_STATE_IDLE; }
bool     TouchMatrix_IsCursorVisible(void){ return s_tm.cursor_visible; }
uint32_t TouchMatrix_GetRawState(void)    { return s_tm.raw_bits; }

/*=============================================================================
 *  Cursor Dirty Region Management
 *===========================================================================*/

#define TM_CURSOR_W  10   /* 8px bitmap + 1px outline each side */
#define TM_CURSOR_H  13

void TouchMatrix_InvalidateCursor(void)
{
    if (!s_tm.cursor_visible) return;

    /* Skip if cursor hasn't moved since last render */
    if (s_tm.cursor_rendered_x >= 0 &&
        s_tm.cursor_rendered_x == s_tm.cursor_x &&
        s_tm.cursor_rendered_y == s_tm.cursor_y) {
        return;  /* No change, no invalidation needed */
    }

    ui_rect_t r;
    r.w = TM_CURSOR_W;
    r.h = TM_CURSOR_H;

    /* Invalidate old rendered position (to erase) */
    if (s_tm.cursor_rendered_x >= 0) {
        r.x = s_tm.cursor_rendered_x - 1;
        r.y = s_tm.cursor_rendered_y - 1;
        ui_page_invalidate(&r);
    }

    /* Invalidate current position (to draw) */
    r.x = s_tm.cursor_x - 1;
    r.y = s_tm.cursor_y - 1;
    ui_page_invalidate(&r);
}

void TouchMatrix_CursorRendered(void)
{
    s_tm.cursor_rendered_x = s_tm.cursor_x;
    s_tm.cursor_rendered_y = s_tm.cursor_y;
}

/*=============================================================================
 *  Scan — called periodically from main loop (~100Hz)
 *===========================================================================*/

void TouchMatrix_Scan(void)
{
    /* Read both TTP229 chips */
    s_tm.cached1 = ttp229_read_raw(0, s_tm.cached1);
    s_tm.cached2 = ttp229_read_raw(1, s_tm.cached2);

    /* Build 32-bit touched bitmap (bit i = TPi touched) */
    uint32_t bits = build_tp_bitmap(s_tm.cached1, s_tm.cached2);
    s_tm.raw_bits = bits;

    /* Compute centroid of active pads */
    int16_t col_x10 = 0, row_x10 = 0;
    bool touched = tm_compute_centroid(bits, &col_x10, &row_x10);

    uint32_t now = ui_get_real_ms();

    switch (s_tm.state) {
    case TM_STATE_IDLE:
        if (touched) {
            /* First touch — record starting position.
             * DON'T inject TOUCH_DOWN here: this prevents widget capture
             * on swipe. We only inject on UP if it was a tap. */
            s_tm.last_col_x10 = col_x10;
            s_tm.last_row_x10 = row_x10;
            s_tm.has_last_pos = true;
            s_tm.start_cursor_x = s_tm.cursor_x;
            s_tm.start_cursor_y = s_tm.cursor_y;
            s_tm.state = TM_STATE_TRACKING;

            printf("[tm] DOWN bits=0x%08lX cursor=(%d,%d) col=%d row=%d\r\n",
                   bits, s_tm.cursor_x, s_tm.cursor_y,
                   col_x10 / 10, row_x10 / 10);
        }
        break;

    case TM_STATE_TRACKING:
        if (touched) {
            /* Compute delta from last sample */
            int16_t dcol = col_x10 - s_tm.last_col_x10;
            int16_t drow = row_x10 - s_tm.last_row_x10;

            s_tm.last_col_x10 = col_x10;
            s_tm.last_row_x10 = row_x10;

            /* Convert to screen pixel displacement (dcol is *10 grid units) */
            int16_t dx = (int16_t)((int32_t)dcol * s_tm.sensitivity_x / 10);
            int16_t dy = (int16_t)((int32_t)drow * s_tm.sensitivity_y / 10);

            if (dx != 0 || dy != 0) {
                /* Update cursor with clamping.
                 * DON'T inject TOUCH_MOVE — the cursor is rendered via
                 * TouchMatrix_InvalidateCursor() in the main loop.
                 * This ensures swipe only moves the cursor visually,
                 * without triggering widget events. */
                s_tm.cursor_x += dx;
                s_tm.cursor_y += dy;

                if (s_tm.cursor_x < 0) s_tm.cursor_x = 0;
                if (s_tm.cursor_x >= TM_SCREEN_W) s_tm.cursor_x = TM_SCREEN_W - 1;
                if (s_tm.cursor_y < 0) s_tm.cursor_y = 0;
                if (s_tm.cursor_y >= TM_SCREEN_H) s_tm.cursor_y = TM_SCREEN_H - 1;

                /* Throttled debug output (every ~200ms) */
                s_tm.debug_move_count++;
                if (now - s_tm.last_debug_ms >= 200) {
                    printf("[tm] MOVE bits=0x%08lX cursor=(%d,%d) d=%d,%d\r\n",
                           bits, s_tm.cursor_x, s_tm.cursor_y, dx, dy);
                    s_tm.last_debug_ms = now;
                    s_tm.debug_move_count = 0;
                }
            }
        } else {
            /* Touch released — determine tap vs swipe by total cursor movement */
            int16_t total_dx = s_tm.cursor_x - s_tm.start_cursor_x;
            int16_t total_dy = s_tm.cursor_y - s_tm.start_cursor_y;
            int16_t abs_dx = (total_dx < 0) ? -total_dx : total_dx;
            int16_t abs_dy = (total_dy < 0) ? -total_dy : total_dy;
            int16_t max_move = (abs_dx > abs_dy) ? abs_dx : abs_dy;

            printf("[tm] UP bits=0x%08lX cursor=(%d,%d) move=(%d,%d)\r\n",
                   bits, s_tm.cursor_x, s_tm.cursor_y, total_dx, total_dy);

            if (max_move < TM_TAP_THRESHOLD) {
                /* Tap: inject TOUCH_DOWN + TOUCH_UP at cursor position.
                 * The gesture synthesizer will produce a CLICK event
                 * (dx=0, dy=0 since both use the same position). */
                ui_input_inject_touch(s_tm.touch_id,
                                      s_tm.cursor_x, s_tm.cursor_y, 1);
                ui_input_inject_touch(s_tm.touch_id,
                                      s_tm.cursor_x, s_tm.cursor_y, 0);
            }
            /* else: swipe — cursor already moved visually, no widget event */

            s_tm.state = TM_STATE_IDLE;
            s_tm.has_last_pos = false;
        }
        break;
    }
}
