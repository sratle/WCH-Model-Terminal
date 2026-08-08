/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_uart.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : Full-duplex UART driver on USART1 (PA9=TX, PA10=RX,
*                      wired to the on-board CH340N USB-serial bridge).
*                      RX is interrupt-driven into a ring buffer so no
*                      bytes are lost while the main loop is busy drawing.
*
*                      Typical use:
*                          UART_Init(115200);
*                          UART_SendString("hello\r\n");
*                          int ch;
*                          while ((ch = UART_ReadByte()) >= 0) { ... }
********************************************************************************/
#ifndef __BSP_UART_H
#define __BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Configuration
 *=============================================================================*/

/* RX ring buffer size in bytes (must be a power of two) */
#define UART_RX_BUF_SIZE    256

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      UART_Init
 *
 * @brief   Configure USART1 TX+RX and the RX interrupt.
 *          Safe to call instead of - or after - USART_Printf_Init()
 *          (both use USART1/PA9/PA10 with the same frame format).
 *
 * @param   baudrate - e.g. 115200
 *
 * @return  none
 */
void UART_Init(uint32_t baudrate);

/*********************************************************************
 * @fn      UART_SendByte / UART_SendBytes / UART_SendString
 *
 * @brief   Blocking transmit helpers (wait for the TX register).
 *
 * @return  none
 */
void UART_SendByte(uint8_t b);
void UART_SendBytes(const uint8_t *data, uint16_t len);
void UART_SendString(const char *str);

/*********************************************************************
 * @fn      UART_Available
 *
 * @brief   Number of bytes currently sitting in the RX ring buffer.
 *
 * @return  Byte count, 0 .. UART_RX_BUF_SIZE.
 */
uint16_t UART_Available(void);

/*********************************************************************
 * @fn      UART_ReadByte
 *
 * @brief   Pop one byte from the RX ring buffer (non-blocking).
 *
 * @return  The byte (0..255), or -1 when the buffer is empty.
 */
int16_t UART_ReadByte(void);

/*********************************************************************
 * @fn      UART_ReadBytes
 *
 * @brief   Pop up to max_len bytes into a user buffer.
 *
 * @param   buf     - destination buffer
 * @param   max_len - buffer capacity
 *
 * @return  Number of bytes actually read.
 */
uint16_t UART_ReadBytes(uint8_t *buf, uint16_t max_len);

/*********************************************************************
 * @fn      UART_RxFlush
 *
 * @brief   Discard all buffered RX data.
 *
 * @return  none
 */
void UART_RxFlush(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_UART_H */
