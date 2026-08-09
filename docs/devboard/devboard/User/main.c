/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH-DevBoard Team
* Version            : V1.2.0
* Date               : 2026/08/08
* Description        : DevBoard peripheral integration test.
*                        - LCD: live 4x4 key-state grid + UART monitor
*                        - KEY (74HC165 x2): 16 keys, debounced scan
*                        - RGB (WS2812 x9): startup chase, S1-S9 toggle LEDs
*                        - BUZZER (PB15/TIM2): beep on key press
*                        - UART (USART1/CH340N): full duplex, RX ring buffer,
*                          every received byte is echoed back and printed
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/

#include "debug.h"
#include <stdio.h>
#include "BSP/bsp_lcd.h"
#include "BSP/bsp_key.h"
#include "BSP/bsp_rgb.h"
#include "BSP/bsp_buzzer.h"
#include "BSP/bsp_uart.h"
#include "BSP/bsp_led.h"
#include "BSP/bsp_hcsr04.h"

/*=============================================================================
 *  Demo Helpers
 *=============================================================================*/

/*********************************************************************
 * @fn      Demo_StartupEffect
 *
 * @brief   Power-on self test: LED chase + beep + title screen.
 *
 * @return  none
 */
static void Demo_StartupEffect(void)
{
    uint8_t i;

    LCD_Fill(LCD_BLACK);
    LCD_DrawTextCenter(100, "WCH-DevBoard", LCD_WHITE);
    LCD_DrawTextCenter(130, "Peripheral Test", LCD_GRAY);

    /* RGB chase across the 9 LEDs */
    for (i = 0; i < RGB_LED_COUNT; i++) {
        RGB_Clear();
        RGB_SetPixel(i, 0, 40, 80);
        RGB_Refresh();
        Delay_Ms(60);
    }
    RGB_Clear();
    RGB_Refresh();

    BUZZER_Beep(4000, 80);      /* 4 kHz resonance beep */
}

/*********************************************************************
 * @fn      Demo_DrawKeyGrid
 *
 * @brief   Draw the 4x4 key grid; pressed keys get a filled box.
 *
 * @param   keys - pressed mask from KEY_Scan()
 *
 * @return  none
 */
static void Demo_DrawKeyGrid(uint16_t keys)
{
    uint8_t i;
    for (i = 0; i < 16; i++) {
        uint16_t bx = (uint16_t)(20 + (i % 4) * 75);
        uint16_t by = (uint16_t)(60 + (i / 4) * 40);
        char label[4];

        label[0] = 'S';
        if (i + 1 < 10) {
            label[1] = (char)('1' + i);
            label[2] = '\0';
        } else {
            label[1] = '1';
            label[2] = (char)('0' + (i + 1 - 10));
            label[3] = '\0';
        }

        if (keys & (1U << i)) {
            LCD_FillRoundRect(bx, by, 60, 30, 6, LCD_DARKGREEN);
            LCD_DrawTextBG((uint16_t)(bx + 16), (uint16_t)(by + 6),
                           label, LCD_WHITE, LCD_DARKGREEN);
        } else {
            LCD_FillRoundRect(bx, by, 60, 30, 6, LCD_BLACK);
            LCD_DrawRoundRect(bx, by, 60, 30, 6, LCD_GRAY);
            LCD_DrawText((uint16_t)(bx + 16), (uint16_t)(by + 6),
                         label, LCD_GRAY);
        }
    }
}

/* Per-LED colors for S1-S9 (physical order, 0xRRGGBB) */
static const uint32_t s_led_colors[RGB_LED_COUNT] = {
    0xFF0000,   /* 1 red    */
    0xFF8000,   /* 2 orange */
    0xFFFF00,   /* 3 yellow */
    0x00FF00,   /* 4 green  */
    0x00FFFF,   /* 5 cyan   */
    0x0000FF,   /* 6 blue   */
    0x8000FF,   /* 7 purple */
    0xFF00FF,   /* 8 magenta*/
    0xFFFFFF,   /* 9 white  */
};

/*********************************************************************
 * @fn      Demo_HandleKeys
 *
 * @brief   React to newly pressed keys: S1-S9 toggle the RGB LEDs
 *          (each with its own color), every key press beeps and
 *          prints over UART.
 *
 * @param   keys - current debounced mask
 * @param   prev - previous mask (for edge detection)
 *
 * @return  none
 */
static void Demo_HandleKeys(uint16_t keys, uint16_t prev)
{
    uint16_t pressed = (uint16_t)(keys & ~prev);    /* Rising edges */
    uint8_t  i;

    for (i = 0; i < 16; i++) {
        if (pressed & (1U << i)) {
            printf("key S%d pressed\r\n", i + 1);
            BUZZER_Beep((uint32_t)(1000 + i * 200), 30);
            LED_Toggle();                   /* Visual key-press feedback */

            if (i < RGB_LED_COUNT) {    /* S1..S9 toggle LED i       */
                rgb_color_t *leds = RGB_GetBuffer();
                if (leds[i].r || leds[i].g || leds[i].b) {
                    RGB_SetPixel(i, 0, 0, 0);
                } else {
                    RGB_SetPixelColor(i, s_led_colors[i]);
                }
                RGB_Refresh();
            }
        }
    }
}

/*********************************************************************
 * @fn      Demo_HandleUart
 *
 * @brief   Drain the RX ring buffer: echo every byte back and show the
 *          received count on the screen.
 *
 * @return  none
 */
static void Demo_HandleUart(void)
{
    int16_t ch;
    while ((ch = UART_ReadByte()) >= 0) {
        UART_SendByte((uint8_t)ch);             /* Echo back */
        printf("uart rx: 0x%02X '%c'\r\n", (uint8_t)ch,
               (ch >= 32 && ch < 127) ? ch : '.');
    }
}

/*********************************************************************
 * @fn      Demo_ShowDistance
 *
 * @brief   Read the HC-SR04 and refresh the distance line on screen.
 *
 * @return  none
 */
static void Demo_ShowDistance(void)
{
    char    buf[24];
    int32_t mm = HCSR04_ReadMm();

    if (mm >= 0) {
        snprintf(buf, sizeof(buf), "Distance: %4ld mm", (long)mm);
    } else {
        snprintf(buf, sizeof(buf), "Distance: ---- mm");
    }
    /* Clear the field first: the font is proportional, so a shorter
     * reading would otherwise leave old pixels at the right edge */
    LCD_FillRect(60, 30, 200, LCD_FontHeight(), LCD_BLACK);
    LCD_DrawTextBG(60, 30, buf, LCD_YELLOW, LCD_BLACK);
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    uint16_t keys = 0, prev = 0;
    uint8_t  dist_div = 0;              /* Measure every N loop passes */

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    UART_Init(115200);          /* Full duplex; printf keeps working   */
    KEY_Init();
    RGB_Init();
    RGB_SetBrightness(40);      /* Keep the 255-level palette eye-safe */
    BUZZER_Init();
    LED_Init();
    LCD_Init();
    HCSR04_Init();

    printf("WCH-DevBoard peripheral test\r\n");

    Demo_StartupEffect();

    LCD_Fill(LCD_BLACK);
    LCD_DrawTextCenter(8, "WCH-DevBoard", LCD_WHITE);
    Demo_DrawKeyGrid(0);

    while (1)
    {
        keys = KEY_Scan();
        if (keys != prev) {
            Demo_DrawKeyGrid(keys);
            Demo_HandleKeys(keys, prev);
            prev = keys;
        }
        Demo_HandleUart();

        /* Distance refresh every ~5 loop passes (KEY_Scan blocks 5 ms) */
        if (++dist_div >= 5) {
            dist_div = 0;
            Demo_ShowDistance();
        }
    }
}
