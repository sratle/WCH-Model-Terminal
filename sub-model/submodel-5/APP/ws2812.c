/*********************************************************************
 * File Name          : ws2812.c
 * Description        : WS2812 LED driver using SPI0 Master MOSI.
 *                      SPI0 remapped to PB12(SCK)/PB14(MOSI).
 *                      7.5 MHz SPI clock (60MHz/8) encodes WS2812 timing:
 *                        10 SPI bits per WS2812 bit:
 *                        bit '1' = 1111100000  (T1H=667ns, T1L=667ns)
 *                        bit '0' = 1100000000  (T0H=267ns, T0L=1067ns)
 *                      Total bit period = 1.33us (WS2812B: 1.25us +/- 600ns)
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
/*  4-bit nibble to 40-bit SPI pattern lookup table                     */
/*  WS2812 bit '1' -> SPI 1111110000, bit '0' -> SPI 1110000000       */
/*  4 WS2812 bits -> 40 SPI bits = 5 bytes                            */
/* ==================================================================== */

/*
 * Encoding: nibble b3 b2 b1 b0 (MSB first)
 * Each bit maps to 10 SPI bits:
 *   '1' -> 1111110000 (0x3F0)
 *   '0' -> 1110000000 (0x380)
 * 4 bits -> 40 bits, stored as 5 bytes [byte0]..[byte4]
 *
 * Timing at 7.5MHz (133ns/SPI bit):
 *   bit '1': T1H = 6*133 = 800ns, T1L = 4*133 = 533ns
 *   bit '0': T0H = 3*133 = 400ns, T0L = 7*133 = 933ns
 */
static const uint8_t s_nibble_lut[16][5] = {
    /* 0b0000 */ { 0xE0, 0x38, 0x0E, 0x03, 0x80 },
    /* 0b0001 */ { 0xE0, 0x38, 0x0E, 0x03, 0xF0 },
    /* 0b0010 */ { 0xE0, 0x38, 0x0F, 0xC3, 0x80 },
    /* 0b0011 */ { 0xE0, 0x38, 0x0F, 0xC3, 0xF0 },
    /* 0b0100 */ { 0xE0, 0x3F, 0x0E, 0x03, 0x80 },
    /* 0b0101 */ { 0xE0, 0x3F, 0x0E, 0x03, 0xF0 },
    /* 0b0110 */ { 0xE0, 0x3F, 0x0F, 0xC3, 0x80 },
    /* 0b0111 */ { 0xE0, 0x3F, 0x0F, 0xC3, 0xF0 },
    /* 0b1000 */ { 0xFC, 0x38, 0x0E, 0x03, 0x80 },
    /* 0b1001 */ { 0xFC, 0x38, 0x0E, 0x03, 0xF0 },
    /* 0b1010 */ { 0xFC, 0x38, 0x0F, 0xC3, 0x80 },
    /* 0b1011 */ { 0xFC, 0x38, 0x0F, 0xC3, 0xF0 },
    /* 0b1100 */ { 0xFC, 0x3F, 0x0E, 0x03, 0x80 },
    /* 0b1101 */ { 0xFC, 0x3F, 0x0E, 0x03, 0xF0 },
    /* 0b1110 */ { 0xFC, 0x3F, 0x0F, 0xC3, 0x80 },
    /* 0b1111 */ { 0xFC, 0x3F, 0x0F, 0xC3, 0xF0 },
};

/* ==================================================================== */
/*  Internal: Encode one LED's GRB data into 30 SPI bytes              */
/* ==================================================================== */

static void EncodePixel(uint8_t g, uint8_t r, uint8_t b, uint8_t *out)
{
    /* WS2812 expects GRB order, MSB first */
    /* 24 bits = 6 nibbles, each nibble -> 5 SPI bytes */
    uint8_t nibbles[6];
    uint8_t i, n;

    nibbles[0] = (g >> 4) & 0x0F;
    nibbles[1] =  g       & 0x0F;
    nibbles[2] = (r >> 4) & 0x0F;
    nibbles[3] =  r       & 0x0F;
    nibbles[4] = (b >> 4) & 0x0F;
    nibbles[5] =  b       & 0x0F;

    for (i = 0; i < 6; i++) {
        n = nibbles[i];
        out[i * 5 + 0] = s_nibble_lut[n][0];
        out[i * 5 + 1] = s_nibble_lut[n][1];
        out[i * 5 + 2] = s_nibble_lut[n][2];
        out[i * 5 + 3] = s_nibble_lut[n][3];
        out[i * 5 + 4] = s_nibble_lut[n][4];
    }
}

/* ==================================================================== */
/*  Internal: Send buffer via SPI0 FIFO                                 */
/*  UART0 RX 现在由中断驱动，SPI 发送期间不需要轮询 UART。              */
/*  这确保 SPI 时序连续不断，WS2812 不会因间隙而误判为 reset。          */
/* ==================================================================== */

static void SPI0_SendBuffer(const uint8_t *buf, uint16_t len)
{
    uint16_t sendlen = len;

    /* 禁用 UART0 RX 中断，防止 ISR 抢占 CPU 导致 SPI FIFO 欠载。
     * FIFO 欠载会使 MOSI 出现空闲间隙，WS2812 误判为 reset，
     * 导致只有前几个 LED 正确显示，其余全黑。
     * 中断期间到达的字节仍由硬件存入 ring buffer，不会丢失。 */
    PFIC_DisableIRQ(UART0_IRQn);

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

    /* 等待所有字节完全移位发送完毕（含最后一个字节） */
    while (!(R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END))
        ;

    /* 恢复 UART0 RX 中断 */
    PFIC_EnableIRQ(UART0_IRQn);
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void WS2812_Init(void)
{
    /* 1. 禁用可能与 SPI0 PB12-15 冲突的复用功能 */
    GPIOPinRemap(DISABLE, RB_PIN_I2C);      /* PB12/PB13 不用于 I2C */
    GPIOPinRemap(DISABLE, RB_PIN_MODEM);    /* PB14/PB15 不用于 MODEM */
    GPIOPinRemap(DISABLE, RB_PIN_UART1);    /* PB12/PB13 不用于 UART1 */

    /* 2. Enable SPI0 pin remap: PB12(SCK), PB13(MISO), PB14(MOSI), PB15(NSS) */
    GPIOPinRemap(ENABLE, RB_PIN_SPI0);

    /* 3. 禁用 USB2 对 PB12/PB13 的占用 */
    R16_PIN_CONFIG &= ~RB_PIN_USB2_EN;

    /* 4. Configure GPIO direction (SPI 外设会接管输出) */
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeOut_PP_20mA);  /* SCK - SPI 驱动 */
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeIN_PU);         /* MISO - 未使用 */
    GPIOB_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_20mA);  /* MOSI -> WS2812 DIN */
    GPIOB_ModeCfg(GPIO_Pin_15, GPIO_ModeOut_PP_20mA);  /* NSS */

    /* 5. 使能 PB12-15 数字输入（SPI 外设需要读取引脚状态） */
    R32_PIN_IN_DIS &= ~((uint32_t)(GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15) << 16);

    /* 6. SPI0 完整初始化（参照 WCH SPI0_MasterDefInit） */
    R8_SPI0_CLOCK_DIV = 8;                              /* 先设分频: 60MHz/8 = 7.5MHz */
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;                /* 清除 FIFO 和计数器 */
    R8_SPI0_CTRL_MOD = RB_SPI_MOSI_OE | RB_SPI_SCK_OE; /* Master: MOSI+SCK 输出使能 */
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;                 /* 写 FIFO 自动清除 BYTE_END 标志 */
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;             /* 关闭 DMA */
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;               /* FIFO 方向 = TX */

    /* 7. Mode 0, MSB first */
    R8_SPI0_CTRL_MOD &= ~RB_SPI_MST_SCK_MOD;            /* CPOL=0, CPHA=0 */
    R8_SPI0_CTRL_CFG &= ~RB_SPI_BIT_ORDER;               /* MSB first */

    /* 8. NSS 拉低（我们只用 MOSI，不需要 NSS） */
    GPIOB_ResetBits(GPIO_Pin_15);

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

    /* Reset bytes: all zeros (MOSI stays low > 280us for WS2812B reset) */
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
