/*********************************************************************
 * File Name          : ws2812.c
 * Description        : WS2812 LED driver using SPI0 Master MOSI.
 *                      SPI0 remapped to PB12(SCK)/PB14(MOSI).
 *                      2.5 MHz SPI clock encodes WS2812 timing:
 *                        bit '1' = 110  bit '0' = 100
 *********************************************************************/

#include "ws2812.h"

/* ==================================================================== */
/*  Internal buffers                                                    */
/* ==================================================================== */

/* LED color buffer (RGB888) */
static rgb888_t s_led_buf[WS2812_LED_COUNT];

/* SPI encoded output buffer */
static uint8_t s_spi_buf[WS2812_SPI_BUF_SIZE];

/* ==================================================================== */
/*  4-bit nibble to 12-bit SPI pattern lookup table                     */
/*  WS2812 bit '1' -> SPI 110, bit '0' -> SPI 100                     */
/*  4 WS2812 bits -> 12 SPI bits = 1.5 bytes                          */
/* ==================================================================== */

/* Encoding: nibble bit3..bit0 -> SPI pattern (MSB first)
 * bit3: 1->110, 0->100
 * bit2: 1->110, 0->100
 * bit1: 1->110, 0->100
 * bit0: 1->110, 0->100
 * Total 12 bits, stored as [byte_hi:8bits][byte_lo:4bits in high nibble]
 */
static const uint16_t s_nibble_lut[16] = {
    /* 0b0000 */ 0x924,  /* 100 100 100 100 */
    /* 0b0001 */ 0x926,  /* 100 100 100 110 */
    /* 0b0010 */ 0x934,  /* 100 100 110 100 */
    /* 0b0011 */ 0x936,  /* 100 100 110 110 */
    /* 0b0100 */ 0x9A4,  /* 100 110 100 100 */
    /* 0b0101 */ 0x9A6,  /* 100 110 100 110 */
    /* 0b0110 */ 0x9B4,  /* 100 110 110 100 */
    /* 0b0111 */ 0x9B6,  /* 100 110 110 110 */
    /* 0b1000 */ 0xD24,  /* 110 100 100 100 */
    /* 0b1001 */ 0xD26,  /* 110 100 100 110 */
    /* 0b1010 */ 0xD34,  /* 110 100 110 100 */
    /* 0b1011 */ 0xD36,  /* 110 100 110 110 */
    /* 0b1100 */ 0xDA4,  /* 110 110 100 100 */
    /* 0b1101 */ 0xDA6,  /* 110 110 100 110 */
    /* 0b1110 */ 0xDB4,  /* 110 110 110 100 */
    /* 0b1111 */ 0xDB6,  /* 110 110 110 110 */
};

/* ==================================================================== */
/*  Internal: Encode one LED's GRB data into 9 SPI bytes               */
/* ==================================================================== */

static void EncodePixel(uint8_t g, uint8_t r, uint8_t b, uint8_t *out)
{
    /* WS2812 expects GRB order, MSB first */
    /* 24 bits = 6 nibbles, each nibble -> 12 SPI bits (1.5 bytes) */
    uint8_t nibbles[6];
    uint8_t i;
    uint16_t pat;

    nibbles[0] = (g >> 4) & 0x0F;
    nibbles[1] =  g       & 0x0F;
    nibbles[2] = (r >> 4) & 0x0F;
    nibbles[3] =  r       & 0x0F;
    nibbles[4] = (b >> 4) & 0x0F;
    nibbles[5] =  b       & 0x0F;

    /* Pack 6 nibbles (6 x 12 bits = 72 bits = 9 bytes) */
    for (i = 0; i < 3; i++) {
        uint16_t hi = s_nibble_lut[nibbles[i * 2]];
        uint16_t lo = s_nibble_lut[nibbles[i * 2 + 1]];
        /* hi is 12 bits, lo is 12 bits -> 24 bits = 3 bytes */
        out[i * 3 + 0] = (uint8_t)(hi >> 4);           /* hi[11:4] */
        out[i * 3 + 1] = (uint8_t)((hi & 0x0F) << 4) | (uint8_t)(lo >> 8);  /* hi[3:0] | lo[11:8] */
        out[i * 3 + 2] = (uint8_t)(lo & 0xFF);          /* lo[7:0] */
    }
}

/* ==================================================================== */
/*  Internal: Send buffer via SPI0 FIFO                                 */
/* ==================================================================== */

static void SPI0_SendBuffer(const uint8_t *buf, uint16_t len)
{
    uint16_t sendlen = len;

    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;   /* FIFO direction = TX */
    R16_SPI0_TOTAL_CNT = sendlen;            /* 设置总发送字节数 */
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END;    /* 清除计数结束标志 */

    while (sendlen) {
        if (R8_SPI0_FIFO_COUNT < SPI_FIFO_SIZE) {
            R8_SPI0_FIFO = *buf;
            buf++;
            sendlen--;
        }
    }

    /* 等待 FIFO 中所有数据发送完毕 */
    while (R8_SPI0_FIFO_COUNT != 0)
        ;
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void WS2812_Init(void)
{
    /* Enable SPI0 pin remap: PB12(SCK), PB13(MISO), PB14(MOSI), PB15(NSS) */
    GPIOPinRemap(ENABLE, RB_PIN_SPI0);

    /* Configure SPI0 pins */
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeOut_PP_5mA);   /* SCK */
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeIN_PU);         /* MISO (unused but required) */
    GPIOB_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);   /* MOSI -> WS2812 DIN */
    GPIOB_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_5mA);   /* NSS */
    GPIOBDigitalCfg(ENABLE, GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15);

    /* NSS always low (we're the only master) */
    GPIOB_ResetBits(GPIO_Pin_15);

    /* Reset SPI0 */
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;

    /* Master mode, MOSI output enable, SCK output enable */
    R8_SPI0_CTRL_MOD = RB_SPI_MOSI_OE | RB_SPI_SCK_OE;

    /* Mode 0, MSB first */
    SPI0_DataMode(Mode0_HighBitINFront);

    /* Auto-clear interrupt flags */
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;

    /* FIFO direction = TX */
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;

    /* SPI clock = SYSCLK / 24 = 60MHz / 24 = 2.5MHz */
    SPI0_CLKCfg(24);

    /* Clear LED buffer */
    memset(s_led_buf, 0, sizeof(s_led_buf));

    /* Clear SPI buffer (reset bytes are all 0) */
    memset(s_spi_buf, 0, sizeof(s_spi_buf));
}

void WS2812_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < WS2812_LED_COUNT) {
        s_led_buf[index].r = r;
        s_led_buf[index].g = g;
        s_led_buf[index].b = b;
    }
}

void WS2812_SetAllPixels(const rgb888_t *colors)
{
    uint8_t i;
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        s_led_buf[i] = colors[i];
    }
}

void WS2812_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t i;
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        s_led_buf[i].r = r;
        s_led_buf[i].g = g;
        s_led_buf[i].b = b;
    }
}

void WS2812_Refresh(void)
{
    uint8_t i;

    /* Encode all LEDs into SPI buffer */
    for (i = 0; i < WS2812_LED_COUNT; i++) {
        EncodePixel(s_led_buf[i].g, s_led_buf[i].r, s_led_buf[i].b,
                    &s_spi_buf[i * WS2812_SPI_BYTES_PER_LED]);
    }

    /* Reset bytes are already zeroed at init; re-clear for safety */
    memset(&s_spi_buf[WS2812_LED_COUNT * WS2812_SPI_BYTES_PER_LED],
           0, WS2812_RESET_BYTES);

    /* Send entire buffer via SPI0 */
    SPI0_SendBuffer(s_spi_buf, WS2812_SPI_BUF_SIZE);
}

void WS2812_Clear(void)
{
    memset(s_led_buf, 0, sizeof(s_led_buf));
    WS2812_Refresh();
}

rgb888_t *WS2812_GetBuffer(void)
{
    return s_led_buf;
}
