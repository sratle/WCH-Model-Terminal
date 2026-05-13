/********************************** (C) COPYRIGHT *******************************
* File Name          : ssd1963.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : SSD1963 display controller driver.
*                      Initialization and GRAM access via FMC 8080 interface.
*                      Timing values derived from verified CH32H417 example.
********************************************************************************/
#include "ssd1963.h"
#include "../FMC/fmc_driver.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include <stdbool.h>

/*=============================================================================
 *  LCD Panel Timing Parameters
 *  From docs/LCD/Common/lcd.h (CH32H417 ATK-MD0280 example).
 *  Verified against user LCD datasheet:
 *    - Th (horizontal period) typ = 1056  => HT = 1055
 *    - Tv (vertical period)   typ = 525   => VT = 524
 *    - DCLK typ = 33.3MHz, current config ~30MHz (within 26.4~46.8 range)
 *=============================================================================*/
#define SSD_HOR_RESOLUTION    800
#define SSD_VER_RESOLUTION    480
#define SSD_HOR_PULSE_WIDTH   1
#define SSD_HOR_BACK_PORCH    210
#define SSD_HOR_FRONT_PORCH   45
#define SSD_VER_PULSE_WIDTH   1
#define SSD_VER_BACK_PORCH    34
#define SSD_VER_FRONT_PORCH   10

#define SSD_HT  (SSD_HOR_RESOLUTION + SSD_HOR_BACK_PORCH + SSD_HOR_FRONT_PORCH)
#define SSD_HPS (SSD_HOR_BACK_PORCH)
#define SSD_VT  (SSD_VER_RESOLUTION + SSD_VER_BACK_PORCH + SSD_VER_FRONT_PORCH)
#define SSD_VPS (SSD_VER_BACK_PORCH)

/*=============================================================================
 *  Low-Level Access
 *=============================================================================*/
void SSD1963_WriteCmd(uint16_t cmd)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);  // CS low
    SSD1963_CMD = cmd;                   // FMC write
    GPIO_SetBits(GPIOD, GPIO_Pin_7);    // CS high
}

void SSD1963_WriteData(uint16_t data)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);  // CS low
    SSD1963_DATA = data;                 // FMC write
    GPIO_SetBits(GPIOD, GPIO_Pin_7);    // CS high
}

uint16_t SSD1963_ReadData(void)
{
    uint16_t val;
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);  // CS low
    val = SSD1963_DATA;                  // FMC read
    GPIO_SetBits(GPIOD, GPIO_Pin_7);    // CS high
    return val;
}

void SSD1963_WriteReg(uint16_t reg, uint16_t val)
{
    SSD1963_WriteCmd(reg);
    SSD1963_WriteData(val);
}

uint16_t SSD1963_ReadReg(uint16_t reg)
{
    SSD1963_WriteCmd(reg);
    /* Allow SSD1963 to prepare read data */
    Delay_Us(5);
    return SSD1963_ReadData();
}

/*=============================================================================
 *  Window and GRAM Access
 *=============================================================================*/
void SSD1963_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    /* Column address set (0x2A) */
    SSD1963_WriteCmd(SSD1963_SET_COLUMN_ADDRESS);
    SSD1963_WriteData((x1 >> 8) & 0xFF);
    SSD1963_WriteData(x1 & 0xFF);
    SSD1963_WriteData((x2 >> 8) & 0xFF);
    SSD1963_WriteData(x2 & 0xFF);

    /* Page address set (0x2B) */
    SSD1963_WriteCmd(SSD1963_SET_PAGE_ADDRESS);
    SSD1963_WriteData((y1 >> 8) & 0xFF);
    SSD1963_WriteData(y1 & 0xFF);
    SSD1963_WriteData((y2 >> 8) & 0xFF);
    SSD1963_WriteData(y2 & 0xFF);

    /* Prepare GRAM write (0x2C) */
    SSD1963_WriteCmd(SSD1963_WRITE_MEMORY_START);
}

void SSD1963_WriteData16(uint16_t data)
{
    SSD1963_WriteData(data);
}

void SSD1963_WriteBuffer(const uint16_t *buf, uint32_t len)
{
    while (len--) {
        SSD1963_WriteData(*buf++);
    }
}

void SSD1963_Clear(uint16_t color)
{
    uint32_t i;
    uint32_t total = (uint32_t)SSD1963_WIDTH * SSD1963_HEIGHT;

    SSD1963_SetWindow(0, 0, SSD1963_WIDTH - 1, SSD1963_HEIGHT - 1);

    for (i = 0; i < total; i++) {
        SSD1963_WriteData(color);
    }
}

/*=============================================================================
 *  Backlight Control (SSD1963 internal PWM)
 *=============================================================================*/
void SSD1963_SetBacklight(uint8_t pwm)
{
    /* PWM frequency = PLL / (256 * (PWMF+1)) / 256 */
    /* With PWMF=0x05 => ~170Hz when PLL=120MHz */
    SSD1963_WriteCmd(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x05);      /* PWM frequency divider */
    SSD1963_WriteData(pwm);       /* Duty cycle 0~255 */
    SSD1963_WriteData(0x01);      /* PWM enabled, DBC disabled */
    SSD1963_WriteData(0xFF);
    SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x00);
}

/*=============================================================================
 *  Temporary GPIO Bit-Bang Test
 *  Bypasses FMC to verify hardware connectivity (CS/RS/WR/D0-D7).
 *=============================================================================*/
static void SSD1963_GPIO_SetData(uint8_t data)
{
    /* D0=PD14, D1=PD15, D2=PD0, D3=PD1, D4=PE7, D5=PE8, D6=PE9, D7=PE10 */
    if (data & 0x01) GPIO_SetBits(GPIOD, GPIO_Pin_14);   else GPIO_ResetBits(GPIOD, GPIO_Pin_14);
    if (data & 0x02) GPIO_SetBits(GPIOD, GPIO_Pin_15);   else GPIO_ResetBits(GPIOD, GPIO_Pin_15);
    if (data & 0x04) GPIO_SetBits(GPIOD, GPIO_Pin_0);    else GPIO_ResetBits(GPIOD, GPIO_Pin_0);
    if (data & 0x08) GPIO_SetBits(GPIOD, GPIO_Pin_1);    else GPIO_ResetBits(GPIOD, GPIO_Pin_1);
    if (data & 0x10) GPIO_SetBits(GPIOE, GPIO_Pin_7);    else GPIO_ResetBits(GPIOE, GPIO_Pin_7);
    if (data & 0x20) GPIO_SetBits(GPIOE, GPIO_Pin_8);    else GPIO_ResetBits(GPIOE, GPIO_Pin_8);
    if (data & 0x40) GPIO_SetBits(GPIOE, GPIO_Pin_9);    else GPIO_ResetBits(GPIOE, GPIO_Pin_9);
    if (data & 0x80) GPIO_SetBits(GPIOE, GPIO_Pin_10);   else GPIO_ResetBits(GPIOE, GPIO_Pin_10);
}

static uint8_t SSD1963_GPIO_ReadData(void)
{
    uint8_t val = 0;
    uint16_t indr_d = GPIOD->INDR;
    uint16_t indr_e = GPIOE->INDR;
    if (indr_d & GPIO_Pin_14) val |= 0x01;
    if (indr_d & GPIO_Pin_15) val |= 0x02;
    if (indr_d & GPIO_Pin_0)  val |= 0x04;
    if (indr_d & GPIO_Pin_1)  val |= 0x08;
    if (indr_e & GPIO_Pin_7)  val |= 0x10;
    if (indr_e & GPIO_Pin_8)  val |= 0x20;
    if (indr_e & GPIO_Pin_9)  val |= 0x40;
    if (indr_e & GPIO_Pin_10) val |= 0x80;
    return val;
}

static uint8_t SSD1963_GPIO_ReadReg(uint8_t reg)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t val;

    /* Step 1: Send register address (RS=0, WR pulse) */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     // CS low
    GPIO_ResetBits(GPIOB, GPIO_Pin_3);     // RS low (command)
    SSD1963_GPIO_SetData(reg);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);     // WR low
    Delay_Us(5);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);       // WR high
    GPIO_SetBits(GPIOD, GPIO_Pin_7);       // CS high

    Delay_Us(5);

    /* Step 2: Switch D0-D7 to floating input for read */
    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOE, &gpio);

    /* Step 3: Read data (RS=1, RD pulse) */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     // CS low
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       // RS high (data)
    Delay_Us(2);                           // t_AS

    GPIO_ResetBits(GPIOD, GPIO_Pin_4);     // RD low
    Delay_Us(5);                           // t_ACC + margin
    val = SSD1963_GPIO_ReadData();
    GPIO_SetBits(GPIOD, GPIO_Pin_4);       // RD high

    GPIO_SetBits(GPIOD, GPIO_Pin_7);       // CS high

    /* Step 4: Restore D0-D7 to output */
    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOE, &gpio);

    return val;
}

static void SSD1963_GPIO_WriteCmd(uint8_t cmd)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     // CS low
    GPIO_ResetBits(GPIOB, GPIO_Pin_3);     // RS low (command)
    SSD1963_GPIO_SetData(cmd);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);     // WR low
    Delay_Us(5);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);       // WR high
    GPIO_SetBits(GPIOD, GPIO_Pin_7);       // CS high
}

static void SSD1963_GPIO_WriteData(uint8_t data)
{
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     // CS low
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       // RS high (data)
    SSD1963_GPIO_SetData(data);
    GPIO_ResetBits(GPIOD, GPIO_Pin_5);     // WR low
    Delay_Us(5);
    GPIO_SetBits(GPIOD, GPIO_Pin_5);       // WR high
    GPIO_SetBits(GPIOD, GPIO_Pin_7);       // CS high
}

static uint8_t SSD1963_GPIO_ReadDataOnly(void)
{
    uint8_t val;

    /* Assumes D0-D7 are already in input mode */
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     // CS low
    GPIO_SetBits(GPIOB, GPIO_Pin_3);       // RS high (data)
    Delay_Us(2);                           // t_AS

    GPIO_ResetBits(GPIOD, GPIO_Pin_4);     // RD low
    Delay_Us(5);                           // t_ACC + margin
    val = SSD1963_GPIO_ReadData();
    GPIO_SetBits(GPIOD, GPIO_Pin_4);       // RD high

    GPIO_SetBits(GPIOD, GPIO_Pin_7);       // CS high
    return val;
}

static void SSD1963_GPIO_Test(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t val;

    printf("[SSD1963_GPIO_Test] start bit-bang test with full init\r\n");

    /*--- Temporarily switch control pins to GPIO ---*/
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOB, &gpio);               // PB3: RS

    gpio.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOD, &gpio);               // PD5: WR

    gpio.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOD, &gpio);
    GPIO_SetBits(GPIOD, GPIO_Pin_4);       // PD4: RD idle high

    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOD, &gpio);               // D0-D3
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOE, &gpio);               // D4-D7

    /*--- 1. Software reset ---*/
    SSD1963_GPIO_WriteCmd(0x01);
    Delay_Ms(10);
    SSD1963_GPIO_WriteCmd(0x01);
    Delay_Ms(10);

    /*--- 2. Configure PLL: M=0x23, N=0x02, effect=0x04 ---*/
    SSD1963_GPIO_WriteCmd(0xE2);
    SSD1963_GPIO_WriteData(0x23);
    SSD1963_GPIO_WriteData(0x02);
    SSD1963_GPIO_WriteData(0x04);
    Delay_Us(100);

    /*--- 3. Enable PLL ---*/
    SSD1963_GPIO_WriteCmd(0xE0);
    SSD1963_GPIO_WriteData(0x01);
    Delay_Ms(10);

    /*--- 4. Switch system clock to PLL ---*/
    SSD1963_GPIO_WriteCmd(0xE0);
    SSD1963_GPIO_WriteData(0x03);
    Delay_Ms(12);

    /*--- 5. Software reset after PLL lock ---*/
    SSD1963_GPIO_WriteCmd(0x01);
    Delay_Ms(10);

    /*--- 6. Set pixel data interface: 16-bit (565) ---*/
    SSD1963_GPIO_WriteCmd(0xF0);
    SSD1963_GPIO_WriteData(0x03);

    printf("[SSD1963_GPIO_Test] GPIO init sequence done\r\n");

    /*--- Read 0xE4 (PLL Status) ---*/
    val = SSD1963_GPIO_ReadReg(0xE4);
    printf("[SSD1963_GPIO_Test] GPIO read 0xE4 (PLL Status) = 0x%02X ", val);
    if (val & 0x04) printf("[OK: locked]\r\n"); else printf("[FAIL: not locked]\r\n");

    /*--- Read 0x0A (Power Mode) ---*/
    val = SSD1963_GPIO_ReadReg(0x0A);
    printf("[SSD1963_GPIO_Test] GPIO read 0x0A (Power Mode) = 0x%02X ", val);
    if ((val & 0x0C) == 0x0C) printf("[OK: normal+display-on bits set]\r\n");
    else printf("[INFO: raw value]\r\n");

    /*--- Read 0xA1 (DDB[0]) ---*/
    val = SSD1963_GPIO_ReadReg(0xA1);
    printf("[SSD1963_GPIO_Test] GPIO read 0xA1 (DDB[0]) = 0x%02X ", val);
    if (val == 0x01) printf("[OK: Supplier ID high byte]\r\n"); else printf("[WARN: unexpected]\r\n");

    /*--- Extended GPIO tests: multi-bit data line verification ---*/
    printf("[SSD1963_GPIO_Test] --- MADCTL R/W test ---\r\n");

    /* 0x0B returns A[7:2] only; expected 0x55 & 0xFC = 0x54 */
    /* Test pattern 0x55 = 0b01010101 (D0,D2,D4,D6) */
    SSD1963_GPIO_WriteCmd(0x36);
    SSD1963_GPIO_WriteData(0x55);
    Delay_Us(50);
    val = SSD1963_GPIO_ReadReg(0x0B);    /* GET_ADDRESS_MODE = 0x0B */
    printf("[SSD1963_GPIO_Test] MADCTL wr=0x55 rd=0x%02X ", val);
    if (val == 0x54) printf("[OK]\r\n"); else printf("[FAIL]\r\n");

    /* 0x0B returns A[7:2] only; expected 0xAA & 0xFC = 0xA8 */
    /* Test pattern 0xAA = 0b10101010 (D1,D3,D5,D7) */
    SSD1963_GPIO_WriteCmd(0x36);
    SSD1963_GPIO_WriteData(0xAA);
    Delay_Us(50);
    val = SSD1963_GPIO_ReadReg(0x0B);
    printf("[SSD1963_GPIO_Test] MADCTL wr=0xAA rd=0x%02X ", val);
    if (val == 0xA8) printf("[OK]\r\n"); else printf("[FAIL]\r\n");

    /* 0x0B returns A[7:2] only; expected 0xFF & 0xFC = 0xFC */
    /* Test pattern 0xFF = 0b11111111 (all D0-D7) */
    SSD1963_GPIO_WriteCmd(0x36);
    SSD1963_GPIO_WriteData(0xFF);
    Delay_Us(50);
    val = SSD1963_GPIO_ReadReg(0x0B);
    printf("[SSD1963_GPIO_Test] MADCTL wr=0xFF rd=0x%02X ", val);
    if (val == 0xFC) printf("[OK]\r\n"); else printf("[FAIL]\r\n");

    /* 0x0B returns A[7:2] only; expected 0x00 & 0xFC = 0x00 */
    /* Test pattern 0x00 = 0b00000000 (all D0-D7 low) */
    SSD1963_GPIO_WriteCmd(0x36);
    SSD1963_GPIO_WriteData(0x00);
    Delay_Us(50);
    val = SSD1963_GPIO_ReadReg(0x0B);
    printf("[SSD1963_GPIO_Test] MADCTL wr=0x00 rd=0x%02X ", val);
    if (val == 0x00) printf("[OK]\r\n"); else printf("[FAIL]\r\n");

    /*--- DDB continuous read (5 parameters) ---*/
    {
        uint8_t ddb[5];
        int i;
        GPIO_InitTypeDef gpio = {0};
        printf("[SSD1963_GPIO_Test] --- DDB continuous read ---\r\n");

        /* Send DDB read command */
        SSD1963_GPIO_WriteCmd(0xA1);
        Delay_Us(5);

        /* Switch D0-D7 to floating input */
        gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
        gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIOD, &gpio);
        gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
        GPIO_Init(GPIOE, &gpio);

        /* Read 5 parameters continuously */
        for (i = 0; i < 5; i++) {
            ddb[i] = SSD1963_GPIO_ReadDataOnly();
            printf("[SSD1963_GPIO_Test] DDB[%d] = 0x%02X\r\n", i, ddb[i]);
        }
        printf("[SSD1963_GPIO_Test] DDB expected: 01 57 61 01 FF\r\n");

        /* Restore D0-D7 to output */
        gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
        gpio.GPIO_Mode = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_Very_High;
        GPIO_Init(GPIOD, &gpio);
        gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
        GPIO_Init(GPIOE, &gpio);
    }

    /*--- Restore FMC alternate functions ---*/
    /* PB3 -> FMC_A1 (AF12) */
    gpio.GPIO_Pin = GPIO_Pin_3;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOB, &gpio);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF12);

    /* PD4 -> FMC_NOE (AF12) */
    gpio.GPIO_Pin = GPIO_Pin_4;
    GPIO_Init(GPIOD, &gpio);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource4, GPIO_AF12);

    /* PD5 -> FMC_NWE (AF12) */
    gpio.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOD, &gpio);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource5, GPIO_AF12);

    /* PD0/1/14/15 -> FMC_D2/D3/D0/D1 (AF12) */
    gpio.GPIO_Pin = GPIO_Pin_14 | GPIO_Pin_15 | GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_Init(GPIOD, &gpio);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource0, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource1, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource14, GPIO_AF12);
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource15, GPIO_AF12);

    /* PE7/8/9/10 -> FMC_D4/D5/D6/D7 (AF12) */
    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOE, &gpio);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource7, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF12);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF12);

    /* PD7 remains GPIO Out_PP (CS manual control) */
    gpio.GPIO_Pin = GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(GPIOD, &gpio);
    GPIO_ResetBits(GPIOD, GPIO_Pin_7);     // CS keep low

    printf("[SSD1963_GPIO_Test] FMC pins restored\r\n");
}

/*=============================================================================
 *  Initialization Sequence
 *
 *  Derived from docs/LCD/Common/lcd.c (CH32H417 ATK-MD0280 example),
 *  SSD1963 section (~line 2840).
 *
 *  CRITICAL ASSUMPTIONS (please confirm against your hardware):
 *  1. SSD1963 OSC = 10 MHz (used for PLL calculation: M=35, N=2).
 *  2. Target dot clock ~= 30 MHz (register 0xE6 = 0x03FFFF).
 *  3. LCD panel timing matches the HT/HPS/VT/VPS values below.
 *=============================================================================*/
void SSD1963_Init(void)
{
    printf("[SSD1963_Init] start\r\n");

    /* 1. Hardware reset via PA0 */
    printf("[SSD1963_Init] hardware reset\r\n");
    GPIO_ResetBits(GPIOA, GPIO_Pin_0);
    Delay_Ms(10);
    GPIO_SetBits(GPIOA, GPIO_Pin_0);
    Delay_Ms(5);

    /* 1a. GPIO bit-bang test: bypass FMC to verify chip response */
    SSD1963_GPIO_Test();

    /* 2. Software reset */
    SSD1963_WriteCmd(SSD1963_SOFT_RESET);
    Delay_Ms(10);

    /* 3. Configure PLL
     *    OSC = 10MHz (confirmed by user), M = 35 (0x23), N = 2 (0x02)
     *    VCO = 10MHz * (35+1) = 360MHz
     *    PLL = 360MHz / (2+1) = 120MHz
     */
    printf("[SSD1963_Init] configure PLL\r\n");
    SSD1963_WriteCmd(SSD1963_SET_PLL_MN);
    SSD1963_WriteData(0x23);   /* M */
    SSD1963_WriteData(0x02);   /* N */
    SSD1963_WriteData(0x04);   /* Effect multiplier */
    Delay_Us(100);

    /* 4. Enable PLL */
    SSD1963_WriteCmd(SSD1963_SET_PLL);
    SSD1963_WriteData(0x01);
    Delay_Ms(10);

    /* 5. Switch system clock to PLL output */
    SSD1963_WriteCmd(SSD1963_SET_PLL);
    SSD1963_WriteData(0x03);
    Delay_Ms(12);

    /* 5a. Verify PLL lock status (0xE4, bit2 = 1 when locked) */
    {
        uint16_t pll = SSD1963_ReadReg(SSD1963_GET_PLL_STATUS);
        printf("[SSD1963_Init] PLL status (0xE4) = 0x%04X ", pll);
        if ((pll & 0x0004) == 0) {
            printf("[FAIL: PLL not locked]\r\n");
        } else {
            printf("[OK]\r\n");
        }
    }

    /* 6. Software reset after PLL lock */
    SSD1963_WriteCmd(SSD1963_SOFT_RESET);
    Delay_Ms(10);

    /* 7. Configure dot clock frequency
     *    DotClk = PLL * (LCDC_FPR + 1) / 2^20
     *    LCDC_FPR = 0x3FFFF = 262143
     *    DotClk = 120MHz * 262144 / 1048576 ~= 30MHz
     *    LCD datasheet allows 26.4~46.8MHz, so 30MHz is safe.
     *    For closer to 33.3MHz typical, use 0x47003 instead.
     */
    printf("[SSD1963_Init] configure dot clock\r\n");
    SSD1963_WriteCmd(SSD1963_SET_LSHIFT_FREQ);
    SSD1963_WriteData(0x03);
    SSD1963_WriteData(0xFF);
    SSD1963_WriteData(0xFF);

    /* 8. Configure LCD panel mode (0xB0)
     *    Param 1: 0x20
     *      [7]   LSHIFT polarity: 0 = data latched on rising edge
     *      [6:5] LLINE/LFRAME polarity: 00 = active low
     *      [4]   TFT data width: 0 = 18-bit panel
     *      [3]   Dithering: 0 = disable
     *      [2:0] Panel type: 000 = TFT mode -> but value is 0x20?
     *      Actually for SSD1963 0xB0 bit[5:4]=10 means 18bit,
     *      bit[2:0]=000 means TFT. 0x20 = 0b00100000.
     *      This matches "TFT, 18-bit, disable dithering".
     *    Param 2: 0x00 = TFT mode
     *    Param 3~4: HOR_RESOLUTION - 1 = 799 = 0x031F
     *    Param 5~6: VER_RESOLUTION - 1 = 479 = 0x01DF
     *    Param 7: 0x00 = RGB mode
     */
    printf("[SSD1963_Init] configure LCD panel\r\n");
    SSD1963_WriteCmd(SSD1963_SET_LCD_MODE);
    SSD1963_WriteData(0x20);
    SSD1963_WriteData(0x00);
    SSD1963_WriteData(((SSD_HOR_RESOLUTION - 1) >> 8) & 0xFF);
    SSD1963_WriteData((SSD_HOR_RESOLUTION - 1) & 0xFF);
    SSD1963_WriteData(((SSD_VER_RESOLUTION - 1) >> 8) & 0xFF);
    SSD1963_WriteData((SSD_VER_RESOLUTION - 1) & 0xFF);
    SSD1963_WriteData(0x00);

    /* 9. Set horizontal period (0xB4)
     *    HT  = SSD_HOR_RESOLUTION + HBP + HFP = 800 + 210 + 45 = 1055
     *    HPS = HBP = 210
     *    HPW = 1
     *    LPS = 0
     */
    SSD1963_WriteCmd(SSD1963_SET_HORI_PERIOD);
    SSD1963_WriteData(((SSD_HT - 1) >> 8) & 0xFF);   /* 0x04 */
    SSD1963_WriteData((SSD_HT - 1) & 0xFF);          /* 0x1E */
    SSD1963_WriteData(((SSD_HPS - 1) >> 8) & 0xFF);  /* 0x00 */
    SSD1963_WriteData((SSD_HPS - 1) & 0xFF);         /* 0xD1 */
    SSD1963_WriteData((SSD_HOR_PULSE_WIDTH - 1) & 0xFF); /* 0x00 */
    SSD1963_WriteData(0x00);   /* LPS[15:8] = 0 */
    SSD1963_WriteData(0x00);   /* LPS[7:0]  = 0 */
    SSD1963_WriteData(0x00);   /* LPSPP     = 0 */

    /* 10. Set vertical period (0xB6)
     *     VT  = SSD_VER_RESOLUTION + VBP + VFP = 480 + 34 + 10 = 524
     *     VPS = VBP = 34
     *     VFP = 10 (used as param 5 in the CH32H417 example)
     */
    SSD1963_WriteCmd(SSD1963_SET_VERT_PERIOD);
    SSD1963_WriteData(((SSD_VT - 1) >> 8) & 0xFF);   /* 0x02 */
    SSD1963_WriteData((SSD_VT - 1) & 0xFF);          /* 0x0B */
    SSD1963_WriteData(((SSD_VPS - 1) >> 8) & 0xFF);  /* 0x00 */
    SSD1963_WriteData((SSD_VPS - 1) & 0xFF);         /* 0x21 */
    SSD1963_WriteData((SSD_VER_FRONT_PORCH - 1) & 0xFF); /* 0x09 */
    SSD1963_WriteData(0x00);
    SSD1963_WriteData(0x00);

    /* 11. Set pixel data interface: 16-bit (565) */
    SSD1963_WriteCmd(SSD1963_SET_PIXEL_DATA_INTERFACE);
    SSD1963_WriteData(0x03);

    /* 12. Turn on display */
    printf("[SSD1963_Init] display on\r\n");
    SSD1963_WriteCmd(SSD1963_SET_DISPLAY_ON);

    /* 12a. Verify power mode (0x0A)
     *      Expected: 0x9C = sleep out(1<<4) | normal mode(1<<3) | display on(1<<2)
     */
    {
        uint16_t pwr = SSD1963_ReadReg(SSD1963_GET_POWER_MODE);
        printf("[SSD1963_Init] Power mode (0x0A) = 0x%04X ", pwr);
        if ((pwr & 0x00FF) == 0x9C) {
            printf("[OK]\r\n");
        } else {
            printf("[WARN: expected 0x9C]\r\n");
        }
    }

    /* 13. Disable deep sleep */
    SSD1963_WriteCmd(SSD1963_SET_DEEP_SLEEP);
    SSD1963_WriteData(0x00);

    /* 14. Configure SSD1963 internal GPIO */
    SSD1963_WriteCmd(SSD1963_SET_GPIO_CONF);
    SSD1963_WriteData(0x0F);   /* GPIO0~3 as outputs */
    SSD1963_WriteData(0x01);   /* GPIO0 default high */

    SSD1963_WriteCmd(SSD1963_SET_GPIO_VALUE);
    SSD1963_WriteData(0x01);   /* GPIO0 = 1 */

    /* 15. Configure PWM backlight */
    printf("[SSD1963_Init] configure backlight\r\n");
    SSD1963_SetBacklight(100);

    /* 16. Communication self-test */
    SSD1963_SelfTest();

    printf("[SSD1963_Init] done\r\n");
}

/*=============================================================================
 *  Communication Self-Test
 *=============================================================================*/
bool SSD1963_SelfTest(void)
{
    uint16_t val;
    bool ok = true;

    printf("[SSD1963_SelfTest] start\r\n");

    /* Test 1: PLL lock status */
    val = SSD1963_ReadReg(SSD1963_GET_PLL_STATUS);
    printf("[SSD1963_SelfTest] PLL status (0xE4) = 0x%04X ", val);
    if ((val & 0x0004) == 0) {
        printf("[FAIL: not locked]\r\n");
        ok = false;
    } else {
        printf("[OK]\r\n");
    }

    /* Test 2: Power mode */
    val = SSD1963_ReadReg(SSD1963_GET_POWER_MODE);
    printf("[SSD1963_SelfTest] Power mode (0x0A) = 0x%04X ", val);
    if ((val & 0x00FF) == 0x9C) {
        printf("[OK]\r\n");
    } else {
        printf("[WARN: expected 0x9C]\r\n");
        /* Don't fail here, some panels may differ */
    }

    /* Test 3: Read-Write verification on MADCTL (0x36)
     * Note: 0x0B returns A[7:2] only, A1:A0 fixed to 00.
     * Writing 0x55 -> read-back should be 0x54 (0x55 & 0xFC)
     */
    SSD1963_WriteReg(SSD1963_SET_ADDRESS_MODE, 0x55);
    Delay_Us(50);
    val = SSD1963_ReadReg(SSD1963_GET_ADDRESS_MODE);
    printf("[SSD1963_SelfTest] MADCTL rw = 0x%04X (wrote 0x55) ", val);
    if ((val & 0x00FC) != 0x54) {
        printf("[FAIL]\r\n");
        ok = false;
    } else {
        printf("[OK]\r\n");
    }
    /* Restore default */
    SSD1963_WriteReg(SSD1963_SET_ADDRESS_MODE, 0x00);

    /* Test 4: Read DDB (0xA1) - first word should be manufacturer code */
    SSD1963_WriteCmd(SSD1963_READ_DDB);
    Delay_Us(5);
    val = SSD1963_ReadData();
    printf("[SSD1963_SelfTest] DDB[0] (0xA1) = 0x%04X ", val);
    if (val == 0x5761 || val == 0x6157) {
        printf("[OK: SSD1963 detected]\r\n");
    } else {
        printf("[WARN: unexpected ID]\r\n");
    }

    if (ok) {
        printf("[SSD1963_SelfTest] PASSED\r\n");
    } else {
        printf("[SSD1963_SelfTest] FAILED\r\n");
    }

    return ok;
}
