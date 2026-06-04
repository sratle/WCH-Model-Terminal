#ifndef __POWER_H
#define __POWER_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "../Protocol/protocol.h"

#define POWER_UART_BAUDRATE 230400
#define POWER_UART USART5
#define POWER_UART_IRQn USART5_IRQn

/* Power UART gpio definitions UART5 */
#define POWER_UART_TX_PORT GPIOE
#define POWER_UART_TX_PIN GPIO_Pin_3
#define POWER_UART_TX_AF GPIO_AF11

#define POWER_UART_RX_PORT GPIOE
#define POWER_UART_RX_PIN GPIO_Pin_2
#define POWER_UART_RX_AF GPIO_AF4

typedef struct
{
    uint8_t type_id; // 供电类型编号
    protocol_rx_ctx_t rx_ctx; // 协议接收状态机上下文
} power_t;

// 初始化电源结构体，初始化串口并和电源沟通编号和设置
void Power_Init(power_t *power);
void Power_Get_Type(power_t *power);
void Power_Send_Data(power_t *power, uint8_t *data, uint16_t length);
void Power_Process(power_t *power);

// UART 中断处理函数（在中断服务函数中调用）
void Power_UART_IRQ_Handler(power_t *power);

#endif
