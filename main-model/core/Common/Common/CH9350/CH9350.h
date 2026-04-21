#ifndef __CH9350_H
#define __CH9350_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "ch32h417_usart.h"
#include "ch32h417_rcc.h"
#include "core_riscv.h"
#include "debug.h"

/************************* 硬件配置宏定义 *************************/
// CH9350串口配置（默认USART1，可根据硬件修改）
#define CH9350_UART_BAUDRATE 115200 // 必须与CH9350硬件BAUD引脚配置一致
#define CH9350_UART USART1
#define CH9350_UART_IRQn USART1_IRQn

// UART引脚定义（PA9-TX, PA10-RX，可根据硬件修改）
#define CH9350_UART_TX_PORT GPIOA
#define CH9350_UART_TX_PIN GPIO_Pin_9
#define CH9350_UART_TX_AF GPIO_AF7
#define CH9350_UART_RX_PORT GPIOA
#define CH9350_UART_RX_PIN GPIO_Pin_10
#define CH9350_UART_RX_AF GPIO_AF7

/************************* CH9350协议宏定义 *************************/
// 核心帧定义
#define CH9350_FRAME_HEAD1 0x57   // 帧头第1字节
#define CH9350_FRAME_HEAD2 0xAB   // 帧头第2字节
#define CH9350_RX_BUF_MAX_LEN 128 // 接收缓冲区最大长度（单帧最大72字节，留冗余）
#define CH9350_DATA_MAX_LEN 64    // 有效数据最大长度

// 下位机模式操作码定义（来自CH9350手册）
#define CH9350_OP_DEV_CONNECT 0x81    // 设备连接命令
#define CH9350_OP_STATUS_REQ 0x82     // 状态请求命令
#define CH9350_OP_KEY_DATA1 0x83      // 状态1有效键值帧
#define CH9350_OP_KEY_DATA0 0x88      // 状态0有效键值帧
#define CH9350_OP_DEV_DISCONNECT 0x86 // 设备断开命令
#define CH9350_OP_KEYBOARD 0x01       // 状态2/3/4键盘数据帧
#define CH9350_OP_MOUSE_REL 0x02      // 状态2相对鼠标数据帧
#define CH9350_OP_MOUSE_ABS 0x04      // 状态3/4绝对鼠标数据帧

// 设备类型定义
#define CH9350_DEV_NONE 0x00       // 无设备
#define CH9350_DEV_KEYBOARD 0x01   // 键盘设备
#define CH9350_DEV_MOUSE_REL 0x02  // 相对鼠标
#define CH9350_DEV_MOUSE_ABS 0x03  // 绝对鼠标
#define CH9350_DEV_MULTIMEDIA 0x04 // 多媒体设备
#define CH9350_DEV_SCAN_GUN 0x05   // 扫描枪设备

/************************* 数据结构体定义 *************************/
// 键盘数据结构体
typedef struct
{
    uint8_t modifier; // 修饰键（Ctrl/Shift/Alt等）
    uint8_t reserved;
    uint8_t key_code[6]; // 普通按键码（USB标准键盘码）
    uint8_t led_state;   // 键盘指示灯状态
} ch9350_keyboard_t;

// 相对鼠标数据结构体
typedef struct
{
    uint8_t button;  // 鼠标按键（左键/右键/中键）
    int8_t x_offset; // X轴相对偏移
    int8_t y_offset; // Y轴相对偏移
    int8_t wheel;    // 滚轮偏移
} ch9350_mouse_rel_t;

// 绝对鼠标数据结构体
typedef struct
{
    uint8_t report_id; // 固定0x01
    uint8_t button;    // 鼠标按键
    uint16_t x_pos;    // X轴绝对坐标
    uint16_t y_pos;    // Y轴绝对坐标
    int8_t wheel;      // 滚轮偏移
} ch9350_mouse_abs_t;

// CH9350核心管理结构体
typedef struct
{
    uint8_t enable;                           // CH9350使能标志
    uint8_t rx_buffer[CH9350_RX_BUF_MAX_LEN]; // 串口接收原始缓冲区
    uint16_t rx_len;                          // 当前已接收字节数
    uint8_t frame_ready;                      // 完整帧接收完成标志（1=就绪）
    uint8_t frame_error;                      // 帧错误标志（校验/溢出/格式错误）

    // 核心有效数据字段（用户需求的current_data）
    uint8_t current_data[CH9350_DATA_MAX_LEN]; // 当前解析后的完整有效数据
    uint8_t data_len;                          // 有效数据长度
    uint8_t device_type;                       // 当前数据对应的设备类型

    // 解析后的标准化设备数据
    ch9350_keyboard_t keyboard_data;
    ch9350_mouse_rel_t mouse_rel_data;
    ch9350_mouse_abs_t mouse_abs_data;
} ch9350_t;

/************************* 函数声明 *************************/
// 基础初始化与硬件控制
void CH9350_Init(ch9350_t *ch9350);
void CH9350_Send_Data(ch9350_t *ch9350, uint8_t *data, uint16_t length);

// 中断接收与帧处理（中断服务函数内调用）
void CH9350_UART_IRQ_Handler(ch9350_t *ch9350);

// 数据解析与业务处理（主循环内调用）
uint8_t CH9350_Parse_Frame(ch9350_t *ch9350);
uint8_t CH9350_Has_New_Data(ch9350_t *ch9350);
void CH9350_Clear_Data(ch9350_t *ch9350);

#endif