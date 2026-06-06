#ifndef __UART_FP_H__
#define __UART_FP_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

#define UART_FP_USART          USART2
#define UART_FP_IRQn           USART2_IRQn
#define UART_FP_BAUDRATE       57600

#define UART_FP_TX_PORT        GPIOA
#define UART_FP_TX_PIN         GPIO_Pin_2
#define UART_FP_RX_PORT        GPIOA
#define UART_FP_RX_PIN         GPIO_Pin_3

#define UART_FP_RX_BUF_SIZE    256

void UartFp_Init(void);
void UartFp_SendData(const uint8_t *data, uint16_t len);

extern volatile uint8_t  uart_fp_rx_buf[UART_FP_RX_BUF_SIZE];
extern volatile uint16_t uart_fp_rx_len;
extern volatile uint8_t  uart_fp_rx_ready;

#ifdef __cplusplus
}
#endif

#endif
