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
#define TTP229_DV_TIMEOUT    200 /* Short DV timeout (~200us at 72MHz)
                                  * TTP229 DV typical: 93us, response: ~32ms.
                                  * If DV not ready, use cached data. */

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

/* ======================== Raw 16-bit Read ======================== */

/*
 * Read 16-bit raw data from one TTP229 chip.
 * Waits for SDO (DV) with a SHORT timeout.
 * If DV not ready, returns cached value (non-blocking).
 */
static uint16_t TTP229_ReadRaw(GPIO_TypeDef *sdo_port, uint16_t sdo_pin,
                                GPIO_TypeDef *scl_port, uint16_t scl_pin,
                                uint16_t cached)
{
    uint16_t raw = 0;
    uint32_t timeout;
    uint8_t i;

    /* Wait for DV with short timeout (~200us) */
    timeout = TTP229_DV_TIMEOUT;
    while (!(sdo_port->INDR & sdo_pin))
    {
        if (--timeout == 0)
            return cached;  /* DV not ready, use cached data */
    }

    /* Clock 16 bits (Tw >= 10us per half-period) */
    for (i = 0; i < 16; i++)
    {
        SCL_High(scl_port, scl_pin);
        Delay_Us(TTP229_SCL_DELAY_US);
        raw <<= 1;
        raw |= SDO_Read(sdo_port, sdo_pin);
        SCL_Low(scl_port, scl_pin);
        Delay_Us(TTP229_SCL_DELAY_US);
    }

    return raw;
}

/* ======================== TP→Key Mapping ======================== */

/*
 * TTP229 #1 reverse lookup: chip bit position → key bitmap bit
 *
 * Chip #1 mapping:
 *   bit 0 (TP0)  → Key 21 (bit 20)
 *   bit 1 (TP1)  → Key 22 (bit 21)
 *   bit 2 (TP2)  → Key 23 (bit 22)
 *   bit 3 (TP3)  → Key 24 (bit 23)
 *   bit 4~11     → unused (TP4~TP11 not connected)
 *   bit 12 (TP12) → Key 17 (bit 16)
 *   bit 13 (TP13) → Key 18 (bit 17)
 *   bit 14 (TP14) → Key 19 (bit 18)
 *   bit 15 (TP15) → Key 20 (bit 19)
 */
static const uint8_t chip1_map[16] = {
    20, 21, 22, 23,  /* bit 0~3  → bitmap bit 20~23 */
    0xFF, 0xFF, 0xFF, 0xFF,  /* bit 4~7  → unused */
    0xFF, 0xFF, 0xFF, 0xFF,  /* bit 8~11 → unused */
    16, 17, 18, 19   /* bit 12~15 → bitmap bit 16~19 */
};

/*
 * TTP229 #2 reverse lookup: chip bit position → key bitmap bit
 *
 * Chip #2 mapping:
 *   bit 0  (TP16) → Key 5  (bit 4)
 *   bit 1  (TP17) → Key 6  (bit 5)
 *   bit 2  (TP18) → Key 7  (bit 6)
 *   bit 3  (TP19) → Key 8  (bit 7)
 *   bit 4  (TP20) → Key 16 (bit 15)
 *   bit 5  (TP21) → Key 15 (bit 14)
 *   bit 6  (TP22) → Key 14 (bit 13)
 *   bit 7  (TP23) → Key 13 (bit 12)
 *   bit 8  (TP24) → Key 12 (bit 11)
 *   bit 9  (TP25) → Key 11 (bit 10)
 *   bit 10 (TP26) → Key 10 (bit 9)
 *   bit 11 (TP27) → Key 9  (bit 8)
 *   bit 12 (TP28) → Key 1  (bit 0)
 *   bit 13 (TP29) → Key 2  (bit 1)
 *   bit 14 (TP30) → Key 3  (bit 2)
 *   bit 15 (TP31) → Key 4  (bit 3)
 */
static const uint8_t chip2_map[16] = {
    4,  5,  6,  7,   /* bit 0~3  → bitmap bit 4~7 */
    15, 14, 13, 12,  /* bit 4~7  → bitmap bit 15,14,13,12 */
    11, 10, 9,  8,   /* bit 8~11 → bitmap bit 11,10,9,8 */
    0,  1,  2,  3    /* bit 12~15 → bitmap bit 0~3 */
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

    /* Map chip #1 bits to key bitmap
     * TTP229-BSF default: active LOW (bit=0 means touched) */
    for (i = 0; i < 16; i++)
    {
        if (!(raw1 & (1u << (15 - i))))
        {
            bmp_bit = chip1_map[i];
            if (bmp_bit != 0xFF)
            {
                key_bitmap[bmp_bit >> 3] |= (1u << (bmp_bit & 7));
            }
        }
    }

    /* Map chip #2 bits to key bitmap */
    for (i = 0; i < 16; i++)
    {
        if (!(raw2 & (1u << (15 - i))))
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
