#ifndef __SUBMODELS_H
#define __SUBMODELS_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "../Protocol/protocol.h"

#define SUBMODELS_UART_BAUDRATE 115200

#define SUBMODELS1_UART USART6
#define SUBMODELS2_UART USART7
#define SUBMODELS3_UART USART8

#define SUBMODELS1_UART_IRQn USART6_IRQn
#define SUBMODELS2_UART_IRQn USART7_IRQn
#define SUBMODELS3_UART_IRQn USART8_IRQn

/* Submodels UART gpio definitions UART6/7/8 */
#define SUBMODELS1_UART_TX_PORT GPIOA
#define SUBMODELS1_UART_TX_PIN GPIO_Pin_0
#define SUBMODELS1_UART_TX_AF GPIO_AF8

#define SUBMODELS1_UART_RX_PORT GPIOA
#define SUBMODELS1_UART_RX_PIN GPIO_Pin_1
#define SUBMODELS1_UART_RX_AF GPIO_AF8

#define SUBMODELS2_UART_TX_PORT GPIOB
#define SUBMODELS2_UART_TX_PIN GPIO_Pin_6
#define SUBMODELS2_UART_TX_AF GPIO_AF14

#define SUBMODELS2_UART_RX_PORT GPIOB
#define SUBMODELS2_UART_RX_PIN GPIO_Pin_5
#define SUBMODELS2_UART_RX_AF GPIO_AF14

#define SUBMODELS3_UART_TX_PORT GPIOA
#define SUBMODELS3_UART_TX_PIN GPIO_Pin_15
#define SUBMODELS3_UART_TX_AF GPIO_AF11

#define SUBMODELS3_UART_RX_PORT GPIOA
#define SUBMODELS3_UART_RX_PIN GPIO_Pin_8
#define SUBMODELS3_UART_RX_AF GPIO_AF11

typedef struct {
    uint8_t submodels_id; // Submodels id
    uint8_t type_id; // Submodels type id
    protocol_rx_ctx_t rx_ctx; // 协议接收状态机上下文
} submodels_t;

// 初始化Submodels结构体，初始化串口，入口参数是Submodels结构体数组，长度为3
void Submodels_Init(submodels_t *submodels);

// 获取Submodels类型，入口参数是Submodels结构体指针
void Submodels_Get_Type(submodels_t *submodel);

// 发送数据，入口参数是Submodels结构体指针，发送数据，发送数据长度
void Submodels_Send_Data(submodels_t *submodel, uint8_t *data, uint16_t length);
void Submodels_Process(submodels_t *submodel);

// UART 中断处理函数（在中断服务函数中调用）
void Submodels_UART_IRQ_Handler(submodels_t *submodel, USART_TypeDef *USARTx);

#endif
