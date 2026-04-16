#ifndef __CH9329_H
#define __CH9329_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

/* CH9329 UART gpio definitions UART1 */
#define CH9329_UART_TX_PORT GPIOA
#define CH9329_UART_TX_PIN GPIO_Pin_9
#define CH9329_UART_TX_AF GPIO_AF7

#define CH9329_UART_RX_PORT GPIOA
#define CH9329_UART_RX_PIN GPIO_Pin_10
#define CH9329_UART_RX_AF GPIO_AF7

typedef struct {
    uint8_t enable; // CH9329 enable
} ch9329_t;

// 初始化CH9329结构体，初始化串口，HID转串口
void CH9329_Init(ch9329_t *ch9329);

#endif