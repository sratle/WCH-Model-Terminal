#ifndef __POWER_H
#define __POWER_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

#define POWER_UART_BAUDRATE 115200
#define POWER_UART USART5

/* Power UART gpio definitions UART5 */
#define POWER_UART_TX_PORT GPIOE
#define POWER_UART_TX_PIN GPIO_Pin_3
#define POWER_UART_TX_AF GPIO_AF11

#define POWER_UART_RX_PORT GPIOE
#define POWER_UART_RX_PIN GPIO_Pin_2
#define POWER_UART_RX_AF GPIO_AF4

typedef struct
{
    uint8_t type_id; // 键盘类型编号
} power_t;

// 初始化电源结构体，初始化串口并和电源沟通编号和设置
void Power_Init(power_t *power);
void Power_Get_Type(power_t *power);
void Power_Send_Data(power_t *power, uint8_t *data, uint16_t length);

#endif