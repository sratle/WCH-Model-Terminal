/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd_config.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : LCD panel control signal implementation.
*                      PA4 (MODE), PA5 (L/R), PA6 (U/D), PA7 (RESET).
********************************************************************************/
#include "lcd_config.h"
#include "ch32h417_gpio.h"
#include "ch32h417_rcc.h"
#include "debug.h"

/*=============================================================================
 *  Public Functions
 *=============================================================================*/

/**
 * @brief  Initialize LCD panel control signals.
 *         - PA4: LCD_MODE, push-pull output, default High (DE mode)
 *         - PA5: LCD_L/R,  push-pull output, default Low (normal)
 *         - PA6: LCD_U/D,  push-pull output, default Low (normal)
 *         - PA7: LCD_RESET, push-pull output, default High (inactive)
 * @param  orientation: LCD_ORIENTATION_NORMAL / HFLIP / VFLIP / HVFLIP
 * @return None
 */
void LCD_Init(uint8_t orientation)
{
    GPIO_InitTypeDef gpio_init;

    printf("[LCD_Init] start\r\n");

    /* Enable GPIOA clock */
    printf("[LCD_Init] enable GPIOA clock\r\n");
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_GPIOA, ENABLE);

    /* Configure PA4~PA7 as push-pull outputs */
    printf("[LCD_Init] configure PA4~PA7\r\n");
    gpio_init.GPIO_Pin = LCD_MODE_PIN | LCD_LR_PIN | LCD_UD_PIN | LCD_RESET_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(LCD_CTRL_PORT, &gpio_init);

    /* Set default states */
    GPIO_SetBits(LCD_CTRL_PORT, LCD_MODE_PIN);      /* DE mode = High */
    GPIO_SetBits(LCD_CTRL_PORT, LCD_RESET_PIN);     /* Reset inactive = High */

    /* Apply orientation */
    LCD_SetOrientation(orientation);

    /* Hardware reset LCD panel */
    printf("[LCD_Init] reset LCD panel\r\n");
    LCD_Reset();

    printf("[LCD_Init] done\r\n");
}

/**
 * @brief  Hardware reset LCD panel via PA7.
 *         Independent from SSD1963 reset (PA0).
 */
void LCD_Reset(void)
{
    GPIO_ResetBits(LCD_CTRL_PORT, LCD_RESET_PIN);
    Delay_Ms(10);
    GPIO_SetBits(LCD_CTRL_PORT, LCD_RESET_PIN);
    Delay_Ms(5);
}

/**
 * @brief  Set LCD orientation (horizontal/vertical flip).
 * @param  orientation: LCD_ORIENTATION_NORMAL / HFLIP / VFLIP / HVFLIP
 */
void LCD_SetOrientation(uint8_t orientation)
{
    switch (orientation)
    {
        case LCD_ORIENTATION_NORMAL:
            GPIO_ResetBits(LCD_CTRL_PORT, LCD_LR_PIN);
            GPIO_ResetBits(LCD_CTRL_PORT, LCD_UD_PIN);
            break;

        case LCD_ORIENTATION_HFLIP:
            GPIO_SetBits(LCD_CTRL_PORT, LCD_LR_PIN);
            GPIO_ResetBits(LCD_CTRL_PORT, LCD_UD_PIN);
            break;

        case LCD_ORIENTATION_VFLIP:
            GPIO_ResetBits(LCD_CTRL_PORT, LCD_LR_PIN);
            GPIO_SetBits(LCD_CTRL_PORT, LCD_UD_PIN);
            break;

        case LCD_ORIENTATION_HVFLIP:
            GPIO_SetBits(LCD_CTRL_PORT, LCD_LR_PIN);
            GPIO_SetBits(LCD_CTRL_PORT, LCD_UD_PIN);
            break;

        default:
            GPIO_ResetBits(LCD_CTRL_PORT, LCD_LR_PIN);
            GPIO_ResetBits(LCD_CTRL_PORT, LCD_UD_PIN);
            break;
    }
}
