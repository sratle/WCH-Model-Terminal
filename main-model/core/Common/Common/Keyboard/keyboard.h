#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "../Protocol/protocol.h"

#define KEYBOARD_UART_BAUDRATE 230400
#define KEYBOARD_UART USART3
#define KEYBOARD_UART_IRQn USART3_IRQn

/* Keyboard UART gpio definitions UART3 */
#define KEYBOARD_UART_TX_PORT GPIOB
#define KEYBOARD_UART_TX_PIN GPIO_Pin_10
#define KEYBOARD_UART_TX_AF GPIO_AF7

#define KEYBOARD_UART_RX_PORT GPIOB
#define KEYBOARD_UART_RX_PIN GPIO_Pin_11
#define KEYBOARD_UART_RX_AF GPIO_AF7

typedef struct
{
    uint8_t type_id; // 键盘类型编号
    protocol_rx_ctx_t rx_ctx; // 协议接收状态机上下文
} keyboard_t;

// 初始化键盘结构体，初始化串口-和键盘沟通编号和设置
void Keyboard_Init(keyboard_t *keyboard);
void Keyboard_Get_Type(keyboard_t *keyboard);
void Keyboard_Send_Data(keyboard_t *keyboard, uint8_t *data, uint16_t length);
void Keyboard_Process(keyboard_t *keyboard);

// UART 中断处理函数（在中断服务函数中调用）
void Keyboard_UART_IRQ_Handler(keyboard_t *keyboard);

/* ============================================================================
 * Game Keyboard (Keyboard-2) Input State
 * ============================================================================ */

typedef struct {
    int8_t  roc1_x;       /* Joystick 1 X (-128~127, center=0) */
    int8_t  roc1_y;       /* Joystick 1 Y (-128~127, center=0) */
    int8_t  roc2_x;       /* Joystick 2 X (-128~127, center=0) */
    int8_t  roc2_y;       /* Joystick 2 Y (-128~127, center=0) */
    uint8_t buttons;      /* bit0~5 = BUT1~BUT6 */
    uint8_t switches;     /* 2 bits per switch: 00=mid, 01=up, 10=down */
    int8_t  ec1_delta;    /* Encoder 1 rotation delta */
    int8_t  ec2_delta;    /* Encoder 2 rotation delta */
    uint8_t ec_buttons;   /* bit0=EC1_D, bit1=EC2_D */
} game_input_state_t;

/* Get current game input state (NULL if no game keyboard connected) */
const game_input_state_t *Keyboard_Game_GetState(void);

/* ============================================================================
 * Music Keyboard (Keyboard-3) Control API
 * ============================================================================ */

/**
 * @brief  启动音乐键盘事件上报
 *         向 Keyboard-3 发送 CMD_KBD_MUSIC_EVENT_CTRL(state=0x01)
 * @return 1=发送成功, 0=键盘未连接或非音乐键盘
 */
uint8_t Keyboard_Music_Start(void);

/**
 * @brief  停止音乐键盘事件上报
 *         向 Keyboard-3 发送 CMD_KBD_MUSIC_EVENT_CTRL(state=0x00)
 * @return 1=发送成功, 0=键盘未连接或非音乐键盘
 */
uint8_t Keyboard_Music_Stop(void);

/**
 * @brief  查询音乐键盘是否处于活动状态（已连接且已启动上报）
 * @return 1=活动, 0=未活动
 */
uint8_t Keyboard_Music_IsActive(void);

#endif
