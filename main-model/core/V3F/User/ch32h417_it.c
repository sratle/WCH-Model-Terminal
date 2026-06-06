/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32h417_it.c
* Author             :
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : Main Interrupt Service Routines for V3F.
*
* V3F 不再处理任何外设中断，所有 UART 中断由 V5F 处理。
*******************************************************************************/
#include "ch32h417_it.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

/*********************************************************************
 * @fn      NMI_Handler
 *
 * @brief   This function handles NMI exception.
 *
 * @return  none
 */
void NMI_Handler(void)
{
  while (1)
  {
  }
}

/*********************************************************************
 * @fn      HardFault_Handler
 *
 * @brief   This function handles Hard Fault exception.
 *
 * @return  none
 */
void HardFault_Handler(void)
{
  NVIC_SystemReset();
  while (1)
  {
  }
}
