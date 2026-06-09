#include "ttp229.h"
#include "debug.h"

/*
 * TTP229 serial interface timing:
 *   DV (data valid) typical: 93 us
 *   SCL min pulse width (Tw): 10 us
 *   SCL timeout: 2 ms
 *   Response time (16-key mode): ~32 ms
 */

#define TTP229_SCL_DELAY_US  10  /* SCL pulse half-period (Tw >= 10us) */
#define TTP229_DV_TIMEOUT    5000 /* DV timeout (~278us at 72MHz)
                                  * TTP229 DV typical: 93us, response: ~32ms.
                                  * Must be long enough for TTP229 to assert DV
                                  * after previous read. If DV not ready, use
                                  * cached data. */

/* Chip #1 uses only TP0~TP3 (bits 12~15) and TP12~TP15 (bits 0~3).
 * TP4~TP11 (bits 4~11) are unused and must be masked out to prevent
 * floating pins from triggering false key events. */
#define CHIP1_USED_MASK  0xF00Fu

/* ======================== GPIO Helpers ======================== */

static void SCL_High(GPIO_TypeDef *port, uint16_t pin)
{
    port->BSHR = pin;
}

static void SCL_Low(GPIO_TypeDef *port, uint16_t pin)
{
    port->BCR = pin;
}

static uint8_t SDO_Read(GPIO_TypeDef *port, uint16_t pin)
{
    return (port->INDR & pin) ? 1 : 0;
}

/* Cached raw data (returned when DV timeout) */
static uint16_t cached_raw1 = 0xFFFF;  /* all keys untouched (active-low) */
static uint16_t cached_raw2 = 0xFFFF;

/* Calibration baseline: captured at startup when no keys are touched.
 * In active-low mode, bit=1 means untouched. The baseline records which
 * bits are normally 1 (untouched). A key is "touched" only when a bit
 * CHANGES from the baseline value (1→0 transition). */
static uint16_t baseline_raw1 = 0xFFFF;
static uint16_t baseline_raw2 = 0xFFFF;

/* ======================== Raw 16-bit Read ======================== */

/*
 * Read 16-bit raw data from one TTP229 chip.
 * Waits for SDO (DV) signal, then clocks 17 bits.
 * If DV not ready within timeout, returns cached value (non-blocking).
 *
 * TTP229-BSF outputs 17 bits per cycle:
 *   D0 = status/padding bit (always 1 when idle)
 *   D1~D16 = TP0~TP15
 *
 * With 17 SCL clocks into a 16-bit variable:
 *   The padding bit is shifted out via overflow.
 *   raw[15] = TP0, raw[14] = TP1, ..., raw[0] = TP15
 *
 * Mapping: TPn → raw bit (15 - n)
 */
static uint16_t TTP229_ReadRaw(GPIO_TypeDef *sdo_port, uint16_t sdo_pin,
                                GPIO_TypeDef *scl_port, uint16_t scl_pin,
                                uint16_t cached)
{
    uint32_t raw32 = 0;  /* 32-bit to hold 17 bits during shifting */
    uint32_t timeout;
    uint8_t i;

    /* Wait for DV (SDO goes LOW = data valid) */
    timeout = TTP229_DV_TIMEOUT;
    while (!(sdo_port->INDR & sdo_pin))
    {
        if (--timeout == 0)
            return cached;  /* DV not ready, use cached data */
    }

    /* Clock 17 bits (Tw >= 10us per half-period).
     * D0 (padding) is read first, then D1~D16 (TP0~TP15).
     * The padding bit overflows out of the 16-bit result. */
    for (i = 0; i < 17; i++)
    {
        SCL_High(scl_port, scl_pin);
        Delay_Us(TTP229_SCL_DELAY_US);
        raw32 <<= 1;
        raw32 |= SDO_Read(sdo_port, sdo_pin);
        SCL_Low(scl_port, scl_pin);
        Delay_Us(TTP229_SCL_DELAY_US);
    }

    return (uint16_t)(raw32 & 0xFFFF);
}

/* ======================== TP→Key Mapping ======================== */

/*
 * TTP229 #1 reverse lookup: chip_map[i] = bitmap bit for TPi
 *
 * The check `raw & (1 << (15-i))` tests raw bit (15-i):
 *   raw[15] = first clocked = TP0,  raw[0] = last clocked = TP15
 *   So raw bit (15-i) = TPi  →  chip_map[i] gives bitmap bit for TPi
 *
 * Chip #1 (from Keyboard-3.md):
 *   TP0  → Key 21 (bit 20)    TP12 → Key 17 (bit 16)
 *   TP1  → Key 22 (bit 21)    TP13 → Key 18 (bit 17)
 *   TP2  → Key 23 (bit 22)    TP14 → Key 19 (bit 18)
 *   TP3  → Key 24 (bit 23)    TP15 → Key 20 (bit 19)
 *   TP4~TP11 → unused
 */
static const uint8_t chip1_map[16] = {
    20, 21, 22, 23,  /* i=0~3: TP0~TP3 → bitmap bit 20~23 (Key 21~24) */
    0xFF, 0xFF, 0xFF, 0xFF,  /* i=4~7: TP4~TP7 unused */
    0xFF, 0xFF, 0xFF, 0xFF,  /* i=8~11: TP8~TP11 unused */
    16, 17, 18, 19   /* i=12~15: TP12~TP15 → bitmap bit 16~19 (Key 17~20) */
};

/*
 * TTP229 #2 reverse lookup: chip_map[i] = bitmap bit for TP(i+16)
 *
 * Same logic as chip1: raw bit (15-i) = TP(i+16)
 *   chip_map[i] gives bitmap bit for TP(i+16)
 *
 * Chip #2 (from Keyboard-3.md):
 *   TP16 → Key 5  (bit 4)     TP24 → Key 12 (bit 11)
 *   TP17 → Key 6  (bit 5)     TP25 → Key 11 (bit 10)
 *   TP18 → Key 7  (bit 6)     TP26 → Key 10 (bit 9)
 *   TP19 → Key 8  (bit 7)     TP27 → Key 9  (bit 8)
 *   TP20 → Key 16 (bit 15)    TP28 → Key 1  (bit 0)
 *   TP21 → Key 15 (bit 14)    TP29 → Key 2  (bit 1)
 *   TP22 → Key 14 (bit 13)    TP30 → Key 3  (bit 2)
 *   TP23 → Key 13 (bit 12)    TP31 → Key 4  (bit 3)
 */
static const uint8_t chip2_map[16] = {
    4,  5,  6,  7,   /* i=0~3: TP16~TP19 → bitmap bit 4~7 (Key 5~8) */
    15, 14, 13, 12,  /* i=4~7: TP20~TP23 → bitmap bit 15~12 (Key 16~13) */
    11, 10, 9,  8,   /* i=8~11: TP24~TP27 → bitmap bit 11~8 (Key 12~9) */
    0,  1,  2,  3    /* i=12~15: TP28~TP31 → bitmap bit 0~3 (Key 1~4) */
};

/* ======================== Public API ======================== */

void TTP229_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* Enable GPIOB clock (all TTP229 pins on GPIOB) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* TTP229 #1 SDO (PB15) - input floating */
    gpio.GPIO_Pin   = TTP1_SDO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(TTP1_SDO_PORT, &gpio);

    /* TTP229 #1 SCL (PB14) - push-pull output, idle low */
    gpio.GPIO_Pin   = TTP1_SCL_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(TTP1_SCL_PORT, &gpio);
    SCL_Low(TTP1_SCL_PORT, TTP1_SCL_PIN);

    /* TTP229 #2 SDO (PB13) - input floating */
    gpio.GPIO_Pin   = TTP2_SDO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(TTP2_SDO_PORT, &gpio);

    /* TTP229 #2 SCL (PB12) - push-pull output, idle low */
    gpio.GPIO_Pin   = TTP2_SCL_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(TTP2_SCL_PORT, &gpio);
    SCL_Low(TTP2_SCL_PORT, TTP2_SCL_PIN);
}

/*
 * Calibrate TTP229 baseline after power-on stabilization.
 * Call once after the 500ms startup delay. Reads both chips multiple
 * times and captures the idle state as baseline. Bits that read as 0
 * in the idle state are parasitic and will be compensated at runtime.
 */
void TTP229_Calibrate(void)
{
    uint8_t attempt;
    uint16_t r1, r2;

    /* Discard first few reads to let the shift register stabilize */
    for (attempt = 0; attempt < 4; attempt++)
    {
        TTP229_ReadRaw(TTP1_SDO_PORT, TTP1_SDO_PIN,
                        TTP1_SCL_PORT, TTP1_SCL_PIN, 0xFFFF);
        TTP229_ReadRaw(TTP2_SDO_PORT, TTP2_SDO_PIN,
                        TTP2_SCL_PORT, TTP2_SCL_PIN, 0xFFFF);
        Delay_Ms(40);  /* wait for TTP229 scan cycle (~32ms) */
    }

    /* Capture baseline from final read */
    r1 = TTP229_ReadRaw(TTP1_SDO_PORT, TTP1_SDO_PIN,
                          TTP1_SCL_PORT, TTP1_SCL_PIN, 0xFFFF);
    Delay_Ms(40);
    r2 = TTP229_ReadRaw(TTP2_SDO_PORT, TTP2_SDO_PIN,
                          TTP2_SCL_PORT, TTP2_SCL_PIN, 0xFFFF);

    baseline_raw1 = r1;
    baseline_raw2 = r2;
    cached_raw1 = r1;
    cached_raw2 = r2;
}

void TTP229_Read(uint8_t key_bitmap[TTP_BITS_BYTES])
{
    uint16_t raw1, raw2;
    uint8_t i, bmp_bit;

    key_bitmap[0] = 0;
    key_bitmap[1] = 0;
    key_bitmap[2] = 0;

    /* Read raw 16-bit data from both chips (non-blocking with cache) */
    raw1 = TTP229_ReadRaw(TTP1_SDO_PORT, TTP1_SDO_PIN,
                           TTP1_SCL_PORT, TTP1_SCL_PIN, cached_raw1);
    cached_raw1 = raw1;  /* Update cache */

    raw2 = TTP229_ReadRaw(TTP2_SDO_PORT, TTP2_SDO_PIN,
                           TTP2_SCL_PORT, TTP2_SCL_PIN, cached_raw2);
    cached_raw2 = raw2;  /* Update cache */

    /* Calibrate: cancel parasitic bits using startup baseline.
     * In active-low mode, untouched=1, touched=0.
     * Parasitic bits are 0 in the baseline (false touch from PCB).
     * A real touch only matters if the baseline bit was 1 (clean pin).
     * Formula: result = (~raw) & baseline
     *   baseline=1, raw=1 (untouched) → result=0 (not touched)
     *   baseline=1, raw=0 (touched)   → result=1 (TOUCHED)
     *   baseline=0, raw=0 (parasitic) → result=0 (ignored) */
    raw1 = (~raw1) & baseline_raw1;
    raw2 = (~raw2) & baseline_raw2;

    /* Mask out chip1 unused pins (TP4~TP11 = bits 4~11) */
    raw1 &= CHIP1_USED_MASK;

    /* Map chip #1 bits to key bitmap
     * After calibration: bit=1 means TOUCHED, bit=0 means not touched */
    for (i = 0; i < 16; i++)
    {
        if (raw1 & (1u << (15 - i)))  /* calibrated: 1 = touched */
        {
            bmp_bit = chip1_map[i];
            if (bmp_bit != 0xFF)
            {
                key_bitmap[bmp_bit >> 3] |= (1u << (bmp_bit & 7));
            }
        }
    }

    /* Map chip #2 bits to key bitmap (calibrated: 1 = touched) */
    for (i = 0; i < 16; i++)
    {
        if (raw2 & (1u << (15 - i)))
        {
            bmp_bit = chip2_map[i];
            key_bitmap[bmp_bit >> 3] |= (1u << (bmp_bit & 7));
        }
    }
}

void TTP229_GetRaw(uint16_t *raw1, uint16_t *raw2)
{
    if (raw1) *raw1 = cached_raw1;
    if (raw2) *raw2 = cached_raw2;
}
