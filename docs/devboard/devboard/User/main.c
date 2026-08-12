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
#include <stdlib.h>
#include <string.h>
#include "BSP/bsp_lcd.h"
#include "BSP/bsp_key.h"
#include "BSP/bsp_rgb.h"
#include "BSP/bsp_buzzer.h"
#include "BSP/bsp_uart.h"
#include "BSP/bsp_led.h"
#include "BSP/bsp_hcsr04.h"
#include "BSP/bsp_cmd.h"

uint8_t timer_mode = 0;
uint8_t timer_state = 0;
uint16_t now_time = 0;
uint16_t start = 0;
uint16_t stop = 0;

uint16_t led_time = 20;
uint16_t led_color = 2; // 0 blue,1 red,2 green

uint8_t yp_mode = 0;

uint16_t yp_count = 0;
uint8_t now_yp[7] = {0};

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
    // LCD_DrawTextCenter(100, "WCH-DevBoard", LCD_WHITE);
    // LCD_DrawTextCenter(130, "Peripheral Test", LCD_GRAY);

    // /* RGB chase across the 9 LEDs */
    // for (i = 0; i < RGB_LED_COUNT; i++) {
    //     RGB_Clear();
    //     RGB_SetPixel(i, 0, 40, 80);
    //     RGB_Refresh();
    //     Delay_Ms(60);
    // }
    RGB_Clear();
    RGB_Refresh();

    // BUZZER_Beep(4000, 80); /* 4 kHz resonance beep */
}

// /*********************************************************************
//  * @fn      Demo_DrawKeyGrid
//  *
//  * @brief   Draw the 4x4 key grid; pressed keys get a filled box.
//  *
//  * @param   keys - pressed mask from KEY_Scan()
//  *
//  * @return  none
//  */
// static void Demo_DrawKeyGrid(uint16_t keys)
// {
//     uint8_t i;
//     for (i = 0; i < 16; i++) {
//         uint16_t bx = (uint16_t)(20 + (i % 4) * 75);
//         uint16_t by = (uint16_t)(60 + (i / 4) * 40);
//         char label[4];

//         label[0] = 'S';
//         if (i + 1 < 10) {
//             label[1] = (char)('1' + i);
//             label[2] = '\0';
//         } else {
//             label[1] = '1';
//             label[2] = (char)('0' + (i + 1 - 10));
//             label[3] = '\0';
//         }

//         if (keys & (1U << i)) {
//             LCD_FillRoundRect(bx, by, 60, 30, 6, LCD_DARKGREEN);
//             LCD_DrawTextBG((uint16_t)(bx + 16), (uint16_t)(by + 6),
//                            label, LCD_WHITE, LCD_DARKGREEN);
//         } else {
//             LCD_FillRoundRect(bx, by, 60, 30, 6, LCD_BLACK);
//             LCD_DrawRoundRect(bx, by, 60, 30, 6, LCD_GRAY);
//             LCD_DrawText((uint16_t)(bx + 16), (uint16_t)(by + 6),
//                          label, LCD_GRAY);
//         }
//     }
// }

/* Per-LED colors for S1-S9 (physical order, 0xRRGGBB) */
static const uint32_t s_led_colors[RGB_LED_COUNT] = {
    0xFF0000, /* 1 red    */
    0xFF8000, /* 2 orange */
    0xFFFF00, /* 3 yellow */
    0x00FF00, /* 4 green  */
    0x00FFFF, /* 5 cyan   */
    0x0000FF, /* 6 blue   */
    0x8000FF, /* 7 purple */
    0xFF00FF, /* 8 magenta*/
    0xFFFFFF, /* 9 white  */
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
    uint16_t pressed = (uint16_t)(keys & ~prev); /* Rising edges */
    uint8_t i;

    for (i = 0; i < 16; i++)
    {
        if (pressed & (1U << i))
        {
            printf("key S%d pressed\r\n", i + 1);
            if (i == 0)
            {
                if (timer_state == 0)
                {
                    start = (uint16_t)TIM_GetCounter(TIM3);
                    timer_state = 1;
                    printf("timer on");
                }
                else if (timer_state == 1)
                {
                    timer_state = 0;
                    printf("timer off");
                }
            }
            else if (i == 1)
            {
                timer_mode = 1 - timer_mode;
            }
            else if (i == 3)
            {
                led_time = led_time + 1;
                if (led_time > 20)
                {
                    led_time = 5;
                }
            }
            else if (i == 2)
            {
                led_time = led_time - 1;
                if (led_time < 5)
                {
                    led_time = 20;
                }
            }
            else if (i == 4)
            {
            }
            // BUZZER_Beep((uint32_t)(1000 + i * 200), 30);
            // LED_Toggle();                   /* Visual key-press feedback */

            // if (i < RGB_LED_COUNT) {    /* S1..S9 toggle LED i       */
            //     rgb_color_t *leds = RGB_GetBuffer();
            //     if (leds[i].r || leds[i].g || leds[i].b) {
            //         RGB_SetPixel(i, 0, 0, 0);
            //     } else {
            //         RGB_SetPixelColor(i, s_led_colors[i]);
            //     }
            //     RGB_Refresh();
            // }
        }
    }
}

/*=============================================================================
 *  Console Commands
 *=============================================================================*/

static int Cmd_Help(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    CMD_PrintHelp();
    return CMD_OK;
}

/* rgb <id 1-9> <rrggbb hex>  e.g. "rgb 3 ff8000" */
static int Cmd_Rgb(int argc, char *argv[])
{
    long id;
    unsigned long color;
    if (argc < 3)
        return CMD_ERR_USAGE;
    id = strtol(argv[1], NULL, 0);
    color = strtoul(argv[2], NULL, 16);
    if (id < 1 || id > RGB_LED_COUNT)
        return CMD_ERR_VALUE;
    RGB_SetPixelColor((uint8_t)(id - 1), color);
    RGB_Refresh();
    printf("rgb %ld <- %06lX\r\n", id, color);
    return CMD_OK;
}

static int Cmd_RgbOff(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    RGB_Clear();
    RGB_Refresh();
    printf("rgb off\r\n");
    return CMD_OK;
}

/* led on|off|toggle */
static int Cmd_Led(int argc, char *argv[])
{
    if (argc < 2)
        return CMD_ERR_USAGE;
    if (!strcmp(argv[1], "on"))
        LED_On();
    else if (!strcmp(argv[1], "off"))
        LED_Off();
    else if (!strcmp(argv[1], "toggle"))
        LED_Toggle();
    else
        return CMD_ERR_USAGE;
    printf("led %s\r\n", LED_Get() ? "on" : "off");
    return CMD_OK;
}

/* beep <freq_hz> <ms> */
static int Cmd_Beep(int argc, char *argv[])
{
    long freq, ms;
    if (argc < 3)
        return CMD_ERR_USAGE;
    freq = strtol(argv[1], NULL, 0);
    ms = strtol(argv[2], NULL, 0);
    if (freq <= 0 || ms <= 0 || ms > 5000)
        return CMD_ERR_VALUE;
    BUZZER_Beep((uint32_t)freq, (uint32_t)ms);
    return CMD_OK;
}

static int Cmd_Dist(int argc, char *argv[])
{
    int32_t mm;
    (void)argc;
    (void)argv;
    mm = HCSR04_ReadMm();
    if (mm >= 0)
        printf("distance %ld mm\r\n", (long)mm);
    else
        printf("distance out of range\r\n");
    return CMD_OK;
}

static int Cmd_Keys(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("keys 0x%04X\r\n", KEY_ReadRaw());
    return CMD_OK;
}

/* text <x> <y> <string...> - draws on the LCD, transparent background */
static int Cmd_Text(int argc, char *argv[])
{
    long x, y;
    int i;
    if (argc < 4)
        return CMD_ERR_USAGE;
    x = strtol(argv[1], NULL, 0);
    y = strtol(argv[2], NULL, 0);
    for (i = 3; i < argc; i++)
    {
        LCD_DrawText((uint16_t)x, (uint16_t)y, argv[i], LCD_WHITE);
        x += LCD_TextWidth(argv[i]) + 8; /* Word + space gap */
    }
    return CMD_OK;
}

static int Cmd_TASKYP(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("startTask");
    yp_mode = 1;
    BUZZER_Beep(1000, 100);
    yp_count = 0;
    return CMD_OK;
}

static int Cmd_YP_01(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 0;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

static int Cmd_YP_02(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 1;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

static int Cmd_YP_03(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 2;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

static int Cmd_YP_04(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 3;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

static int Cmd_YP_05(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 4;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

static int Cmd_YP_06(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 5;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

static int Cmd_YP_07(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    now_yp[0] = 6;
    now_yp[1] = strtol(argv[1], NULL, 0) - 1;
    now_yp[2] = strtol(argv[2], NULL, 0) - 1;
    now_yp[3] = strtol(argv[3], NULL, 0) - 1;
    now_yp[4] = strtol(argv[4], NULL, 0) - 1;
    now_yp[5] = strtol(argv[5], NULL, 0) - 1;
    now_yp[6] = (char)(argv[6][1]) - 48 - 1;
    return CMD_OK;
}

/*********************************************************************
 * @fn      Demo_RegisterCommands
 *
 * @brief   Fill the console command table.
 *
 * @return  none
 */
static void Demo_RegisterCommands(void)
{
    CMD_Register("help", Cmd_Help, "help                     - list commands");
    CMD_Register("rgb", Cmd_Rgb, "rgb <id1-9> <rrggbb>     - set one RGB LED");
    CMD_Register("rgboff", Cmd_RgbOff, "rgboff                   - all RGB LEDs off");
    CMD_Register("led", Cmd_Led, "led on|off|toggle        - user LED");
    CMD_Register("beep", Cmd_Beep, "beep <freq_hz> <ms>      - buzzer beep");
    CMD_Register("dist", Cmd_Dist, "dist                     - read HC-SR04 (mm)");
    CMD_Register("keys", Cmd_Keys, "keys                     - key state mask");
    CMD_Register("text", Cmd_Text, "text <x> <y> <string...> - draw text on LCD");
    CMD_Register("TaskYP", Cmd_TASKYP, "TASKYP");
    CMD_Register("YP=<01", Cmd_YP_01, "TASKYP");
    CMD_Register("YP=<02", Cmd_YP_02, "TASKYP");
    CMD_Register("YP=<03", Cmd_YP_03, "TASKYP");
    CMD_Register("YP=<04", Cmd_YP_04, "TASKYP");
    CMD_Register("YP=<05", Cmd_YP_05, "TASKYP");
    CMD_Register("YP=<06", Cmd_YP_06, "TASKYP");
    CMD_Register("YP=<07", Cmd_YP_07, "TASKYP");
}

// /*********************************************************************
//  * @fn      Demo_ShowDistance
//  *
//  * @brief   Read the HC-SR04 and refresh the distance line on screen.
//  *
//  * @return  none
//  */
// static void Demo_ShowDistance(void)
// {
//     char    buf[24];
//     int32_t mm = HCSR04_ReadMm();

//     if (mm >= 0) {
//         snprintf(buf, sizeof(buf), "Distance: %4ld mm", (long)mm);
//     } else {
//         snprintf(buf, sizeof(buf), "Distance: ---- mm");
//     }
//     /* Clear the field first: the font is proportional, so a shorter
//      * reading would otherwise leave old pixels at the right edge */
//     LCD_FillRect(60, 30, 200, LCD_FontHeight(), LCD_BLACK);
//     LCD_DrawTextBG(60, 30, buf, LCD_YELLOW, LCD_BLACK);
// }

static void ShowTimer(void)
{
    char buf[24];
    static uint16_t front=0;
    static uint16_t back=0;
    stop = (uint16_t)TIM_GetCounter(TIM3);
    LCD_FillRect(0, 20, 300, LCD_FontHeight(), LCD_BLACK);
    if (timer_state == 1)
    {
        if (timer_mode == 0)
        {
            front = ((stop - start) / 1000) % 60;
            back = (stop - start) % 100;
            
        }
        else if (timer_mode == 1)
        {

            front = 100 - ((stop - start) / 1000) % 60 - 1;
            back = 100 - (stop - start) % 100 - 1;
        }
    }
    snprintf(buf, sizeof(buf), "%d . %d", front, back);
    LCD_DrawText(120, 20, buf, LCD_YELLOW);
    if (timer_mode == 0)
    {
        LCD_DrawText(0, 20, "UP TIMER", LCD_WHITE);
    }
    else if (timer_mode == 1)
    {
        LCD_DrawText(0, 20, "DOWN TIMER", LCD_WHITE);
    }
}

static void ShowLED(void)
{
    static uint8_t led_now = 0;
    static uint16_t led_off_start = 0;
    static uint16_t led_off_stop = 0;
    static uint16_t led_on_start = 0;
    static uint16_t led_on_stop = 0;
    if (led_now == 0)
    {
        RGB_Clear();
        led_off_stop = (uint16_t)TIM_GetCounter(TIM3);
        if (led_off_stop - led_off_start > led_time * 100)
        {
            led_on_start = (uint16_t)TIM_GetCounter(TIM3);
            led_off_stop = 0;
            led_now = 1;
        }
    }
    if (led_now == 1)
    {
        led_on_stop = (uint16_t)TIM_GetCounter(TIM3);
        if (led_color == 0)
        {
            RGB_SetPixelColor(0, 0x0000FF);
        }
        if (led_color == 1)
        {
            RGB_SetPixelColor(0, 0xFF0000);
        }
        if (led_color == 2)
        {
            RGB_SetPixelColor(0, 0x00FF00);
        }
        if (led_on_stop - led_on_start > led_time * 100)
        {
            led_off_start = (uint16_t)TIM_GetCounter(TIM3);
            led_on_stop = 0;
            led_now = 0;
        }
    }
    char buf[24];
    uint16_t front = led_time / 10;
    uint16_t back = led_time % 10;
    LCD_FillRect(0, 40, 300, LCD_FontHeight(), LCD_BLACK);
    snprintf(buf, sizeof(buf), "%d . %d", front, back);
    LCD_DrawText(0, 40, "LED", LCD_WHITE);
    LCD_DrawText(120, 40, buf, LCD_YELLOW);
    RGB_Refresh();
}

static void ProcessTaskYP(void)
{
    static uint16_t yp_start = 0;
    static uint16_t yp_stop = 0;
    char buf[24];
    static uint16_t yp_freq[7] = {523, 587, 659, 698, 784, 880, 980};
    if (yp_mode == 1)
    {

        yp_stop = (uint16_t)TIM_GetCounter(TIM3);

        if (yp_count > 0)
        {
            if (yp_stop - yp_start > led_time * 100)
            {
                BUZZER_Off();
                BUZZER_On(yp_freq[now_yp[yp_count]]);
                yp_start = (uint16_t)TIM_GetCounter(TIM3);
                yp_count++;
                if (yp_count > 6)
                {
                    yp_count = 0;
                }
            }
        }
        else
        {
            yp_start = (uint16_t)TIM_GetCounter(TIM3);
            BUZZER_On(yp_freq[now_yp[yp_count]]);
            yp_count++;
        }
    }
    LCD_FillRect(0, 60, 300, LCD_FontHeight(), LCD_BLACK);
    snprintf(buf, sizeof(buf), "YP=%d-%d-%d-%d-%d-%d-%d", now_yp[0] + 1, now_yp[1] + 1, now_yp[2] + 1, now_yp[3] + 1, now_yp[4] + 1, now_yp[5] + 1, now_yp[6] + 1);
    LCD_DrawText(120, 60, buf, LCD_YELLOW);
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
    uint8_t dist_div = 0; /* Measure every N loop passes */

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    UART_Init(115200); /* Full duplex; printf keeps working   */
    CMD_Init();        /* Console line editor + prompt        */
    Demo_RegisterCommands();
    KEY_Init();
    RGB_Init();
    RGB_SetBrightness(40); /* Keep the 255-level palette eye-safe */
    BUZZER_Init();
    LED_Init();
    LCD_Init();
    HCSR04_Init();

    printf("WCH-DevBoard peripheral test\r\n");

    Demo_StartupEffect();

    LCD_Fill(LCD_BLACK);
    LCD_DrawTextCenter(0, "CH2026 AI for Design, Design for AI", LCD_WHITE);
    // Demo_DrawKeyGrid(0);

    while (1)
    {
        keys = KEY_Scan();
        if (keys != prev)
        {
            Demo_HandleKeys(keys, prev);
            prev = keys;
        }
        CMD_Task(); /* Console: assemble + run commands    */
        ShowTimer();
        ShowLED();

        ProcessTaskYP();
        Delay_Ms(100);
    }
}
