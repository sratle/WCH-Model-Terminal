/**
 * @file    epaper.c
 * @brief   JD79686AB 648×480 B/W e-paper driver.
 *
 * STRICTLY based on manufacturer STM32 reference code (中景园电子).
 * Module has OTP pre-programmed LUT – NO register writes needed.
 *
 * Flow:
 *   Init:       HW Reset → WaitBusy
 *   Display:    DTM1(old) → DTM2(new) → PON → WaitBusy → DRF → WaitBusy
 *   DeepSleep:  POF → WaitBusy → DSLP(0xA5)
 */
#include "epaper.h"

/*********************************************************************
 * @fn      Epaper_Init
 *
 * @brief   HW Reset + WaitBusy. Matches manufacturer EPD_Init().
 */
void Epaper_Init(void)
{
    Epaper_Hw_Init();
    Epaper_Hw_Reset();
    Epaper_Hw_WaitBusy(5000);

    printf("EPD: Init complete (OTP LUT, no registers)\r\n");
}

/*********************************************************************
 * @fn      Epaper_DisplayFull
 *
 * @brief   Write image data to DTM1 + DTM2.
 *          Matches manufacturer EPD_Display(imageBW, NULL, 0):
 *            DTM1 = imageBW (old data)
 *            DTM2 = 0x00    (new data, "red" channel = black)
 *
 *          To clear white: pass NULL → DTM1=0xFF, DTM2=0x00.
 */
void Epaper_DisplayFull(const uint8_t *image)
{
    uint16_t i, j;

    printf("EPD: DisplayFull start\r\n");

    /* DTM1 (0x10): OLD data */
    Epaper_Hw_WriteCmd(0x10);
    for (j = 0; j < EPD_HEIGHT; j++)
    {
        for (i = 0; i < EPD_ROW_BYTES; i++)
        {
            if (image != NULL)
                Epaper_Hw_WriteData(image[i + j * EPD_ROW_BYTES]);
            else
                Epaper_Hw_WriteData(0xFF);  /* old = white */
        }
    }

    /* DTM2 (0x13): NEW data = 0x00 (per manufacturer convention) */
    Epaper_Hw_WriteCmd(0x13);
    for (j = 0; j < EPD_HEIGHT; j++)
    {
        for (i = 0; i < EPD_ROW_BYTES; i++)
        {
            Epaper_Hw_WriteData(0x00);
        }
    }

    printf("EPD: DisplayFull data sent\r\n");
}

/*********************************************************************
 * @fn      Epaper_ClearWhite
 *
 * @brief   Clear screen to white.
 *          Matches manufacturer EPD_Display_Clear():
 *            DTM1 = 0xFF (old = white)
 *            DTM2 = 0x00 (new = black/white depending on LUT)
 */
void Epaper_ClearWhite(void)
{
    uint16_t i, j;

    printf("EPD: Clear white\r\n");

    Epaper_Hw_WriteCmd(0x10);
    for (j = 0; j < EPD_HEIGHT; j++)
    {
        for (i = 0; i < EPD_ROW_BYTES; i++)
        {
            Epaper_Hw_WriteData(0xFF);
        }
    }

    Epaper_Hw_WriteCmd(0x13);
    for (j = 0; j < EPD_HEIGHT; j++)
    {
        for (i = 0; i < EPD_ROW_BYTES; i++)
        {
            Epaper_Hw_WriteData(0x00);
        }
    }

    printf("EPD: Clear white data sent\r\n");
}

/*********************************************************************
 * @fn      Epaper_Update
 *
 * @brief   Power ON + Refresh. Matches manufacturer EPD_Update():
 *            PON(0x04) → WaitBusy → DRF(0x12) → WaitBusy
 */
void Epaper_Update(void)
{
    printf("EPD: Update (PON + DRF)...\r\n");

    Epaper_Hw_WriteCmd(0x04);  /* Power ON */
    Delay_Ms(10);               /* Give IC time to pull BUSY_N low */
    Epaper_Hw_WaitBusy(5000);

    Epaper_Hw_WriteCmd(0x12);  /* Display Refresh */
    Delay_Ms(10);               /* Give IC time to pull BUSY_N low */
    Epaper_Hw_WaitBusy(30000); /* Full refresh ~4 seconds */

    printf("EPD: Update done\r\n");
}

/*********************************************************************
 * @fn      Epaper_Sleep
 *
 * @brief   Power OFF + Deep Sleep. Matches manufacturer EPD_DeepSleep():
 *            POF(0x02) → WaitBusy → DSLP(0x07, 0xA5)
 */
void Epaper_Sleep(void)
{
    printf("EPD: Deep sleep\r\n");

    Epaper_Hw_WriteCmd(0x02);  /* Power OFF */
    Delay_Ms(10);               /* Give IC time to pull BUSY_N low */
    Epaper_Hw_WaitBusy(5000);

    Epaper_Hw_WriteCmd(0x07);  /* Deep Sleep */
    Epaper_Hw_WriteData(0xA5); /* Check code */
}

/*********************************************************************
 * @fn      Epaper_WakeUp
 *
 * @brief   Wake from deep sleep via HW reset + re-init.
 */
void Epaper_WakeUp(void)
{
    printf("EPD: Wake up\r\n");
    Epaper_Hw_Reset();
    Epaper_Hw_WaitBusy(5000);
}
