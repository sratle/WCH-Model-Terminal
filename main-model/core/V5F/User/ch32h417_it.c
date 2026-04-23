/********************************** (C) COPYRIGHT *******************************
* File Name          : ch32h417_it.c
* Author             : 
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : Main Interrupt Service Routines.
*******************************************************************************/
#include "ch32h417_it.h"
#include "hardware.h"

void NMI_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void HardFault_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void USART4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

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

/*********************************************************************
 * @fn      USART4_IRQHandler
 *
 * @brief   This function handles USART4 global interrupt (Display).
 *
 * @return  none
 */
void USART4_IRQHandler(void)
{
    Display_UART_IRQ_Handler(&display_g);
}
