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

void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART6_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART7_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART8_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

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
 * @fn      USART1_IRQHandler
 *
 * @brief   This function handles USART1 global interrupt (CH9350 HID).
 *
 * @return  none
 */
void USART1_IRQHandler(void)
{
    CH9350_UART_IRQ_Handler(&ch9350_g);
}

/*********************************************************************
 * @fn      USART2_IRQHandler
 *
 * @brief   This function handles USART2 global interrupt (Debug/CLI).
 *
 * @return  none
 */
void USART2_IRQHandler(void)
{
    Debug_UART_IRQ_Handler();
}

/*********************************************************************
 * @fn      USART3_IRQHandler
 *
 * @brief   This function handles USART3 global interrupt (Keyboard).
 *
 * @return  none
 */
void USART3_IRQHandler(void)
{
    Keyboard_UART_IRQ_Handler(&keyboard_g);
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

/*********************************************************************
 * @fn      USART5_IRQHandler
 *
 * @brief   This function handles USART5 global interrupt (Power).
 *
 * @return  none
 */
void USART5_IRQHandler(void)
{
    Power_UART_IRQ_Handler(&power_g);
}

/*********************************************************************
 * @fn      USART6_IRQHandler
 *
 * @brief   This function handles USART6 global interrupt (Submodel-1).
 *
 * @return  none
 */
void USART6_IRQHandler(void)
{
    Submodels_UART_IRQ_Handler(&submodels_g[0], USART6);
}

/*********************************************************************
 * @fn      USART7_IRQHandler
 *
 * @brief   This function handles USART7 global interrupt (Submodel-2).
 *
 * @return  none
 */
void USART7_IRQHandler(void)
{
    Submodels_UART_IRQ_Handler(&submodels_g[1], USART7);
}

/*********************************************************************
 * @fn      USART8_IRQHandler
 *
 * @brief   This function handles USART8 global interrupt (Submodel-3).
 *
 * @return  none
 */
void USART8_IRQHandler(void)
{
    Submodels_UART_IRQ_Handler(&submodels_g[2], USART8);
}
