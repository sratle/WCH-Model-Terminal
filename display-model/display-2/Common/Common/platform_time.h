/********************************** (C) COPYRIGHT *******************************
* File Name          : platform_time.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : Free-running millisecond time base for MiniUI.
*
*  Background: ui_get_real_ms() previously read SysTick->CNT, but the WCH
*  Delay_Us/Delay_Ms helpers reprogram the SysTick counter on every call,
*  so the derived "milliseconds" value was always ~0.  This module provides
*  an independent 1 kHz tick on TIM2 (clocked at 144 MHz from PCLK1 x2)
*  that nothing else can disturb.
********************************************************************************/
#ifndef __PLATFORM_TIME_H
#define __PLATFORM_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Start TIM2 @ 1 kHz update interrupt.  Call once at startup. */
void Platform_Time_Init(void);

/* Milliseconds since Platform_Time_Init() (wraps after ~49.7 days).
 * Unsigned subtraction of two snapshots stays correct across wrap. */
uint32_t Platform_Millis(void);

#ifdef __cplusplus
}
#endif

#endif /* __PLATFORM_TIME_H */
