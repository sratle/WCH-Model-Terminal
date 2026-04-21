/********************************** (C) COPYRIGHT *******************************
* File Name          : ssd1963.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : SSD1963 display controller driver.
*                      800x480 RGB888 LCD, MCU 16-bit RGB565 input via FMC 8080.
*                      10MHz crystal, PLL -> 100MHz system clock.
*                      PCLK -> 33.26MHz for 800x480@60Hz DE mode.
********************************************************************************/
#include "ssd1963.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "debug.h"

/* Note: Uses system Delay_Ms() / Delay_Us() from debug.c */
/* Delay_Init() must be called before using SSD1963_Init() */

/*=============================================================================
 *  Reset Functions
 *=============================================================================*/

/**
 * @brief  Hardware reset SSD1963 via PA0.
 *         RST low >= 10ms, then high >= 5ms.
 */
void SSD1963_HardReset(void)
{
    GPIO_InitTypeDef gpio_init;

    /* Enable GPIOA clock and configure PA0 as push-pull output */
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);
    gpio_init.GPIO_Pin = SSD1963_RST_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(SSD1963_RST_PORT, &gpio_init);

    /* Pull RST low for >= 10ms */
    GPIO_ResetBits(SSD1963_RST_PORT, SSD1963_RST_PIN);
    Delay_Ms(10);

    /* Pull RST high and wait >= 5ms */
    GPIO_SetBits(SSD1963_RST_PORT, SSD1963_RST_PIN);
    Delay_Ms(5);
}

/**
 * @brief  Software reset SSD1963.
 *         Command 0x01, wait >= 5ms after.
 */
void SSD1963_SoftReset(void)
{
    SSD1963_WriteCmd(SSD1963_SOFT_RESET);
    Delay_Ms(5);
}

/*=============================================================================
 *  GRAM Access Functions
 *=============================================================================*/

/**
 * @brief  Write multiple 16-bit pixels to SSD1963 GRAM.
 */
void SSD1963_WriteBuffer(const uint16_t *buf, uint32_t len)
{
    while (len--)
    {
        SSD1963_DATA_ADDR = *buf++;
    }
}

/**
 * @brief  Set display window (GRAM write area).
 *         Uses SSD1963 column (0x2A) and page (0x2B) address commands.
 */
void SSD1963_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    /* Set column address: 0x2A */
    SSD1963_WriteCmd(SSD1963_SET_COLUMN_ADDRESS);
    SSD1963_WriteData(x0 >> 8);
    SSD1963_WriteData(x0 & 0xFF);
    SSD1963_WriteData(x1 >> 8);
    SSD1963_WriteData(x1 & 0xFF);

    /* Set page address: 0x2B */
    SSD1963_WriteCmd(SSD1963_SET_PAGE_ADDRESS);
    SSD1963_WriteData(y0 >> 8);
    SSD1963_WriteData(y0 & 0xFF);
    SSD1963_WriteData(y1 >> 8);
    SSD1963_WriteData(y1 & 0xFF);

    /* Prepare to write GRAM: 0x2C */
    SSD1963_WriteCmd(SSD1963_WRITE_MEMORY_START);
}

/**
 * @brief  Fill entire screen with a single RGB565 color.
 */
void SSD1963_Clear(uint16_t color)
{
    uint32_t pixel_num = LCD_WIDTH * LCD_HEIGHT;

    SSD1963_SetWindow(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    while (pixel_num--)
    {
        SSD1963_DATA_ADDR = color;
    }
}

/**
 * @brief  Draw a single pixel at specified coordinates.
 */
void SSD1963_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    SSD1963_SetWindow(x, y, x, y);
    SSD1963_DATA_ADDR = color;
}

/*=============================================================================
 *  Backlight PWM Control
 *=============================================================================*/

/**
 * @brief  Configure SSD1963 internal PWM for backlight.
 * @param  duty: 0~255 (0=off, 255=100%)
 */
void SSD1963_SetBacklight(uint8_t duty)
{
    /* Command 0xBE: PWM configuration */
    SSD1963_WriteCmd(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x0E);    /* PWM frequency setting */
    SSD1963_WriteData(duty);    /* PWM duty cycle */
    SSD1963_WriteData(0x09);    /* PWM enabled, controlled by host */
    SSD1963_WriteData(0xFF);    /* Manual brightness max */
    SSD1963_WriteData(0x00);    /* Minimum brightness 0 */
    SSD1963_WriteData(0x00);    /* Gradation disabled */
}

/*=============================================================================
 *  Initialization Sequence
 *=============================================================================*/

/**
 * @brief  Complete SSD1963 initialization for 800x480@60Hz DE mode.
 *
 *  Sequence (from datasheet):
 *  1. Hardware reset (RST low 10ms, high 5ms)
 *  2. Software reset (0x01, wait 5ms)
 *  3. Configure PLL M/N: 0xE2, M=29(0x1D), N=2(0x02), VCO=300MHz
 *  4. Enable PLL: 0xE0, param 0x01, wait 100us
 *  5. Lock PLL / switch sysclk: 0xE0, param 0x03, wait 10ms
 *  6. Set pixel data interface: 0xF0, 16-bit RGB565 (0x03)
 *  7. Set LCD mode: 0xB0, 24-bit RGB888 panel, TFT, DE mode
 *  8. Set horizontal period: 0xB4
 *  9. Set vertical period: 0xB6
 *  10. Set pixel clock (LSHIFT): 0xE6, 33.26MHz
 *  11. Set address mode: 0x36, normal scan
 *  12. Enable TE: 0x35
 *  13. Exit sleep: 0x11, wait 5ms
 *  14. Display on: 0x29
 *  15. Optional: PWM backlight
 */
void SSD1963_Init(void)
{
    /* Step 1: Hardware reset */
    SSD1963_HardReset();

    /* Step 2: Software reset */
    SSD1963_SoftReset();

    /* Step 3: Configure PLL M/N
     * VCO = 10MHz * (M+1) = 10 * 30 = 300MHz
     * SysClk = VCO / (N+1) = 300 / 3 = 100MHz
     * Parameter 3 (0x04) is VCO range, see datasheet
     */
    SSD1963_WriteCmd(SSD1963_SET_PLL_MN);
    SSD1963_WriteData(0x1D);    /* M = 29 */
    SSD1963_WriteData(0x02);    /* N = 2 */
    SSD1963_WriteData(0x04);    /* VCO range / PLL divider */

    /* Step 4: Enable PLL */
    SSD1963_WriteCmd(SSD1963_SET_PLL);
    SSD1963_WriteData(0x01);
    Delay_Us(100);

    /* Step 5: Lock PLL, switch system clock to PLL output */
    SSD1963_WriteCmd(SSD1963_SET_PLL);
    SSD1963_WriteData(0x03);
    Delay_Ms(10);

    /* Step 6: Set pixel data interface to 16-bit RGB565 */
    SSD1963_WriteCmd(SSD1963_SET_PIXEL_DATA_INTERFACE);
    SSD1963_WriteData(0x03);    /* 16-bit RGB565 */

    /* Step 7: Set LCD mode
     * Param 0: 0x24
     *   [7:6] = 00: 24-bit panel (RGB888 output to LCD)
     *   [5]   = 1:  LSHIFT polarity: data latched on rising edge
     *   [4]   = 0:  HSYNC polarity: low active
     *   [3]   = 0:  VSYNC polarity: low active
     *   [2:0] = 100: DE mode
     * Param 1: 0x20 - TFT mode
     * Param 2: 0x03 - HDP[10:8] = 3 (HDP = 799 = 0x31F)
     * Param 3: 0x1F - HDP[7:0] = 0x1F
     * Param 4: 0x01 - VDP[10:8] = 1 (VDP = 479 = 0x1DF)
     * Param 5: 0xDF - VDP[7:0] = 0xDF
     * Param 6: 0x00 - RGB order normal
     */
    SSD1963_WriteCmd(SSD1963_SET_LCD_MODE);
    SSD1963_WriteData(0x24);    /* 24-bit panel, rising edge, DE mode */
    SSD1963_WriteData(0x20);    /* TFT mode */
    SSD1963_WriteData(0x03);    /* HDP[10:8] = 3  -> HDP = 799 */
    SSD1963_WriteData(0x1F);    /* HDP[7:0]  = 0x1F */
    SSD1963_WriteData(0x01);    /* VDP[10:8] = 1  -> VDP = 479 */
    SSD1963_WriteData(0xDF);    /* VDP[7:0]  = 0xDF */
    SSD1963_WriteData(0x00);    /* RGB order: RGB */

    /* Step 8: Set horizontal period
     * HT = 1056 (0x420), HPS = 47 (0x2F), HPW = 1, LPS = 47, LPSPP = 0
     */
    SSD1963_WriteCmd(SSD1963_SET_HORI_PERIOD);
    SSD1963_WriteData(0x04);    /* HT[10:8]  = 4  -> HT = 1056 */
    SSD1963_WriteData(0x20);    /* HT[7:0]   = 0x20 */
    SSD1963_WriteData(0x00);    /* HPS[10:8] = 0  -> HPS = 47 */
    SSD1963_WriteData(0x2F);    /* HPS[7:0]  = 0x2F */
    SSD1963_WriteData(0x01);    /* HPW = 1 */
    SSD1963_WriteData(0x00);    /* LPS[10:8] = 0  -> LPS = 47 */
    SSD1963_WriteData(0x2F);    /* LPS[7:0]  = 0x2F */
    SSD1963_WriteData(0x00);    /* LPSPP = 0 */

    /* Step 9: Set vertical period
     * VT = 527 (0x20F), VPS = 35 (0x23), VPW = 3, FPS = 35
     */
    SSD1963_WriteCmd(SSD1963_SET_VERT_PERIOD);
    SSD1963_WriteData(0x02);    /* VT[10:8]  = 2  -> VT = 527 */
    SSD1963_WriteData(0x0F);    /* VT[7:0]   = 0x0F */
    SSD1963_WriteData(0x00);    /* VPS[10:8] = 0  -> VPS = 35 */
    SSD1963_WriteData(0x23);    /* VPS[7:0]  = 0x23 */
    SSD1963_WriteData(0x03);    /* VPW = 3 */
    SSD1963_WriteData(0x00);    /* FPS[10:8] = 0  -> FPS = 35 */
    SSD1963_WriteData(0x23);    /* FPS[7:0]  = 0x23 */

    /* Step 10: Set pixel clock (LSHIFT) frequency
     * PCLK = PLL_SysClk * (LCDC_FPR + 1) / 2^20
     * LCDC_FPR = (33.26M * 1048576 / 100M) - 1 = 348732 = 0x5523C
     * High 4 bits: 0x05, Mid 8 bits: 0x52, Low 8 bits: 0x3C
     */
    SSD1963_WriteCmd(SSD1963_SET_LSHIFT_FREQ);
    SSD1963_WriteData(0x05);    /* LCDC_FPR[19:16] */
    SSD1963_WriteData(0x52);    /* LCDC_FPR[15:8] */
    SSD1963_WriteData(0x3C);    /* LCDC_FPR[7:0] */

    /* Step 11: Set address mode
     * 0x00: Top-to-bottom, left-to-right, normal RGB order
     */
    SSD1963_WriteCmd(SSD1963_SET_ADDRESS_MODE);
    SSD1963_WriteData(0x00);

    /* Step 12: Enable TE (Tearing Effect) output
     * 0x00: TE output enabled during vertical blanking only
     */
    SSD1963_WriteCmd(SSD1963_SET_TEAR_ON);
    SSD1963_WriteData(0x00);

    /* Step 13: Exit sleep mode */
    SSD1963_WriteCmd(SSD1963_EXIT_SLEEP_MODE);
    Delay_Ms(5);

    /* Step 14: Turn on display */
    SSD1963_WriteCmd(SSD1963_SET_DISPLAY_ON);

    /* Step 15: Configure PWM backlight (100% brightness by default) */
    SSD1963_SetBacklight(0xFF);
}
