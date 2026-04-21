/********************************** (C) COPYRIGHT *******************************
* File Name          : lcd_config.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : LCD panel control signal configuration.
*                      Controls LCD_MODE (DE mode select), L/R flip, U/D flip,
*                      and LCD module reset via GPIO.
********************************************************************************/
#ifndef __LCD_CONFIG_H
#define __LCD_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32h417.h"

/*=============================================================================
 *  LCD Panel Control Signal Definitions
 *  From Plan.md Section 2.3:
 *    PA4 - LCD_MODE:  High = DE mode, Low = SYNC mode
 *    PA5 - LCD_L/R:   Left/Right flip control
 *    PA6 - LCD_U/D:   Up/Down flip control
 *    PA7 - LCD_RESET: LCD module reset (independent from SSD1963 RST)
 *=============================================================================*/

/* --- Control pins --- */
#define LCD_MODE_PIN            GPIO_Pin_4      /* PA4: DE mode select */
#define LCD_LR_PIN              GPIO_Pin_5      /* PA5: Left/Right flip */
#define LCD_UD_PIN              GPIO_Pin_6      /* PA6: Up/Down flip */
#define LCD_RESET_PIN           GPIO_Pin_7      /* PA7: LCD module reset */

/* --- GPIO Port --- */
#define LCD_CTRL_PORT           GPIOA

/*=============================================================================
 *  LCD Orientation Macros
 *  Set before LCD_Init() to configure initial orientation.
 *=============================================================================*/
#define LCD_ORIENTATION_NORMAL  0   /* No flip */
#define LCD_ORIENTATION_HFLIP   1   /* Horizontal flip (L/R) */
#define LCD_ORIENTATION_VFLIP   2   /* Vertical flip (U/D) */
#define LCD_ORIENTATION_HVFLIP  3   /* Both H and V flip */

/*=============================================================================
 *  Function Prototypes
 *=============================================================================*/

/**
 * @brief  Initialize LCD panel control signals (PA4~PA7).
 *         Configures GPIO and sets default state:
 *           - LCD_MODE = High (DE mode)
 *           - L/R, U/D = Normal orientation
 *           - LCD_RESET = High (inactive)
 * @param  orientation: LCD_ORIENTATION_NORMAL / HFLIP / VFLIP / HVFLIP
 * @return None
 */
void LCD_Init(uint8_t orientation);

/**
 * @brief  Hardware reset LCD panel via PA7 (independent from SSD1963).
 *         Pull low >= 10ms, then high >= 5ms.
 * @param  None
 * @return None
 */
void LCD_Reset(void);

/**
 * @brief  Set LCD orientation (horizontal/vertical flip).
 * @param  orientation: LCD_ORIENTATION_NORMAL / HFLIP / VFLIP / HVFLIP
 * @return None
 */
void LCD_SetOrientation(uint8_t orientation);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_CONFIG_H */
