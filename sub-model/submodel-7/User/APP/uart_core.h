/**
 * @file uart_core.h
 * @brief Submodel-7 UART1 驱动：230400/8-N-1，协议通信 + Bulk 模式
 *        PA9-TX, PA10-RX
 */
#ifndef __UART_CORE_H__
#define __UART_CORE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v10x.h"
#include "protocol.h"

#define UART_CORE_USART         USART1
#define UART_CORE_IRQn          USART1_IRQn
#define UART_CORE_BAUDRATE      230400

#define UART_CORE_TX_PORT       GPIOA
#define UART_CORE_TX_PIN        GPIO_Pin_9
#define UART_CORE_RX_PORT       GPIOA
#define UART_CORE_RX_PIN        GPIO_Pin_10

/*=============================================================================
 *  API
 *=============================================================================*/

void     UartCore_Init(void);
void     UartCore_SendData(const uint8_t *data, uint16_t len);
uint16_t UartCore_PackAndSend(uint8_t dst, uint8_t cmd,
                              const uint8_t *data, uint8_t data_len);

extern protocol_rx_ctx_t uart_core_rx_ctx;

#ifdef __cplusplus
}
#endif

#endif /* __UART_CORE_H__ */
