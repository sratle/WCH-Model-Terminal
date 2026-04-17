#ifndef __CH9350_H
#define __CH9350_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

#define CH9350_UART_BAUDRATE 115200
#define CH9350_UART USART1

/* CH9350 UART gpio definitions UART1 */
#define CH9350_UART_TX_PORT GPIOA
#define CH9350_UART_TX_PIN GPIO_Pin_9
#define CH9350_UART_TX_AF GPIO_AF7

#define CH9350_UART_RX_PORT GPIOA
#define CH9350_UART_RX_PIN GPIO_Pin_10
#define CH9350_UART_RX_AF GPIO_AF7

typedef struct
{
    uint8_t enable; // CH9350 enable
} ch9350_t;

// 初始化CH9329结构体，初始化串口，HID转串口
void CH9350_Init(ch9350_t *ch9350);
void CH9350_Send_Data(ch9350_t *ch9350, uint8_t *data, uint16_t length);

#endif