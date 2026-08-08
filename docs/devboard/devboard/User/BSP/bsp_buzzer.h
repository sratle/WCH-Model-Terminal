/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_buzzer.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : Passive buzzer driver (4 kHz resonant, Q1 SS8550 PNP,
*                      active low on PB15).
*                      PB15 is NOT a timer channel on CH32V307, so the
*                      square wave is produced by toggling the pin inside
*                      the TIM2 update interrupt (rate = 2 x frequency).
*
*                      Typical use:
*                          BUZZER_Init();
*                          BUZZER_On(4000);        // start 4 kHz tone
*                          BUZZER_Off();           // stop
*                          BUZZER_Beep(2000, 100); // blocking 100 ms beep
********************************************************************************/
#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      BUZZER_Init
 *
 * @brief   Configure PB15 high (buzzer off) and set up TIM2 as the
 *          toggle timebase (stopped until the first tone).
 *
 * @return  none
 */
void BUZZER_Init(void);

/*********************************************************************
 * @fn      BUZZER_On
 *
 * @brief   Start a continuous tone (non-blocking).
 *
 * @param   freq_hz - tone frequency in Hz, valid ~8 .. 50000
 *                    (the buzzer resonates at ~4 kHz = loudest)
 *
 * @return  none
 */
void BUZZER_On(uint32_t freq_hz);

/*********************************************************************
 * @fn      BUZZER_Off
 *
 * @brief   Stop the tone immediately (pin back to idle high).
 *
 * @return  none
 */
void BUZZER_Off(void);

/*********************************************************************
 * @fn      BUZZER_Beep
 *
 * @brief   Blocking beep: tone on, wait, tone off.
 *
 * @param   freq_hz    - tone frequency in Hz
 * @param   duration_ms - beep duration in milliseconds
 *
 * @return  none
 */
void BUZZER_Beep(uint32_t freq_hz, uint32_t duration_ms);

/*********************************************************************
 * @fn      BUZZER_IsOn
 *
 * @brief   Query whether a tone is currently playing.
 *
 * @return  1 = sounding, 0 = silent.
 */
uint8_t BUZZER_IsOn(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_BUZZER_H */
