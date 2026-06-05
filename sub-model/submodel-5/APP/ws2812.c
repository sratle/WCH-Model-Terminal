/*********************************************************************
 * File Name          : ws2812.c
 * Description        : WS2812 LED driver using GPIO bit-banging on PB14.
 *                      Single inline assembly block for bit transmission
 *                      to prevent compiler optimization of NOP timing.
 *
 *                      Timing @ 62.4 MHz (user-adjusted for MCU delay):
 *                        T0H = 250ns (15 nop)   T0L = 750ns (47 nop)
 *                        T1H = 750ns (47 nop)   T1L = 250ns (15 nop)
 *                        Reset: LOW > 300µs
 *                        Bit period: 1µs
 *********************************************************************/

#include "ws2812.h"

/* ==================================================================== */
/*  Internal buffers                                                    */
/* ==================================================================== */
static rgb888_t s_led_buf[WS2812_LED_COUNT];

/* ==================================================================== */
/*  WS2812 bit-bang: single inline assembly block                       */
/*  编译器无法重排或优化 asm volatile 块内的指令                           */
/*  改 NOP 数量即可调整时序，立竿见影                                      */
/* ==================================================================== */
static inline void send_bit(uint8_t bit) __attribute__((always_inline));
static inline void send_bit(uint8_t bit)
{
    __asm__ volatile (
        /* PB14 = HIGH */
        "li   a4, 0x400010D8  \n"  /* PB_SET 寄存器地址 */
        "li   a5, 0x4000      \n"  /* GPIO_Pin_14 掩码 */
        "sw   a5, 0(a4)       \n"  /* PB14 = HIGH */

        "bnez %[b], 1f        \n"  /* bit==1 → T1H 路径 */

        /* ---- bit 0: T0H ≈ 250ns (15 nop) ---- */
        "nop\n"
        /* PB14 = LOW */
        "li   a4, 0x400010CC  \n"  /* PB_CLR 寄存器地址 */
        "sw   a5, 0(a4)       \n"  /* PB14 = LOW */
        /* T0L ≈ 750ns (47 nop) */
        "nop\nnop\nnop\n"
        "j    2f              \n"  /* 跳到结束 */

        /* ---- bit 1: T1H ≈ 750ns (47 nop) ---- */
        "1:                   \n"
        "nop\nnop\nnop\n"
        /* PB14 = LOW */
        "li   a4, 0x400010CC  \n"  /* PB_CLR 寄存器地址 */
        "sw   a5, 0(a4)       \n"  /* PB14 = LOW */
        /* T1L ≈ 250ns (15 nop) */
        "nop\n"

        "2:                   \n"  /* 结束 */
        : /* no output */
        : [b] "r" (bit)
        : "a4", "a5", "memory"
    );
}

/* ==================================================================== */
/*  Send one byte (MSB first)                                           */
/* ==================================================================== */
static inline void send_byte(uint8_t byte) __attribute__((always_inline));
static inline void send_byte(uint8_t byte)
{
    send_bit((byte >> 7) & 1);
    send_bit((byte >> 6) & 1);
    send_bit((byte >> 5) & 1);
    send_bit((byte >> 4) & 1);
    send_bit((byte >> 3) & 1);
    send_bit((byte >> 2) & 1);
    send_bit((byte >> 1) & 1);
    send_bit((byte >> 0) & 1);
}

/* ==================================================================== */
/*  Send one LED: 24-bit GRB                                            */
/* ==================================================================== */
static inline void send_led(uint8_t g, uint8_t r, uint8_t b) __attribute__((always_inline));
static inline void send_led(uint8_t g, uint8_t r, uint8_t b)
{
    send_byte(g);
    send_byte(r);
    send_byte(b);
}

/* ==================================================================== */
/*  Reset signal: LOW > 300µs                                           */
/* ==================================================================== */
static void send_reset(void)
{
    /* PB14 = LOW via PB_CLR */
    *(volatile uint32_t *)0x400010CC = 0x4000;

    /* ~960µs delay (well above 300µs minimum) */
    {
        volatile uint32_t d = 20000;
        do { __asm__ volatile("nop"); } while (--d);
    }
}

/* ==================================================================== */
/*  Public API                                                          */
/* ==================================================================== */

void WS2812_Init(void)
{
    /* 禁用可能与 PB14 冲突的复用功能 */
    GPIOPinRemap(DISABLE, RB_PIN_MODEM);
    GPIOPinRemap(DISABLE, RB_PIN_SPI0);
    R16_PIN_CONFIG &= ~RB_PIN_USB2_EN;

    /* PB14 推挽输出，初始 LOW */
    *(volatile uint32_t *)0x400010CC = 0x4000;  /* PB14 = LOW */
    GPIOB_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_20mA);

    /* Reset WS2812 (>300µs LOW) */
    send_reset();

    /* 发送全 bit-0 数据帧（49×24bit GRB = 0），让 WS2812 锁存全黑 */
    {
        uint8_t i;
        uint32_t irq_state;
        PFIC_DisableIRQ(UART0_IRQn);
        irq_state = __risc_v_disable_irq();

        for (i = 0; i < WS2812_LED_COUNT; i++) {
            send_led(0, 0, 0);   /* G=0, R=0, B=0 — 全 bit-0 */
        }
        send_reset();

        __risc_v_enable_irq(irq_state);
        PFIC_EnableIRQ(UART0_IRQn);
    }

    /* 清 LED 缓冲 */
    memset(s_led_buf, 0, sizeof(s_led_buf));
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
    uint32_t irq_state;

    /* WS2812 传输期间禁用中断，保证时序精确 */
    PFIC_DisableIRQ(UART0_IRQn);
    irq_state = __risc_v_disable_irq();

    for (i = 0; i < WS2812_LED_COUNT; i++) {
        send_led(s_led_buf[i].g, s_led_buf[i].r, s_led_buf[i].b);
    }

    /* Reset: LOW > 300µs，也作为帧间间隔的低电平 */
    send_reset();

    /* 恢复中断 */
    __risc_v_enable_irq(irq_state);
    PFIC_EnableIRQ(UART0_IRQn);
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
