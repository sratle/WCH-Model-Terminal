/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_lcd.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : ST7789P3 TFT LCD driver implementation.
*                      4-wire SPI on SPI1, no framebuffer: every drawing
*                      primitive opens an address window and streams pixels
*                      straight into the controller GRAM.
********************************************************************************/
#include "bsp_lcd.h"
#include "ch32v30x.h"
#include "debug.h"              /* Delay_Ms / Delay_Us */

/*=============================================================================
 *  Pin Mapping (see docs/devboard/HARDWARE.md)
 *=============================================================================*/

#define LCD_DC_PORT     GPIOA
#define LCD_DC_PIN      GPIO_Pin_3      /* PA3  - data/command select    */
#define LCD_CS_PORT     GPIOA
#define LCD_CS_PIN      GPIO_Pin_4      /* PA4  - chip select (manual)   */
#define LCD_SCK_PORT    GPIOA
#define LCD_SCK_PIN     GPIO_Pin_5      /* PA5  - SPI1_SCK               */
#define LCD_MOSI_PORT   GPIOA
#define LCD_MOSI_PIN    GPIO_Pin_7      /* PA7  - SPI1_MOSI              */
#define LCD_RES_PORT    GPIOA
#define LCD_RES_PIN     GPIO_Pin_8      /* PA8  - controller reset       */

/* Fast pin control helpers */
#define LCD_CS_LOW()    GPIO_ResetBits(LCD_CS_PORT,  LCD_CS_PIN)
#define LCD_CS_HIGH()   GPIO_SetBits(LCD_CS_PORT,    LCD_CS_PIN)
#define LCD_DC_CMD()    GPIO_ResetBits(LCD_DC_PORT,  LCD_DC_PIN)
#define LCD_DC_DATA()   GPIO_SetBits(LCD_DC_PORT,    LCD_DC_PIN)
#define LCD_RES_LOW()   GPIO_ResetBits(LCD_RES_PORT, LCD_RES_PIN)
#define LCD_RES_HIGH()  GPIO_SetBits(LCD_RES_PORT,   LCD_RES_PIN)

/*=============================================================================
 *  ST7789 Command Codes (subset used by this driver)
 *=============================================================================*/

#define ST7789_SLPOUT   0x11    /* Sleep out                              */
#define ST7789_NORON    0x13    /* Normal display mode on                 */
#define ST7789_INVOFF   0x20    /* Display inversion off                  */
#define ST7789_INVON    0x21    /* Display inversion on (IPS panels)      */
#define ST7789_DISPOFF  0x28    /* Display off                            */
#define ST7789_DISPON   0x29    /* Display on                             */
#define ST7789_CASET    0x2A    /* Column address set                     */
#define ST7789_RASET    0x2B    /* Row address set                        */
#define ST7789_RAMWR    0x2C    /* Memory write                           */
#define ST7789_MADCTL   0x36    /* Memory data access control             */
#define ST7789_COLMOD   0x3A    /* Interface pixel format                 */
#define ST7789_PORCTRL  0xB2    /* Porch control                          */
#define ST7789_GCTRL    0xB7    /* Gate control                           */
#define ST7789_VCOMS    0xBB    /* VCOM setting                           */
#define ST7789_LCMCTRL  0xC0    /* LCM control                            */
#define ST7789_VDVVRHEN 0xC2    /* VDV and VRH command enable             */
#define ST7789_VRHS     0xC3    /* VRH set                                */
#define ST7789_VDVSET   0xC4    /* VDV setting                            */
#define ST7789_FRCTRL2  0xC6    /* Frame rate control in normal mode      */
#define ST7789_PWCTRL1  0xD0    /* Power control 1                        */
#define ST7789_PVGAMCTRL 0xE0   /* Positive voltage gamma control         */
#define ST7789_NVGAMCTRL 0xE1   /* Negative voltage gamma control         */
#define ST7789_SPI2EN   0xE7    /* SPI 2-data-line mode control           */

/* MADCTL bits */
#define MADCTL_MY       0x80    /* Row address order (bottom to top)      */
#define MADCTL_MX       0x40    /* Column address order (right to left)   */
#define MADCTL_MV       0x20    /* Row/column exchange                    */
#define MADCTL_BGR      0x08    /* BGR color filter panel                 */

/*=============================================================================
 *  Module State
 *=============================================================================*/

/* Current text font (default: Montserrat 16px) */
static const lcd_font_t *s_font = &font_montserrat_16;

/* Forward declaration (defined in the text rendering section) */
static void LCD_DrawGlyph(int16_t x, int16_t y, const lcd_glyph_t *glyph,
                          uint16_t fg);

/*=============================================================================
 *  Low-level SPI Helpers
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_SPI_WriteByte
 *
 * @brief   Transmit one byte over SPI1 (polling, RX discarded).
 *
 * @param   b - byte to send
 *
 * @return  none
 */
static void LCD_SPI_WriteByte(uint8_t b)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) { }
    SPI_I2S_SendData(SPI1, b);
}

/*********************************************************************
 * @fn      LCD_SPI_WaitIdle
 *
 * @brief   Wait until the SPI shift register is empty.
 *
 * @return  none
 */
static void LCD_SPI_WaitIdle(void)
{
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) { }
}

/*********************************************************************
 * @fn      LCD_WriteCmd
 *
 * @brief   Send a command byte (DC low).
 *
 * @param   cmd - ST7789 command code
 *
 * @return  none
 */
static void LCD_WriteCmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_SPI_WriteByte(cmd);
    LCD_SPI_WaitIdle();
}

/*********************************************************************
 * @fn      LCD_WriteData8
 *
 * @brief   Send a single data byte (DC high).
 *
 * @param   data - parameter byte
 *
 * @return  none
 */
static void LCD_WriteData8(uint8_t data)
{
    LCD_DC_DATA();
    LCD_SPI_WriteByte(data);
    LCD_SPI_WaitIdle();
}

/*=============================================================================
 *  Hardware Initialization
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_GPIO_SPI_Init
 *
 * @brief   Configure control pins as push-pull outputs and SPI1 as
 *          8-bit master, mode 0, MSB first (SCK=PA5, MOSI=PA7).
 *
 * @return  none
 */
static void LCD_GPIO_SPI_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef  spi;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO  |
                           RCC_APB2Periph_SPI1, ENABLE);

    /* DC / CS / RES: general purpose push-pull outputs */
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Pin   = LCD_DC_PIN | LCD_CS_PIN | LCD_RES_PIN;
    GPIO_Init(GPIOA, &gpio);

    /* SCK / MOSI: SPI1 alternate function push-pull */
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin   = LCD_SCK_PIN | LCD_MOSI_PIN;
    GPIO_Init(GPIOA, &gpio);

    /* Idle levels: CS high, RES high */
    LCD_CS_HIGH();
    LCD_RES_HIGH();

    /* SPI1: master, 8-bit, CPOL=0 CPHA=0, MSB first, software NSS.
     * Full-duplex mode is used (same as the proven WCH e-paper driver);
     * the LCD never drives MISO, RX bytes are simply discarded. */
    SPI_I2S_DeInit(SPI1);
    spi.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode              = SPI_Mode_Master;
    spi.SPI_DataSize          = SPI_DataSize_8b;
    spi.SPI_CPOL              = SPI_CPOL_Low;
    spi.SPI_CPHA              = SPI_CPHA_1Edge;
    spi.SPI_NSS               = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = LCD_SPI_PRESCALER;
    spi.SPI_FirstBit          = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);
}

/*********************************************************************
 * @fn      LCD_HardwareReset
 *
 * @brief   Pulse the RES line to reset the ST7789P3.
 *
 * @return  none
 */
static void LCD_HardwareReset(void)
{
    LCD_RES_LOW();
    Delay_Ms(500);      /* Vendor reference holds reset low for 1000 ms  */
    LCD_RES_HIGH();
    Delay_Ms(500);      /* and waits 1000 ms before the first command    */
}

/*********************************************************************
 * @fn      LCD_ControllerInit
 *
 * @brief   ST7789P3 register initialization sequence.
 *          Power / gamma values follow the module vendor's verified
 *          reference code (hardware/devboard-lcd参考代码/STM32/2.0TFTspi.C);
 *          pixel format is changed to 16-bit RGB565 for this driver.
 *
 * @return  none
 */
static void LCD_ControllerInit(void)
{
    uint8_t madctl;

    LCD_CS_LOW();

    LCD_WriteCmd(ST7789_SLPOUT);        /* Exit sleep mode              */
    Delay_Ms(120);

    /* Porch control (defaults recommended by Sitronix) */
    LCD_WriteCmd(ST7789_PORCTRL);
    LCD_WriteData8(0x0C);
    LCD_WriteData8(0x0C);
    LCD_WriteData8(0x00);
    LCD_WriteData8(0x33);
    LCD_WriteData8(0x33);

    LCD_WriteCmd(ST7789_INVOFF);        /* Inversion off first (vendor) */

    LCD_WriteCmd(ST7789_GCTRL);         /* Gate control: VGH/VGL (0x56  */
    LCD_WriteData8(0x56);               /* per vendor, NOT the 0x35     */
                                        /* datasheet default)           */

    LCD_WriteCmd(ST7789_VCOMS);         /* VCOM setting                 */
    LCD_WriteData8(0x18);

    LCD_WriteCmd(ST7789_LCMCTRL);       /* LCM control                  */
    LCD_WriteData8(0x2C);

    LCD_WriteCmd(ST7789_VDVVRHEN);      /* VDV/VRH command enable       */
    LCD_WriteData8(0x01);

    LCD_WriteCmd(ST7789_VRHS);          /* VRH set (0x1F per vendor)    */
    LCD_WriteData8(0x1F);

    LCD_WriteCmd(ST7789_VDVSET);        /* VDV set                      */
    LCD_WriteData8(0x20);

    LCD_WriteCmd(ST7789_FRCTRL2);       /* Frame rate = 60 Hz           */
    LCD_WriteData8(0x0F);

    LCD_WriteCmd(ST7789_PWCTRL1);       /* Power control 1 (0xA6 vendor)*/
    LCD_WriteData8(0xA6);
    LCD_WriteData8(0xA1);

    /* Gamma curves (vendor values for this panel) */
    LCD_WriteCmd(ST7789_PVGAMCTRL);
    LCD_WriteData8(0xD0); LCD_WriteData8(0x0D); LCD_WriteData8(0x14);
    LCD_WriteData8(0x0B); LCD_WriteData8(0x0B); LCD_WriteData8(0x07);
    LCD_WriteData8(0x3A); LCD_WriteData8(0x44); LCD_WriteData8(0x50);
    LCD_WriteData8(0x08); LCD_WriteData8(0x13); LCD_WriteData8(0x13);
    LCD_WriteData8(0x2D); LCD_WriteData8(0x32);

    LCD_WriteCmd(ST7789_NVGAMCTRL);
    LCD_WriteData8(0xD0); LCD_WriteData8(0x0D); LCD_WriteData8(0x14);
    LCD_WriteData8(0x0B); LCD_WriteData8(0x0B); LCD_WriteData8(0x07);
    LCD_WriteData8(0x3A); LCD_WriteData8(0x44); LCD_WriteData8(0x50);
    LCD_WriteData8(0x08); LCD_WriteData8(0x13); LCD_WriteData8(0x13);
    LCD_WriteData8(0x2D); LCD_WriteData8(0x32);

    /* Memory access control: rotation + color order */
    switch (LCD_ROTATION) {
    case 0:  madctl = 0x00;                    break;   /* Portrait        */
    case 2:  madctl = MADCTL_MX | MADCTL_MY;   break;   /* Portrait inv.   */
    case 3:  madctl = MADCTL_MV | MADCTL_MY;   break;   /* Landscape inv.  */
    case 1:
    default: madctl = MADCTL_MV | MADCTL_MX;   break;   /* Landscape       */
    }
#if LCD_MADCTL_BGR
    madctl |= MADCTL_BGR;
#endif
    LCD_WriteCmd(ST7789_MADCTL);
    LCD_WriteData8(madctl);

    /* Interface pixel format: 16 bit/pixel (RGB565).
     * (The vendor sample uses 0x66 = 18-bit; 0x55 keeps 2 bytes/pixel.) */
    LCD_WriteCmd(ST7789_COLMOD);
    LCD_WriteData8(0x55);

    LCD_WriteCmd(ST7789_SPI2EN);        /* 2-data-line mode off (vendor)*/
    LCD_WriteData8(0x00);

    /* IPS (normally black) panel: inversion must be ON for correct colors */
    LCD_WriteCmd(ST7789_INVON);

    LCD_WriteCmd(ST7789_NORON);         /* Normal display mode          */
    LCD_WriteCmd(ST7789_DISPON);        /* Display on                   */
    Delay_Ms(20);

    LCD_CS_HIGH();
}

/*********************************************************************
 * @fn      LCD_Init
 *
 * @brief   Full LCD initialization: SPI/GPIO, reset, register setup.
 *
 * @return  none
 */
void LCD_Init(void)
{
    LCD_GPIO_SPI_Init();
    LCD_HardwareReset();
    LCD_ControllerInit();
    s_font = &font_montserrat_16;
}

/*=============================================================================
 *  Address Window / Pixel Streaming
 *=============================================================================*/

/*********************************************************************
 * @fn      LCD_SetWindow
 *
 * @brief   Set the GRAM address window and issue the RAMWR command.
 *          After this call, pixel data can be streamed directly.
 *          CS is left LOW; call LCD_EndWrite() when finished.
 *
 * @param   x0,y0 - top-left corner (inclusive)
 * @param   x1,y1 - bottom-right corner (inclusive)
 *
 * @return  none
 */
static void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    x0 += LCD_XSTART;  x1 += LCD_XSTART;
    y0 += LCD_YSTART;  y1 += LCD_YSTART;

    LCD_CS_LOW();

    LCD_WriteCmd(ST7789_CASET);
    LCD_DC_DATA();
    LCD_SPI_WriteByte((uint8_t)(x0 >> 8));
    LCD_SPI_WriteByte((uint8_t)(x0 & 0xFF));
    LCD_SPI_WriteByte((uint8_t)(x1 >> 8));
    LCD_SPI_WriteByte((uint8_t)(x1 & 0xFF));
    LCD_SPI_WaitIdle();

    LCD_WriteCmd(ST7789_RASET);
    LCD_DC_DATA();
    LCD_SPI_WriteByte((uint8_t)(y0 >> 8));
    LCD_SPI_WriteByte((uint8_t)(y0 & 0xFF));
    LCD_SPI_WriteByte((uint8_t)(y1 >> 8));
    LCD_SPI_WriteByte((uint8_t)(y1 & 0xFF));
    LCD_SPI_WaitIdle();

    LCD_WriteCmd(ST7789_RAMWR);
    LCD_DC_DATA();
}

/*********************************************************************
 * @fn      LCD_EndWrite
 *
 * @brief   Finish a pixel streaming session (release CS).
 *
 * @return  none
 */
static void LCD_EndWrite(void)
{
    LCD_SPI_WaitIdle();
    LCD_CS_HIGH();
}

/*********************************************************************
 * @fn      LCD_WriteColor
 *
 * @brief   Write one RGB565 pixel inside an open address window.
 *
 * @param   color - RGB565 pixel value
 *
 * @return  none
 */
static void LCD_WriteColor(uint16_t color)
{
    LCD_SPI_WriteByte((uint8_t)(color >> 8));
    LCD_SPI_WriteByte((uint8_t)(color & 0xFF));
}

/*=============================================================================
 *  Drawing Primitives
 *=============================================================================*/

void LCD_Fill(uint16_t color)
{
    LCD_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t chunk[64];                  /* 32 pixels per burst          */
    uint32_t count;
    uint8_t  i;

    /* Clip to the visible area */
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if ((uint32_t)x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if ((uint32_t)y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w == 0 || h == 0) return;

    /* Prepare a burst buffer with repeated pixel data */
    for (i = 0; i < sizeof(chunk); i += 2) {
        chunk[i]     = hi;
        chunk[i + 1] = lo;
    }

    LCD_SetWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    count = (uint32_t)w * h;
    while (count > 0) {
        uint32_t n = (count > 32) ? 32 : count;     /* pixels this burst */
        uint32_t bytes = n * 2;
        for (i = 0; i < bytes; i++) {
            LCD_SPI_WriteByte(chunk[i]);
        }
        count -= n;
    }

    LCD_EndWrite();
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    LCD_SetWindow(x, y, x, y);
    LCD_WriteColor(color);
    LCD_EndWrite();
}

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0) return;
    LCD_FillRect(x, y, w, 1, color);                    /* Top          */
    LCD_FillRect(x, (uint16_t)(y + h - 1), w, 1, color);/* Bottom       */
    if (h > 2) {
        LCD_FillRect(x, (uint16_t)(y + 1), 1, (uint16_t)(h - 2), color);
        LCD_FillRect((uint16_t)(x + w - 1), (uint16_t)(y + 1),
                     1, (uint16_t)(h - 2), color);
    }
}

void LCD_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx  = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int16_t dy  = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int16_t sx  = (x0 < x1) ? 1 : -1;
    int16_t sy  = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    /* Fast paths for horizontal / vertical lines */
    if (y0 == y1) {
        int16_t xs = (x0 < x1) ? x0 : x1;
        if (xs < 0) xs = 0;
        LCD_FillRect((uint16_t)xs, (uint16_t)y0, (uint16_t)(dx + 1), 1, color);
        return;
    }
    if (x0 == x1) {
        int16_t ys = (y0 < y1) ? y0 : y1;
        if (ys < 0) ys = 0;
        LCD_FillRect((uint16_t)x0, (uint16_t)ys, 1, (uint16_t)(dy + 1), color);
        return;
    }

    for (;;) {
        if (x0 >= 0 && y0 >= 0 && x0 < LCD_WIDTH && y0 < LCD_HEIGHT) {
            LCD_DrawPixel((uint16_t)x0, (uint16_t)y0, color);
        }
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = (int16_t)(2 * err);
        if (e2 > -dy) { err -= dy; x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err += dx; y0 = (int16_t)(y0 + sy); }
    }
}

/*********************************************************************
 * @fn      LCD_HSpan / LCD_VSpan
 *
 * @brief   Draw a clipped horizontal / vertical span (internal helpers
 *          for the filled shapes). Handle negative coordinates safely.
 *
 * @param   x,y  - span start
 * @param   len  - span length in pixels
 * @param   color - RGB565 color
 *
 * @return  none
 */
static void LCD_HSpan(int16_t x, int16_t y, int16_t len, uint16_t color)
{
    if (y < 0 || y >= LCD_HEIGHT || len <= 0) return;
    if (x < 0) { len = (int16_t)(len + x); x = 0; }
    if (x + len > LCD_WIDTH) len = (int16_t)(LCD_WIDTH - x);
    if (len <= 0) return;
    LCD_FillRect((uint16_t)x, (uint16_t)y, (uint16_t)len, 1, color);
}

static void LCD_VSpan(int16_t x, int16_t y, int16_t len, uint16_t color)
{
    if (x < 0 || x >= LCD_WIDTH || len <= 0) return;
    if (y < 0) { len = (int16_t)(len + y); y = 0; }
    if (y + len > LCD_HEIGHT) len = (int16_t)(LCD_HEIGHT - y);
    if (len <= 0) return;
    LCD_FillRect((uint16_t)x, (uint16_t)y, 1, (uint16_t)len, color);
}

/*********************************************************************
 * @fn      LCD_DrawCircleHelper
 *
 * @brief   Plot selected circle octants (internal). corners bitmask:
 *          0x1 = upper-left, 0x2 = upper-right,
 *          0x4 = lower-right, 0x8 = lower-left.
 *
 * @return  none
 */
static void LCD_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r,
                                 uint8_t corners, uint16_t color)
{
    int16_t f     = (int16_t)(1 - r);
    int16_t ddF_x = 1;
    int16_t ddF_y = (int16_t)(-2 * r);
    int16_t x     = 0;
    int16_t y     = r;

    while (x < y) {
        if (f >= 0) { y--; ddF_y = (int16_t)(ddF_y + 2); f = (int16_t)(f + ddF_y); }
        x++; ddF_x = (int16_t)(ddF_x + 2); f = (int16_t)(f + ddF_x);
        if (corners & 0x4) { LCD_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 + y), color);
                             LCD_DrawPixel((uint16_t)(x0 + y), (uint16_t)(y0 + x), color); }
        if (corners & 0x2) { LCD_DrawPixel((uint16_t)(x0 + x), (uint16_t)(y0 - y), color);
                             LCD_DrawPixel((uint16_t)(x0 + y), (uint16_t)(y0 - x), color); }
        if (corners & 0x8) { LCD_DrawPixel((uint16_t)(x0 - y), (uint16_t)(y0 + x), color);
                             LCD_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 + y), color); }
        if (corners & 0x1) { LCD_DrawPixel((uint16_t)(x0 - y), (uint16_t)(y0 - x), color);
                             LCD_DrawPixel((uint16_t)(x0 - x), (uint16_t)(y0 - y), color); }
    }
}

/*********************************************************************
 * @fn      LCD_FillCircleHelper
 *
 * @brief   Fill selected circle sides with vertical spans (internal,
 *          Adafruit-GFX algorithm). corners: 0x1 = right side columns,
 *          0x2 = left side columns; delta extends the column length
 *          (used by LCD_FillRoundRect to cover the straight part).
 *
 * @return  none
 */
static void LCD_FillCircleHelper(int16_t x0, int16_t y0, int16_t r,
                                 uint8_t corners, int16_t delta, uint16_t color)
{
    int16_t f     = (int16_t)(1 - r);
    int16_t ddF_x = 1;
    int16_t ddF_y = (int16_t)(-2 * r);
    int16_t x     = 0;
    int16_t y     = r;

    while (x < y) {
        if (f >= 0) { y--; ddF_y = (int16_t)(ddF_y + 2); f = (int16_t)(f + ddF_y); }
        x++; ddF_x = (int16_t)(ddF_x + 2); f = (int16_t)(f + ddF_x);
        if (corners & 0x1) {    /* Columns to the right of the center  */
            LCD_VSpan((int16_t)(x0 + x), (int16_t)(y0 - y), (int16_t)(2 * y + 1 + delta), color);
            LCD_VSpan((int16_t)(x0 + y), (int16_t)(y0 - x), (int16_t)(2 * x + 1 + delta), color);
        }
        if (corners & 0x2) {    /* Columns to the left of the center   */
            LCD_VSpan((int16_t)(x0 - x), (int16_t)(y0 - y), (int16_t)(2 * y + 1 + delta), color);
            LCD_VSpan((int16_t)(x0 - y), (int16_t)(y0 - x), (int16_t)(2 * x + 1 + delta), color);
        }
    }
}

void LCD_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    if (r <= 0) return;
    LCD_DrawCircleHelper(x0, y0, r, 0x0F, color);
    LCD_DrawPixel((uint16_t)x0, (uint16_t)(y0 + r), color);
    LCD_DrawPixel((uint16_t)x0, (uint16_t)(y0 - r), color);
    LCD_DrawPixel((uint16_t)(x0 + r), (uint16_t)y0, color);
    LCD_DrawPixel((uint16_t)(x0 - r), (uint16_t)y0, color);
}

void LCD_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    if (r <= 0) return;
    LCD_VSpan(x0, (int16_t)(y0 - r), (int16_t)(2 * r + 1), color);
    LCD_FillCircleHelper(x0, y0, r, 0x03, 0, color);
}

void LCD_DrawRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t r, uint16_t color)
{
    int16_t ir = (int16_t)r;

    if (w == 0 || h == 0) return;
    if (r == 0 || 2 * r > w || 2 * r > h) {
        LCD_DrawRect(x, y, w, h, color);
        return;
    }

    /* Straight edges */
    LCD_HSpan((int16_t)(x + ir), (int16_t)y,             (int16_t)(w - 2 * ir), color);
    LCD_HSpan((int16_t)(x + ir), (int16_t)(y + h - 1),   (int16_t)(w - 2 * ir), color);
    LCD_FillRect(x, (uint16_t)(y + ir), 1, (uint16_t)(h - 2 * ir), color);
    LCD_FillRect((uint16_t)(x + w - 1), (uint16_t)(y + ir),
                 1, (uint16_t)(h - 2 * ir), color);

    /* Four corner arcs */
    LCD_DrawCircleHelper((int16_t)(x + ir),         (int16_t)(y + ir),         ir, 0x1, color);
    LCD_DrawCircleHelper((int16_t)(x + w - 1 - ir), (int16_t)(y + ir),         ir, 0x2, color);
    LCD_DrawCircleHelper((int16_t)(x + w - 1 - ir), (int16_t)(y + h - 1 - ir), ir, 0x4, color);
    LCD_DrawCircleHelper((int16_t)(x + ir),         (int16_t)(y + h - 1 - ir), ir, 0x8, color);
}

void LCD_FillRoundRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint16_t r, uint16_t color)
{
    int16_t ir = (int16_t)r;

    if (w == 0 || h == 0) return;
    if (r == 0 || 2 * r > w || 2 * r > h) {
        LCD_FillRect(x, y, w, h, color);
        return;
    }

    /* Center body + side strips between the corner arcs */
    LCD_FillRect((uint16_t)(x + ir), y, (uint16_t)(w - 2 * ir), h, color);
    LCD_FillCircleHelper((int16_t)(x + w - 1 - ir), (int16_t)(y + ir),
                         ir, 0x1, (int16_t)(h - 2 * ir - 1), color);
    LCD_FillCircleHelper((int16_t)(x + ir),         (int16_t)(y + ir),
                         ir, 0x2, (int16_t)(h - 2 * ir - 1), color);
}

void LCD_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color)
{
    LCD_DrawLine(x0, y0, x1, y1, color);
    LCD_DrawLine(x1, y1, x2, y2, color);
    LCD_DrawLine(x2, y2, x0, y0, color);
}

void LCD_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color)
{
    int16_t a, b, y, last;
    int32_t dx01, dx12, dx02;
    int32_t sa, sb;

    /* Sort vertices by y: (x0,y0) top ... (x2,y2) bottom */
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=t; t=x0;x0=x1;x1=t; }
    if (y1 > y2) { int16_t t; t=y1;y1=y2;y2=t; t=x1;x1=x2;x2=t; }
    if (y0 > y1) { int16_t t; t=y0;y0=y1;y1=t; t=x0;x0=x1;x1=t; }

    if (y0 == y2) {     /* Degenerate: flat single row */
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        LCD_HSpan(a, y0, (int16_t)(b - a + 1), color);
        return;
    }

    dx01 = x1 - x0;
    dx12 = x2 - x1;
    dx02 = x2 - x0;
    sa = 0;
    sb = 0;

    /* Upper part: scan from top vertex down to (and including) y1 */
    last = (y1 == y2) ? y1 : (int16_t)(y1 + 1);
    for (y = y0; y <= last; y++) {
        int16_t dy1 = (int16_t)(y1 - y0);  if (dy1 == 0) dy1 = 1;
        int16_t dy2 = (int16_t)(y2 - y0);
        a = (int16_t)(x0 + sa / dy1);
        b = (int16_t)(x0 + sb / dy2);
        sa += dx01;
        sb += dx02;
        LCD_HSpan((a < b) ? a : b, y, (int16_t)((a > b ? a - b : b - a) + 1), color);
    }

    /* Lower part: scan from y1 down to the bottom vertex */
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        int16_t dy1 = (int16_t)(y2 - y1);  if (dy1 == 0) dy1 = 1;
        int16_t dy2 = (int16_t)(y2 - y0);
        a = (int16_t)(x1 + sa / dy1);
        b = (int16_t)(x0 + sb / dy2);
        sa += dx12;
        sb += dx02;
        LCD_HSpan((a < b) ? a : b, y, (int16_t)((a > b ? a - b : b - a) + 1), color);
    }
}

/*=============================================================================
 *  Bitmap / Image
 *=============================================================================*/

void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   const uint16_t *data)
{
    uint16_t row;

    if (!data || w == 0 || h == 0) return;
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if ((uint32_t)x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if ((uint32_t)y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    /* Stream one row per address window so partial lines clip cleanly */
    for (row = 0; row < h; row++) {
        const uint16_t *line = data + (uint32_t)row * w;
        uint16_t col;
        LCD_SetWindow(x, (uint16_t)(y + row),
                      (uint16_t)(x + w - 1), (uint16_t)(y + row));
        for (col = 0; col < w; col++) {
            LCD_WriteColor(line[col]);
        }
        LCD_EndWrite();
    }
}

void LCD_DrawMonoBitmap(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const uint8_t *bmp, uint16_t fg, uint16_t bg)
{
    uint32_t bit = 0;
    uint16_t row, col;

    if (!bmp || w == 0 || h == 0) return;
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if ((uint32_t)x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if ((uint32_t)y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    LCD_SetWindow(x, y, (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));
    for (row = 0; row < h; row++) {
        for (col = 0; col < w; col++, bit++) {
            uint8_t set = bmp[bit >> 3] & (0x80 >> (bit & 7));
            LCD_WriteColor(set ? fg : bg);
        }
    }
    LCD_EndWrite();
}

void LCD_DrawMonoBitmapTR(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                          const uint8_t *bmp, uint16_t fg)
{
    uint32_t bit = 0;
    uint16_t row, col;

    if (!bmp || w == 0 || h == 0) return;

    for (row = 0; row < h; row++) {
        uint16_t py = (uint16_t)(y + row);
        if (py >= LCD_HEIGHT) { bit += w; continue; }
        for (col = 0; col < w; col++, bit++) {
            if (bmp[bit >> 3] & (0x80 >> (bit & 7))) {
                uint16_t px = (uint16_t)(x + col);
                if (px < LCD_WIDTH) LCD_DrawPixel(px, py, fg);
            }
        }
    }
}

/*=============================================================================
 *  Text Rendering
 *=============================================================================*/

const lcd_glyph_t *LCD_FontGetGlyph(const lcd_font_t *font, uint16_t unicode)
{
    int16_t lo, hi;
    if (!font || !font->glyphs) return NULL;

    lo = 0;
    hi = (int16_t)font->glyph_count - 1;
    while (lo <= hi) {
        int16_t mid = (int16_t)((lo + hi) >> 1);
        if (font->glyphs[mid].unicode == unicode) return &font->glyphs[mid];
        if (font->glyphs[mid].unicode < unicode) lo = (int16_t)(mid + 1);
        else                                     hi = (int16_t)(mid - 1);
    }
    return NULL;
}

void LCD_SetFont(const lcd_font_t *font)
{
    s_font = (font != NULL) ? font : &font_montserrat_16;
}

const lcd_font_t *LCD_GetFont(void)
{
    return s_font;
}

uint16_t LCD_FontHeight(void)
{
    return s_font ? s_font->height : 0;
}

uint16_t LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color)
{
    const lcd_glyph_t *glyph;
    if (!s_font) return 0;
    glyph = LCD_FontGetGlyph(s_font, (uint8_t)ch);
    if (!glyph) return 0;
    LCD_DrawGlyph((int16_t)x, (int16_t)(y + s_font->baseline), glyph, color);
    return glyph->advance;
}

uint16_t LCD_DrawCharBG(uint16_t x, uint16_t y, char ch,
                        uint16_t fg, uint16_t bg)
{
    const lcd_glyph_t *glyph;
    if (!s_font) return 0;
    glyph = LCD_FontGetGlyph(s_font, (uint8_t)ch);
    if (!glyph) return 0;
    /* Fill the whole character cell so narrow glyphs leave no ghost */
    LCD_FillRect(x, y, glyph->advance, s_font->height, bg);
    LCD_DrawGlyph((int16_t)x, (int16_t)(y + s_font->baseline), glyph, fg);
    return glyph->advance;
}

/*********************************************************************
 * @fn      LCD_DrawGlyph
 *
 * @brief   Render one glyph, foreground pixels only (transparent
 *          background). The caller is responsible for the background.
 *
 * @param   x,y   - cursor position of the BASELINE start
 * @param   glyph - glyph descriptor
 * @param   fg    - foreground color
 *
 * @return  none
 */
static void LCD_DrawGlyph(int16_t x, int16_t y, const lcd_glyph_t *glyph,
                          uint16_t fg)
{
    int16_t gx = (int16_t)(x + glyph->x_offset);
    int16_t gy = (int16_t)(y + glyph->y_offset);
    uint8_t row, col;
    uint32_t bit = 0;

    if (glyph->width == 0 || glyph->height == 0) return;

    for (row = 0; row < glyph->height; row++) {
        int16_t py = (int16_t)(gy + row);
        if (py < 0 || py >= LCD_HEIGHT) { bit += glyph->width; continue; }
        for (col = 0; col < glyph->width; col++, bit++) {
            if (glyph->bitmap[bit >> 3] & (0x80 >> (bit & 7))) {
                int16_t px = (int16_t)(gx + col);
                if (px >= 0 && px < LCD_WIDTH) {
                    LCD_DrawPixel((uint16_t)px, (uint16_t)py, fg);
                }
            }
        }
    }
}

/*********************************************************************
 * @fn      LCD_DrawTextInternal
 *
 * @brief   Shared text renderer. When opaque is set, each character
 *          CELL (advance width x font height) is filled with bg before
 *          the glyph is drawn - this also clears the inter-glyph gaps
 *          and the padding of narrow glyphs, so no ghosting is left
 *          when overwriting older text.
 *
 * @return  none
 */
static void LCD_DrawTextInternal(uint16_t x, uint16_t y, const char *text,
                                 uint16_t fg, uint16_t bg, uint8_t opaque)
{
    int16_t cursor_x, base_y;

    if (!text || !s_font) return;

    cursor_x = (int16_t)x;
    base_y   = (int16_t)(y + s_font->baseline);

    while (*text) {
        uint8_t ch = (uint8_t)*text++;
        const lcd_glyph_t *glyph = LCD_FontGetGlyph(s_font, ch);
        uint8_t advance = glyph ? glyph->advance
                                : (uint8_t)(s_font->height / 2);
        if (opaque) {
            LCD_FillRect((uint16_t)cursor_x, y, advance,
                         s_font->height, bg);
        }
        if (glyph) {
            LCD_DrawGlyph(cursor_x, base_y, glyph, fg);
        }
        cursor_x = (int16_t)(cursor_x + advance);
    }
}

void LCD_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color)
{
    LCD_DrawTextInternal(x, y, text, color, 0, 0);
}

void LCD_DrawTextBG(uint16_t x, uint16_t y, const char *text,
                    uint16_t fg, uint16_t bg)
{
    LCD_DrawTextInternal(x, y, text, fg, bg, 1);
}

uint16_t LCD_TextWidth(const char *text)
{
    uint16_t w = 0;
    if (!text || !s_font) return 0;

    while (*text) {
        const lcd_glyph_t *glyph = LCD_FontGetGlyph(s_font, (uint8_t)*text++);
        w = (uint16_t)(w + (glyph ? glyph->advance : s_font->height / 2));
    }
    return w;
}

void LCD_DrawTextCenter(uint16_t y, const char *text, uint16_t color)
{
    uint16_t w = LCD_TextWidth(text);
    uint16_t x = (w < LCD_WIDTH) ? (uint16_t)((LCD_WIDTH - w) / 2) : 0;
    LCD_DrawText(x, y, text, color);
}

/*=============================================================================
 *  Display Control
 *=============================================================================*/

void LCD_DisplayOn(void)
{
    LCD_CS_LOW();
    LCD_WriteCmd(ST7789_DISPON);
    LCD_CS_HIGH();
}

void LCD_DisplayOff(void)
{
    LCD_CS_LOW();
    LCD_WriteCmd(ST7789_DISPOFF);
    LCD_CS_HIGH();
}

void LCD_InvertColors(uint8_t enable)
{
    LCD_CS_LOW();
    LCD_WriteCmd(enable ? ST7789_INVON : ST7789_INVOFF);
    LCD_CS_HIGH();
}
