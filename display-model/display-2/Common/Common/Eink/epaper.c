/**
 * @file    epaper.c
 * @brief   JD79686AB 648×480 B/W e-paper driver.
 *
 * Init sequence per JD79686AB User Guide §2.4.1.2 "After OTP Model".
 * OTP LUT mode (REG_EN=0): no register LUT tables needed.
 *
 * Command order (MANDATORY per §3.2 Note 4):
 *   1. Dummy code  (0x08 = 0x00)
 *   2. TDY key     (0xF8 = 0x60 0xA5)
 *   3. TDY commands(0xF8 = addr data)
 *   4. User commands (PSR, TRES, etc.)
 */
#include "epaper.h"

/*============================================================================
 *  Internal helpers
 *==========================================================================*/

/**
 * @brief  Send a TDY register write: Cmd(0xF8) + Data(addr) + Data(data).
 *         Must be preceded by TDY key (F8 60 A5) at init start.
 */
static void Epaper_TDY_Write(uint8_t addr, uint8_t data)
{
    Epaper_Hw_WriteCmd(0xF8);
    Epaper_Hw_WriteData(addr);
    Epaper_Hw_WriteData(data);
}

/*============================================================================
 *  Public API
 *==========================================================================*/

/*********************************************************************
 * @fn      Epaper_Init
 *
 * @brief   Init per JD79686AB §2.4.1.2 "After OTP Model" + essential
 *          user commands for modules with or without OTP.
 *
 *          Sequence:
 *          1. HW Reset + WaitBusy
 *          2. Dummy code (0x08 = 0x00)          ← MANDATORY first SPI tx
 *          3. TDY key (F8 60 A5)                ← unlocks TDY registers
 *          4. TDY commands (F8 addr data)        ← per datasheet table
 *          5. User commands: PSR, TRES, TSGS     ← resolution & panel cfg
 */
void Epaper_Init(void)
{
    Epaper_Hw_Init();
    Epaper_Hw_Reset();
    Epaper_Hw_WaitBusy(2000);

    /* ---- Step 1: Dummy code (MUST be first SPI transaction after reset) ---- */
    Epaper_Hw_WriteCmd(0x08);
    Epaper_Hw_WriteData(0x00);

    /* ---- Step 2: TDY key (unlocks TDY register access) ---- */
    Epaper_TDY_Write(0x60, 0xA5);   /* TDY key */

    /* ---- Step 3: TDY commands (per §2.4.1.2 After OTP Model) ---- */
    Epaper_TDY_Write(0x93, 0x18);   /* OSC clock keep running */
    Epaper_TDY_Write(0x73, 0x05);   /* VCOM driving capability */
    Epaper_TDY_Write(0x92, 0x08);   /* VGL → GND (T ≥ 10°C) */
    Epaper_TDY_Write(0xA8, 0x3A);   /* r_partial_all_gate_en */
    Epaper_TDY_Write(0x88, 0x06);   /* Power off: VCOM discharge → GND */

    /* ---- Step 4: User commands ---- */

    /* PSR: B/W mode, OTP LUT, scan up, shift right */
    Epaper_Hw_WriteCmd(0x00);
    Epaper_Hw_WriteData(0xDF);
    /* 0xDF = 1101_1111:
       RES[1:0]=11(default 800x600, overridden by TRES 0x61),
       REG_EN=0(use OTP LUT),
       BWR=1(B/W mode),
       UD=1, SHL=1, SHD_N=1, RST_N=1 */

    /* TRES: resolution 648×480 */
    Epaper_Hw_WriteCmd(0x61);
    Epaper_Hw_WriteData(0x02);   /* HRES[9:8] = 10 */
    Epaper_Hw_WriteData(0x88);   /* HRES[7:3] = 10001, low 3 bits=000 → 648 */
    Epaper_Hw_WriteData(0x01);   /* VRES[9:8] = 01 */
    Epaper_Hw_WriteData(0xE0);   /* VRES[7:0] = 1110_0000 → 480 */

    /* TSGS: gate/source start setting (S_Start=0, G_Start=0) */
    Epaper_Hw_WriteCmd(0x62);
    Epaper_Hw_WriteData(0x00);
    Epaper_Hw_WriteData(0x10);
    Epaper_Hw_WriteData(0x00);
    Epaper_Hw_WriteData(0x00);
}

/*********************************************************************
 * @fn      Epaper_ClearWhite
 *
 * @brief   Clear to white: DTM1=0xFF, DTM2=0x00
 */
void Epaper_ClearWhite(void)
{
    uint16_t i, j;

    Epaper_Hw_WriteCmd(0x10);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0xFF);

    Epaper_Hw_WriteCmd(0x13);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0x00);
}

/*********************************************************************
 * @fn      Epaper_DisplayImage
 *
 * @brief   Display image data (same data for both DTM1 and DTM2).
 *          Matches Arduino DisplayFrame() which sends same buffer twice.
 */
void Epaper_DisplayImage(const uint8_t *image)
{
    /* DTM1: OLD data */
    Epaper_Hw_WriteCmd(0x10);
    Epaper_Hw_WriteDataBuf(image, EPD_FRAME_SIZE);

    /* DTM2: NEW data */
    Epaper_Hw_WriteCmd(0x13);
    Epaper_Hw_WriteDataBuf(image, EPD_FRAME_SIZE);
}

/*********************************************************************
 * @fn      Epaper_Update
 *
 * @brief   PON → WaitBusy → DRF → WaitBusy → POF → WaitBusy
 *          Matches Arduino Update() sequence.
 */
void Epaper_Update(void)
{
    Epaper_Hw_WriteCmd(0x04);
    Delay_Ms(1);
    Epaper_Hw_WaitBusy(5000);

    Epaper_Hw_WriteCmd(0x12);
    Epaper_Hw_WaitBusy(10000);

    Epaper_Hw_WriteCmd(0x02);
    Epaper_Hw_WaitBusy(3000);
}

/*********************************************************************
 * @fn      Epaper_PartialRefresh
 *
 * @brief   Partial refresh using PDTM1 + PDTM2 + PDRF.
 *          X and W must be multiples of 8.
 */
void Epaper_PartialRefresh(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *old_data, const uint8_t *new_data)
{
    uint16_t row_bytes = w / 8;
    uint32_t data_len = (uint32_t)row_bytes * h;

    /* PDTM1: OLD data */
    Epaper_Hw_WriteCmd(0x14);
    Epaper_Hw_WriteData((x >> 8) & 0x03);
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);
    Epaper_Hw_WriteDataBuf(old_data, data_len);

    /* PDTM2: NEW data */
    Epaper_Hw_WriteCmd(0x15);
    Epaper_Hw_WriteData((x >> 8) & 0x03);
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);
    Epaper_Hw_WriteDataBuf(new_data, data_len);

    /* PDRF: trigger partial refresh with DFV_EN=1 */
    Epaper_Hw_WriteCmd(0x16);
    Epaper_Hw_WriteData(((1 << 7) | ((x >> 8) & 0x03)));  /* DFV_EN=1 + X[9:8] */
    Epaper_Hw_WriteData(x & 0xF8);
    Epaper_Hw_WriteData((y >> 8) & 0x03);
    Epaper_Hw_WriteData(y & 0xFF);
    Epaper_Hw_WriteData((w >> 8) & 0x03);
    Epaper_Hw_WriteData(w & 0xF8);
    Epaper_Hw_WriteData((h >> 8) & 0x03);
    Epaper_Hw_WriteData(h & 0xFF);

    Epaper_Hw_WaitBusy(3000);
}

/*********************************************************************
 * @fn      Epaper_DisplayImageDiff
 *
 * @brief   Full refresh with separate old/new images.
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image)
{
    /* DTM1: OLD data */
    Epaper_Hw_WriteCmd(0x10);
    Epaper_Hw_WriteDataBuf(old_image, EPD_FRAME_SIZE);

    /* DTM2: NEW data */
    Epaper_Hw_WriteCmd(0x13);
    Epaper_Hw_WriteDataBuf(new_image, EPD_FRAME_SIZE);
}

/*********************************************************************
 * @fn      Epaper_Sleep
 *
 * @brief   DSLP (deep sleep). Per datasheet, needs PON before DSLP.
 */
void Epaper_Sleep(void)
{
    Epaper_Hw_WriteCmd(0x04);
    Epaper_Hw_WaitBusy(5000);

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
