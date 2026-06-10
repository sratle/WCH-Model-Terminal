/**
 * @file    epaper.c
 * @brief   JD79686AB 648×480 B/W e-paper driver.
 *
 * Based on Arduino OKRA0583BNF686F0 sample code (After OTP Model).
 * Uses OTP LUT (REG_EN=0), no register LUT tables needed.
 */
#include "epaper.h"

/*============================================================================
 *  Internal helpers
 *==========================================================================*/

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
 * @brief   Init per Arduino OKRA0583BNF686F0 sample code:
 *          1. HW Reset + WaitBusy
 *          2. PSR (0xDF: OTP LUT, B/W mode)
 *          3. TRES (648x480)
 *          4. TSGS
 *          5. TDY commands (matching Arduino sequence)
 */
void Epaper_Init(void)
{
    Epaper_Hw_Init();
    Epaper_Hw_Reset();
    Epaper_Hw_WaitBusy(5000);

    /* PSR: B/W mode, use OTP LUT, scan up, shift right */
    Epaper_Hw_WriteCmd(0x00);
    Epaper_Hw_WriteData(0xDF);
    /* 0xDF = 1101_1111:
       RES[1:0]=11(default 800x600, overridden by 0x61),
       REG_EN=0(use OTP LUT),
       BWR=1(B/W mode),
       UD=1, SHL=1, SHD_N=1, RST_N=1 */

    /* TRES: resolution 648×480 */
    Epaper_Hw_WriteCmd(0x61);
    Epaper_Hw_WriteData(0x02);   /* HRES[9:8] */
    Epaper_Hw_WriteData(0x88);   /* HRES[7:3], low 3 bits=000 → 648 */
    Epaper_Hw_WriteData(0x01);   /* VRES[9:8] */
    Epaper_Hw_WriteData(0xE0);   /* VRES[7:0] → 480 */

    /* TSGS: gate/source setting */
    Epaper_Hw_WriteCmd(0x62);
    Epaper_Hw_WriteData(0x00);
    Epaper_Hw_WriteData(0x10);
    Epaper_Hw_WriteData(0x00);
    Epaper_Hw_WriteData(0x00);

    /* --- TDY commands (matching Arduino sample) --- */
    Epaper_TDY_Write(0x60, 0xA5);   /* TDY key */
    Epaper_TDY_Write(0x89, 0xA5);   /* (Arduino specific) */
    Epaper_TDY_Write(0x76, 0x1F);   /* (Arduino specific) */
    Epaper_TDY_Write(0x7E, 0x31);   /* boost constant on time */
    Epaper_TDY_Write(0xB8, 0x80);   /* (Arduino specific) */
    Epaper_TDY_Write(0x92, 0x00);   /* VGL => HiZ (default) */
    Epaper_TDY_Write(0x87, 0x01);   /* VCOM(2frame off) => GND */
    Epaper_TDY_Write(0x88, 0x06);   /* Power off Vcom discharge GND */
    Epaper_TDY_Write(0xA8, 0x30);   /* r_partial_all_gate_en */
    Epaper_TDY_Write(0xE8, 0x88);   /* (Arduino specific) */

    printf("EPD: Init complete (OTP LUT mode)\r\n");
}

/*********************************************************************
 * @fn      Epaper_ClearWhite
 *
 * @brief   Clear to white: DTM1=0xFF, DTM2=0x00
 */
void Epaper_ClearWhite(void)
{
    uint16_t i, j;

    printf("EPD: ClearWhite (DTM1=FF DTM2=00)\r\n");

    Epaper_Hw_WriteCmd(0x10);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0xFF);

    Epaper_Hw_WriteCmd(0x13);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0x00);

    printf("EPD: ClearWhite data sent\r\n");
}

/*********************************************************************
 * @fn      Epaper_DisplayImage
 *
 * @brief   Display image data (same data for both DTM1 and DTM2).
 *          Matches Arduino DisplayFrame() which sends same buffer twice.
 */
void Epaper_DisplayImage(const uint8_t *image)
{
    printf("EPD: DisplayImage\r\n");

    /* DTM1 */
    Epaper_Hw_WriteCmd(0x10);
    Epaper_Hw_WriteDataBuf(image, EPD_FRAME_SIZE);
    Epaper_Hw_WaitBusy(5000);

    /* DTM2 */
    Epaper_Hw_WriteCmd(0x13);
    Epaper_Hw_WriteDataBuf(image, EPD_FRAME_SIZE);

    printf("EPD: DisplayImage data sent\r\n");
}

/*********************************************************************
 * @fn      Epaper_Update
 *
 * @brief   PON → WaitBusy → DRF → WaitBusy → POF → WaitBusy
 *          Matches Arduino Update() sequence.
 */
void Epaper_Update(void)
{
    printf("EPD: PON...\r\n");
    Epaper_Hw_WriteCmd(0x04);
    Delay_Ms(1);
    Epaper_Hw_WaitBusy(60000);

    printf("EPD: DRF...\r\n");
    Epaper_Hw_WriteCmd(0x12);
    Epaper_Hw_WaitBusy(60000);

    printf("EPD: POF...\r\n");
    Epaper_Hw_WriteCmd(0x02);
    Epaper_Hw_WaitBusy(10000);

    printf("EPD: Update done\r\n");
}

/*********************************************************************
 * @fn      Epaper_Sleep
 *
 * @brief   DSLP (deep sleep). Per datasheet, needs PON before DSLP.
 */
void Epaper_Sleep(void)
{
    printf("EPD: PON + DSLP\r\n");
    Epaper_Hw_WriteCmd(0x04);
    Epaper_Hw_WaitBusy(10000);

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
    printf("EPD: Wake up\r\n");
    Epaper_Init();
}
