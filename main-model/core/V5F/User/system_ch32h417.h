/********************************** (C) COPYRIGHT *******************************
* File Name          : system_ch32h417.h
* Author             : 
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : CH32H417 Device Peripheral Access Layer System Header File.
*******************************************************************************/
#ifndef __SYSTEM_CH32H417_H 
#define __SYSTEM_CH32H417_H

#ifdef __cplusplus
 extern "C" {
#endif 

extern uint32_t HCLKClock; 
extern uint32_t SystemClock;               /* System Clock Frequency */
extern uint32_t SystemCoreClock;           /* System Core Frequency */

/* System_Exported_Functions */  
extern void SystemInit(void);
extern void SystemAndCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif 



