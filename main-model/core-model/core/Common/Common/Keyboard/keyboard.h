#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

/* Keyboard UART gpio definitions UART3 */
#define KEYBOARD_UART_TX_PORT GPIOB
#define KEYBOARD_UART_TX_PIN GPIO_Pin_10
#define KEYBOARD_UART_TX_AF GPIO_AF7

#define KEYBOARD_UART_RX_PORT GPIOB
#define KEYBOARD_UART_RX_PIN GPIO_Pin_11
#define KEYBOARD_UART_RX_AF GPIO_AF7

typedef struct {
    uint8_t type_id; // 键盘类型编号
} keyboard_t;

// 初始化键盘结构体，初始化串口-和键盘沟通编号和设置
void Keyboard_Init(keyboard_t *keyboard);

#endif
