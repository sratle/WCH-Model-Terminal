/********************************** (C) COPYRIGHT *******************************
* File Name          : epaper.c
* Author             : E-ink Model Team
* Description        : JD79686AB 648x480 B/W e-paper driver.
*
* Simplified init based on vendor reference code:
*   - EPD_HW_RESET() + EPD_READBUSY()
*   - No register/LUT writes needed; panel uses power-on defaults.
*
* Pixel format (panel side): 1bpp, MSB = leftmost pixel, bit=1 = white,
* bit=0 = black. The MiniUI renderer produces bit=1 = black, so we invert
* data before sending it to the panel.
********************************************************************************/
#include "epaper.h"

/*============================================================================
 *  Internal helpers
 *==========================================================================*/

static void Epaper_SendDataByte(uint8_t data)
{
    Epaper_Hw_WriteData(data);
}

static void Epaper_SendDataBuf(const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        Epaper_Hw_WriteData(buf[i]);
    }
}

/*============================================================================
 *  Public API
 *==========================================================================*/

/*********************************************************************
 * @fn      Epaper_Init
 *
 * @brief   Hardware reset and wait for BUSY to go high.
 *          The panel uses its power-on default configuration.
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

    Epaper_Hw_WriteCmd(0x13);
    for (j = 0; j < EPD_HEIGHT; j++)
        for (i = 0; i < EPD_ROW_BYTES; i++)
            Epaper_Hw_WriteData(0xFF);    /* new = white (panel: 0xFF) */
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

    Epaper_Hw_WriteCmd(0x13);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~image[i]);
}

/*********************************************************************
 * @fn      Epaper_Update
 *
 * @brief   PON -> WaitBusy -> DRF -> WaitBusy.
 *          Reference code does NOT send POF (0x02).
 */
void Epaper_Update(void)
{
    int ret;
    printf("[epd] update: pon\r\n");
    Epaper_Hw_WriteCmd(0x04);
    ret = Epaper_Hw_WaitBusy(5000);
    printf("[epd] update: pon wait=%d\r\n", ret);

    printf("[epd] update: drf\r\n");
    Epaper_Hw_WriteCmd(0x12);
    ret = Epaper_Hw_WaitBusy(10000);
    printf("[epd] update: drf wait=%d, done\r\n", ret);
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
    Epaper_SendDataBuf(old_data, data_len);

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
    Epaper_SendDataBuf(new_data, data_len);

    /* PDRF: trigger partial refresh with DFV_EN=1 */
    Epaper_Hw_WriteCmd(0x16);
    Epaper_Hw_WriteData(((1 << 7) | ((x >> 8) & 0x03)));
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
 *          Invert both old and new data to match panel mapping.
 */
void Epaper_DisplayImageDiff(const uint8_t *old_image, const uint8_t *new_image)
{
    uint32_t i;

    printf("[epd] diff: dtm1 start\r\n");
    Epaper_Hw_WriteCmd(0x10);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~old_image[i]);
    printf("[epd] diff: dtm2 start\r\n");
    Epaper_Hw_WriteCmd(0x13);
    for (i = 0; i < EPD_FRAME_SIZE; i++)
        Epaper_Hw_WriteData(~new_image[i]);
    printf("[epd] diff: done\r\n");
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
