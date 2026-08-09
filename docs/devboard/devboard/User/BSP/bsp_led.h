/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_led.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : On-board user LED driver (LED11, wired 3V3 -> LED ->
*                      R9 -> PC0, so the LED lights when PC0 is LOW).
*
*                      Typical use:
*                          LED_Init();
*                          LED_On();
*                          LED_Toggle();
********************************************************************************/
#ifndef __BSP_LED_H
#define __BSP_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Configuration
 *=============================================================================*/

/* 1 = LED lights when the pin is driven LOW (this board: 3V3-LED-R9-PC0).
 * 0 = LED lights when the pin is driven HIGH. */
#define LED_ACTIVE_LOW      1

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      LED_Init
 *
 * @brief   Configure PC0 as push-pull output, LED off.
 *
 * @return  none
 */
void LED_Init(void);

/*********************************************************************
 * @fn      LED_On / LED_Off / LED_Toggle
 *
 * @brief   Basic LED control.
 *
 * @return  none
 */
void LED_On(void);
void LED_Off(void);
void LED_Toggle(void);

/*********************************************************************
 * @fn      LED_Set
 *
 * @brief   Set the LED state explicitly.
 *
 * @param   on - 1 = on, 0 = off
 *
 * @return  none
 */
void LED_Set(uint8_t on);

/*********************************************************************
 * @fn      LED_Get
 *
 * @brief   Query the current LED state (tracked in software).
 *
 * @return  1 = on, 0 = off.
 */
uint8_t LED_Get(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_LED_H */
