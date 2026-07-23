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
    uint8_t battery_pct;      // 最新电量百分比 0~100
    uint8_t charge_flags;     // 状态位图（bit0=充电中, bit1=已充满, bit2=输出中）
    uint8_t has_status;       // 是否已收到过状态上报
} power_t;

// 初始化电源结构体，初始化串口并和电源沟通编号和设置
void Power_Init(power_t *power);
void Power_Get_Type(power_t *power);
void Power_Send_Data(power_t *power, uint8_t *data, uint16_t length);
void Power_Process(power_t *power);

// 重发最新电量/充电状态给 Display（供进页拉取一次）
void Power_ReportStatusToDisplay(power_t *power);

// UART 中断处理函数（在中断服务函数中调用）
void Power_UART_IRQ_Handler(power_t *power);

#endif
