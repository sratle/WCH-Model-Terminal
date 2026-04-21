#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

#define DISPLAY_UART_BAUDRATE 115200
#define DISPLAY_UART USART4

/* Display UART gpio definitions UART4 */
#define DISPLAY_UART_TX_PORT GPIOF
#define DISPLAY_UART_TX_PIN GPIO_Pin_4
#define DISPLAY_UART_TX_AF GPIO_AF7

#define DISPLAY_UART_RX_PORT GPIOF
#define DISPLAY_UART_RX_PIN GPIO_Pin_3
#define DISPLAY_UART_RX_AF GPIO_AF7

typedef struct
{
    uint8_t type_id; // 屏幕类型编号
} display_t;

// 初始化屏幕结构体，初始化串口-和屏幕沟通编号和设置
void Display_Init(display_t *display);
void Display_Get_Type(display_t *display);
void Display_Send_Data(display_t *display, uint8_t *data, uint16_t length);

#endif