/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_rgb.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : WS2812 bit-banged driver for CH32V307 @ 144 MHz.
*                      Timing reference: sub-model/submodel-5 (ws2812.c),
*                      NOP counts retuned for 144 MHz (1 cycle ~ 6.94 ns).
*
*                      WS2812 bit timing (us, +/-0.15 tolerance):
*                          T0H = 0.40   T0L = 0.85
*                          T1H = 0.80   T1L = 0.45
*                          RES = low > 280
********************************************************************************/
#include "bsp_rgb.h"
#include "ch32v30x.h"
#include "debug.h"              /* Delay_Us */

/*=============================================================================
 *  Pin / Register Map
 *=============================================================================*/

#define RGB_PIN         GPIO_Pin_0      /* PD0 - WS2812 DIN              */

/* GPIOD atomic set/clear registers (address baked into the asm below) */
#define GPIOD_BSHR_ADDR 0x40011410      /* GPIOD base 0x40011400 + 0x10  */
#define GPIOD_BCR_ADDR  0x40011414      /* GPIOD base 0x40011400 + 0x14  */

/*=============================================================================
 *  Module State
 *=============================================================================*/

static rgb_color_t s_leds[RGB_LED_COUNT];
static uint8_t     s_brightness = 255;

/*=============================================================================
 *  Low-level Bit Bang
 *=============================================================================*/

/* NOP padding: at 144 MHz one NOP ~ 6.94 ns.
 * High/low windows below include the li/sw/branch overhead (~8 cycles),
 * giving T0H~0.36us/T0L~0.83us and T1H~0.74us/T1L~0.45us. */

/*********************************************************************
 * @fn      RGB_SendBit
 *
 * @brief   Emit one WS2812 bit on PD0. All timing is inside a single
 *          volatile asm block so the compiler cannot reorder it.
 *
 * @param   bit - 0 or 1
 *
 * @return  none
 */
static inline __attribute__((always_inline)) void RGB_SendBit(uint8_t bit)
{
    __asm__ volatile (
        "li   a4, %[bshr]          \n"  /* PD0 = HIGH (T0H/T1H start)  */
        "li   a5, 0x1              \n"
        "sw   a5, 0(a4)            \n"
        "bnez %[b], 1f             \n"

        /* ---- bit 0: T0H ~ 0.36us ---- */
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "nop\nnop\nnop\nnop\nnop\nnop\nnop\n"
        "li   a4, %[bcr]           \n"  /* PD0 = LOW                   */
        "sw   a5, 0(a4)            \n"
        /* T0L ~ 0.83us (112 NOP) */
        ".rept 112\n nop\n .endr   \n"
        "j    2f                   \n"

        /* ---- bit 1: T1H ~ 0.74us ---- */
        "1:                        \n"
        ".rept 88\n nop\n .endr    \n"
        "li   a4, %[bcr]           \n"  /* PD0 = LOW                   */
        "sw   a5, 0(a4)            \n"
        /* T1L ~ 0.45us (58 NOP) */
        ".rept 58\n nop\n .endr    \n"

        "2:                        \n"
        :
        : [b] "r" (bit), [bshr] "i" (GPIOD_BSHR_ADDR), [bcr] "i" (GPIOD_BCR_ADDR)
        : "a4", "a5", "memory"
    );
}

/*********************************************************************
 * @fn      RGB_SendByte
 *
 * @brief   Send one byte, MSB first (WS2812 wire order).
 *
 * @return  none
 */
static void RGB_SendByte(uint8_t byte)
{
    int8_t i;
    for (i = 7; i >= 0; i--) {
        RGB_SendBit((uint8_t)((byte >> i) & 1));
    }
}

/*********************************************************************
 * @fn      RGB_SendLed
 *
 * @brief   Send one pixel in GRB order with the brightness scaler.
 *          Interrupts are masked for the 24 bit transfer (~30 us) -
 *          short enough to keep the 115200 baud UART RX interrupt safe.
 *
 * @return  none
 */
static void RGB_SendLed(uint8_t r, uint8_t g, uint8_t b)
{
    __disable_irq();
    RGB_SendByte((uint8_t)(((uint16_t)g * s_brightness) >> 8));
    RGB_SendByte((uint8_t)(((uint16_t)r * s_brightness) >> 8));
    RGB_SendByte((uint8_t)(((uint16_t)b * s_brightness) >> 8));
    __enable_irq();
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void RGB_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    gpio.GPIO_Pin   = RGB_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOD, &gpio);
    GPIO_ResetBits(GPIOD, RGB_PIN);

    RGB_Clear();
    RGB_Refresh();
}

void RGB_SetPixel(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index >= RGB_LED_COUNT) return;
    s_leds[index].r = r;
    s_leds[index].g = g;
    s_leds[index].b = b;
}

void RGB_SetPixelColor(uint8_t index, uint32_t color)
{
    RGB_SetPixel(index,
                 (uint8_t)(color >> 16),
                 (uint8_t)(color >> 8),
                 (uint8_t)(color));
}

void RGB_SetAll(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t i;
    for (i = 0; i < RGB_LED_COUNT; i++) {
        RGB_SetPixel(i, r, g, b);
    }
}

void RGB_Clear(void)
{
    RGB_SetAll(0, 0, 0);
}

void RGB_SetBrightness(uint8_t brightness)
{
    s_brightness = brightness;
}

rgb_color_t *RGB_GetBuffer(void)
{
    return s_leds;
}

void RGB_Refresh(void)
{
    uint8_t i;
    for (i = 0; i < RGB_LED_COUNT; i++) {
        RGB_SendLed(s_leds[i].r, s_leds[i].g, s_leds[i].b);
    }
    /* Latch: DIN low > 280 us */
    GPIO_ResetBits(GPIOD, RGB_PIN);
    Delay_Us(300);
}
