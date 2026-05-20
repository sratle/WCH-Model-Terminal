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
#include "ch32h417_rcc.h"
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
 *  Low-Level Access (via FMC 8080)
 *  FMC Bank1 NORSRAM1: CMD @ 0x60000000, DATA @ 0x60000004
 *  Writing to CMD_ADDR asserts RS=0 (command phase).
 *  Writing to DATA_ADDR asserts RS=1 (data phase).
 *  FMC auto-generates WR/CS timing, no manual GPIO toggling needed.
 *=============================================================================*/
void SSD1963_WriteCmd(uint16_t cmd)   { SSD1963_CMD = cmd; }
void SSD1963_WriteData(uint16_t data) { SSD1963_DATA = data; }

uint16_t SSD1963_ReadData(void)
{
    return SSD1963_DATA;
}

void SSD1963_WriteReg(uint16_t reg, uint16_t val)
{
    SSD1963_CMD = reg;
    SSD1963_DATA = val;
}

uint16_t SSD1963_ReadReg(uint16_t reg)
{
    SSD1963_CMD = reg;
    __NOP();
    return SSD1963_ReadData();
}

/*=============================================================================
 *  Window and GRAM Access (via FMC 8080)
 *=============================================================================*/
void SSD1963_SetWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    SSD1963_CMD = 0x2A;
    SSD1963_DATA = (x1 >> 8) & 0xFF;
    SSD1963_DATA = x1 & 0xFF;
    SSD1963_DATA = (x2 >> 8) & 0xFF;
    SSD1963_DATA = x2 & 0xFF;

    SSD1963_CMD = 0x2B;
    SSD1963_DATA = (y1 >> 8) & 0xFF;
    SSD1963_DATA = y1 & 0xFF;
    SSD1963_DATA = (y2 >> 8) & 0xFF;
    SSD1963_DATA = y2 & 0xFF;

    SSD1963_CMD = 0x2C;
}

void SSD1963_WriteData16(uint16_t data)
{
    SSD1963_DATA = data;
}

void SSD1963_WriteData16Fast(uint16_t data)
{
    SSD1963_DATA = data;
}

void SSD1963_WriteBuffer(const uint16_t *buf, uint32_t len)
{
    volatile uint16_t *data_reg = &SSD1963_DATA;
    while (len--) {
        *data_reg = *buf++;
    }
}

void SSD1963_FillColor(uint32_t count, uint16_t color)
{
    volatile uint16_t *data_reg = &SSD1963_DATA;
    uint32_t count8 = count / 8;
    uint32_t rem = count % 8;
    while (count8--) {
        *data_reg = color;
        *data_reg = color;
        *data_reg = color;
        *data_reg = color;
        *data_reg = color;
        *data_reg = color;
        *data_reg = color;
        *data_reg = color;
    }
    while (rem--) {
        *data_reg = color;
    }
}

void SSD1963_Clear(uint16_t color)
{
    SSD1963_SetWindow(0, 0, 800 - 1, 480 - 1);
    SSD1963_FillColor((uint32_t)800 * 480, color);
}

/*=============================================================================
 *  Backlight Control (SSD1963 internal PWM)
 *=============================================================================*/
void SSD1963_SetBacklight(uint8_t pwm)
{
    /* PWM frequency = PLL / (256 * PWMF) / 256
     * With PLL~100MHz and PWMF=1 => ~1.5kHz (good for LED, no flicker)
     * Duty cycle = PWM[7:0] / 256
     * Param 3: C[0]=1(enable), C[3]=0(host control)
     * Param 4: D=0xFF (max manual brightness, no effect if DBC off)
     * Param 5: E=0x00 (DBC min brightness, no effect if DBC off)
     * Param 6: F=0x00 (brightness prescaler off)
     */
    SSD1963_WriteCmd(SSD1963_SET_PWM_CONF);
    SSD1963_WriteData(0x01);      /* PWMF = 1 => ~1.5kHz */
    SSD1963_WriteData(pwm);       /* Duty cycle 0~255 */
    SSD1963_WriteData(0x01);      /* C=0x01: PWM enable, host controlled */
    SSD1963_WriteData(0xFF);      /* D=0xFF: manual brightness max */
    SSD1963_WriteData(0x00);      /* E=0x00: DBC min brightness */
    SSD1963_WriteData(0x00);      /* F=0x00: prescaler off */
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

    /* 1. Initialize FMC Bank1 NORSRAM 8080 interface (also inits PA0 RESET pin) */
    FMC_Driver_Init();

    /* 1a. Hardware reset SSD1963 via PA0 */
    printf("[SSD1963_Init] hardware reset\r\n");
    GPIO_ResetBits(GPIOA, GPIO_Pin_0);
    Delay_Ms(10);
    GPIO_SetBits(GPIOA, GPIO_Pin_0);
    Delay_Ms(5);

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

    /* 12. Exit sleep mode, then turn on display */
    printf("[SSD1963_Init] display on\r\n");
    SSD1963_WriteCmd(SSD1963_EXIT_SLEEP_MODE);
    Delay_Ms(10);
    SSD1963_WriteCmd(SSD1963_SET_DISPLAY_ON);
    
    /* 13. Disable deep sleep */
    SSD1963_WriteCmd(SSD1963_SET_DEEP_SLEEP);
    SSD1963_WriteData(0x00);

    /* Ensure BGR output order (D3=1) for correct color on this LCD panel */
    SSD1963_WriteReg(SSD1963_SET_ADDRESS_MODE, 0x08);

    /* 14. Configure SSD1963 internal GPIO */
    SSD1963_WriteCmd(SSD1963_SET_GPIO_CONF);
    SSD1963_WriteData(0x0F);   /* GPIO0~3 as outputs */
    SSD1963_WriteData(0x01);   /* GPIO0 default high */

    SSD1963_WriteCmd(SSD1963_SET_GPIO_VALUE);
    SSD1963_WriteData(0x01);   /* GPIO0 = 1 */

    /* 15. PWM backlight configured by settings module after init */
    printf("[SSD1963_Init] backlight will be set by settings_init()\r\n");

    /* 16. Enable Tearing Effect (TE) output on SSD1963 TE pin (connected to PB8).
     * Command 0x35 with parameter bit[0]=1: TE pulses at vertical blanking.
     * This allows MCU to sync writes to the VSYNC period, preventing tearing. */
    SSD1963_WriteCmd(SSD1963_SET_TEAR_ON);
    SSD1963_WriteData(0x00);   /* bit0=0: TE only during V-blanking (not H-blanking) */

    // /* 16. Communication self-test */
    // SSD1963_SelfTest();

    printf("[SSD1963_Init] done\r\n");
}

/*=============================================================================
 *  VSYNC / Tearing Effect Synchronization
 *  PB8 is connected to SSD1963 TE pin.
 *  TE goes high at the start of vertical blanking period.
 *  By waiting for TE rising edge before writing, we avoid tearing.
 *=============================================================================*/

void SSD1963_TE_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    printf("[SSD1963_TE_Init] PB8 configured as TE input\r\n");
}

bool SSD1963_TE_IsHigh(void)
{
    return (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8) != Bit_RESET);
}

void SSD1963_WaitVSync(void)
{
    uint32_t timeout = 100000;  /* ~1ms timeout at 100MHz */

    /* Wait for TE to go low first (ensure we catch the next rising edge) */
    while (SSD1963_TE_IsHigh()) {
        if (--timeout == 0) return;
    }

    /* Wait for TE rising edge = start of vertical blanking */
    timeout = 100000;
    while (!SSD1963_TE_IsHigh()) {
        if (--timeout == 0) return;
    }
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
    if ((val & 0x00FF) == 0x1C) {
        printf("[OK]\r\n");
    } else {
        printf("[WARN: expected 0x1C]\r\n");
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
    /* Restore default: BGR mode (D3=1) to match LCD panel wiring */
    SSD1963_WriteReg(SSD1963_SET_ADDRESS_MODE, 0x08);

    /* Test 4: Read DDB (0xA1) - 5 parameters total.
     * In 8-bit read mode we only fetch the first byte: SSL[15:8] = 0x01.
     * Full sequence would be: 0x01, 0x57, 0x61, 0x01, 0xFF */
    SSD1963_WriteCmd(SSD1963_READ_DDB);
    Delay_Us(5);
    val = SSD1963_ReadData();
    printf("[SSD1963_SelfTest] DDB[0] (0xA1) = 0x%04X ", val);
    if ((val & 0x00FF) == 0x01) {
        printf("[OK: Supplier ID high byte = 0x01]\r\n");
    } else {
        printf("[WARN: expected 0x01]\r\n");
    }

    if (ok) {
        printf("[SSD1963_SelfTest] PASSED\r\n");
    } else {
        printf("[SSD1963_SelfTest] FAILED\r\n");
    }

    return ok;
}
