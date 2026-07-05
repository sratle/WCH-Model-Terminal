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
 *          Reads rectangular region data directly from full frame buffers.
 *
 *          Datasheet display flow (section 2.1):
 *            PDTM1(R14h) + PDTM2(R15h)
 *            -> PON(R04h) [wait BUSY]
 *            -> PDRF(R16h) [wait BUSY]
 *            -> POF(R02h) [wait BUSY]
 *
 *          Data is inverted (~) before sending because:
 *            - Frame buffer: bit=1=black, bit=0=white (UI convention)
 *            - Panel:        bit=1=white, bit=0=black (panel convention)
 *
 *          DFV_EN=1 in PDRF: unchanged pixels (old==new) follow VCOM LUT
 *          instead of data LUT, preventing ghosting on static areas.
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

    /* PDTM1: OLD data (inverted to match panel format) */
    Epaper_Hw_WriteCmd(0x14);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &old_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        for (col = 0; col < row_bytes; col++) {
            Epaper_Hw_WriteData(~src[col]);
        }
    }

    /* PDTM2: NEW data (inverted to match panel format) */
    Epaper_Hw_WriteCmd(0x15);
    epaper_send_region_header(x, y, w, h);
    for (row = 0; row < h; row++) {
        const uint8_t *src = &new_frame[(uint32_t)(y + row) * EPD_ROW_BYTES + start_byte];
        for (col = 0; col < row_bytes; col++) {
            Epaper_Hw_WriteData(~src[col]);
        }
    }

    /* PON: Power ON (required before refresh per datasheet) */
    Epaper_Hw_WriteCmd(0x04);
    Epaper_Hw_WaitBusy(5000);

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

    /* POF: Power OFF (required after refresh per datasheet) */
    Epaper_Hw_WriteCmd(0x02);
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
