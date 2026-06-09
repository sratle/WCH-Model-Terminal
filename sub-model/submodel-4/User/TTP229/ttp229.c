#include "ttp229.h"
#include "debug.h"

/*
 * TTP229-BSF serial interface timing:
 *   DV (data valid) typical: 93 us
 *   SCL min pulse width (Tw): 10 us
 *   SCL timeout: 2 ms
 *   Response time (16-key mode): ~32 ms
 */

#define TTP229_SCL_DELAY_US  10   /* SCL pulse half-period (Tw >= 10us) */
#define TTP229_DV_TIMEOUT    5000 /* DV timeout (~278us at 72MHz).
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
static uint16_t cached_raw = 0xFFFF;  /* all keys untouched (active-low) */

/* Calibration baseline: captured at startup when no keys are touched.
 * bit=1 means untouched, bit=0 means parasitic. */
static uint16_t baseline_raw = 0xFFFF;

/* ======================== Raw 16-bit Read ======================== */

/*
 * Read 16-bit raw data from TTP229.
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
static uint16_t TTP229_ReadRaw(uint16_t cached)
{
    uint32_t raw32 = 0;  /* 32-bit to hold 17 bits during shifting */
    uint32_t timeout;
    uint8_t i;

    /* Wait for DV (SDO goes LOW = data valid) */
    timeout = TTP229_DV_TIMEOUT;
    while (!(TTP_SDO_PORT->INDR & TTP_SDO_PIN))
    {
        if (--timeout == 0)
            return cached;  /* DV not ready, use cached data */
    }

    /* Clock 17 bits (Tw >= 10us per half-period).
     * D0 (padding) is read first, then D1~D16 (TP0~TP15).
     * The padding bit overflows out of the 16-bit result. */
    for (i = 0; i < 17; i++)
    {
        SCL_High(TTP_SCL_PORT, TTP_SCL_PIN);
        Delay_Us(TTP229_SCL_DELAY_US);
        raw32 <<= 1;
        raw32 |= SDO_Read(TTP_SDO_PORT, TTP_SDO_PIN);
        SCL_Low(TTP_SCL_PORT, TTP_SCL_PIN);
        Delay_Us(TTP229_SCL_DELAY_US);
    }

    return (uint16_t)(raw32 & 0xFFFF);
}

/* ======================== TP→Key Mapping ======================== */

/*
 * key_tp_bit[i] = raw bit index for Key (i+1)
 *
 * TTP229 outputs 17 bits: padding + TP0~TP15.
 * With 17 SCL clocks, padding overflows, leaving:
 *   raw[15] = TP0, raw[14] = TP1, ..., raw[0] = TP15
 *   Mapping: TPn → raw bit (15 - n)
 *
 * Key mapping (from Submodel-4.md):
 *   Key 1  = TP9  → bit 6     Key 9  = TP0  → bit 15
 *   Key 2  = TP11 → bit 4     Key 10 = TP15 → bit 0
 *   Key 3  = TP8  → bit 7     Key 11 = TP14 → bit 1
 *   Key 4  = TP10 → bit 5     Key 12 = TP7  → bit 8
 *   Key 5  = TP13 → bit 2     Key 13 = TP6  → bit 9
 *   Key 6  = TP3  → bit 12    Key 14 = TP5  → bit 10
 *   Key 7  = TP2  → bit 13    Key 15 = TP4  → bit 11
 *   Key 8  = TP1  → bit 14    Key 16 = TP12 → bit 3
 */
static const uint8_t key_tp_bit[16] = {
     6,   /* Key 1  = TP9  → bit 6  */
     4,   /* Key 2  = TP11 → bit 4  */
     7,   /* Key 3  = TP8  → bit 7  */
     5,   /* Key 4  = TP10 → bit 5  */
     2,   /* Key 5  = TP13 → bit 2  */
    12,   /* Key 6  = TP3  → bit 12 */
    13,   /* Key 7  = TP2  → bit 13 */
    14,   /* Key 8  = TP1  → bit 14 */
    15,   /* Key 9  = TP0  → bit 15 */
     0,   /* Key 10 = TP15 → bit 0  */
     1,   /* Key 11 = TP14 → bit 1  */
     8,   /* Key 12 = TP7  → bit 8  */
     9,   /* Key 13 = TP6  → bit 9  */
    10,   /* Key 14 = TP5  → bit 10 */
    11,   /* Key 15 = TP4  → bit 11 */
     3    /* Key 16 = TP12 → bit 3  */
};

/* ======================== Public API ======================== */

void TTP229_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* Enable GPIOB clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* TTP229 SDO (PB15) - input floating */
    gpio.GPIO_Pin   = TTP_SDO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(TTP_SDO_PORT, &gpio);

    /* TTP229 SCL (PB14) - push-pull output, idle low */
    gpio.GPIO_Pin   = TTP_SCL_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(TTP_SCL_PORT, &gpio);
    SCL_Low(TTP_SCL_PORT, TTP_SCL_PIN);
}

/*
 * Calibrate TTP229 baseline after power-on stabilization.
 * Call once after the 500ms startup delay.
 */
void TTP229_Calibrate(void)
{
    uint8_t attempt;
    uint16_t r;

    /* Discard first few reads to let the shift register stabilize */
    for (attempt = 0; attempt < 4; attempt++)
    {
        TTP229_ReadRaw(0xFFFF);
        Delay_Ms(40);  /* wait for TTP229 scan cycle (~32ms) */
    }

    /* Capture baseline from final read */
    r = TTP229_ReadRaw(0xFFFF);
    Delay_Ms(40);
    r = TTP229_ReadRaw(r);  /* second read for stability */

    baseline_raw = r;
    cached_raw   = r;
}

/*
 * Read TTP229 and return calibrated 16-bit touch bitmap.
 * Returns: bitmap where bit i = 1 means Key (i+1) is touched.
 */
uint16_t TTP229_Read(void)
{
    uint16_t raw, bitmap;
    uint8_t i;

    /* Read raw 16-bit data (non-blocking with cache) */
    raw = TTP229_ReadRaw(cached_raw);
    cached_raw = raw;

    /* Calibrate: cancel parasitic bits using startup baseline.
     * result = (~raw) & baseline */
    bitmap = (~raw) & baseline_raw;

    /* Remap TP bits to key bitmap */
    {
        uint16_t key_bmp = 0;
        for (i = 0; i < TTP_KEY_COUNT; i++)
        {
            if (bitmap & (1u << key_tp_bit[i]))
                key_bmp |= (1u << i);
        }
        return key_bmp;
    }
}

uint16_t TTP229_GetRaw(void)
{
    return cached_raw;
}
