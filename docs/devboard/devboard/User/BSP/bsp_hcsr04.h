/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_hcsr04.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : HC-SR04 ultrasonic ranging driver.
*                      Wiring: Trig = PC14 (output), Echo = PC13 (input).
*                      TIM3 runs as a free-running 1 MHz counter used to
*                      measure the Echo pulse width.
*
*                      Typical use:
*                          HCSR04_Init();
*                          int32_t mm = HCSR04_ReadMm();   // -1 = no echo
********************************************************************************/
#ifndef __BSP_HCSR04_H
#define __BSP_HCSR04_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Configuration
 *=============================================================================*/

/* Ranging limits of the HC-SR04 (used for timeout handling) */
#define HCSR04_MAX_DISTANCE_MM  4000    /* Spec maximum range           */
#define HCSR04_TIMEOUT_MS       40      /* Worst-case echo wait         */

/* Return value when no valid echo was received */
#define HCSR04_INVALID          (-1)

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      HCSR04_Init
 *
 * @brief   Configure Trig/Echo pins and start the TIM3 1 MHz timebase.
 *
 * @return  none
 */
void HCSR04_Init(void);

/*********************************************************************
 * @fn      HCSR04_ReadMm
 *
 * @brief   Trigger one measurement and return the distance.
 *          Blocking: typically returns within a few ms, at most
 *          HCSR04_TIMEOUT_MS when no object is in range.
 *
 * @return  Distance in millimetres, or HCSR04_INVALID (-1) on timeout
 *          (no echo / object out of range).
 */
int32_t HCSR04_ReadMm(void);

/*********************************************************************
 * @fn      HCSR04_ReadRawUs
 *
 * @brief   Trigger one measurement and return the raw Echo pulse width.
 *
 * @return  Pulse width in microseconds, or HCSR04_INVALID on timeout.
 */
int32_t HCSR04_ReadRawUs(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_HCSR04_H */
