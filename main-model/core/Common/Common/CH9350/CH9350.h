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

/* 下位机→上位机操作码定义（来自CH9350手册） */
#define CH9350_OP_DEV_CONNECT       0x81    // 设备连接命令（状态0/1）
#define CH9350_OP_STATUS_REQ        0x82    // 状态请求命令（状态0/1，需应答）
#define CH9350_OP_KEY_DATA1         0x83    // 状态1有效键值帧
#define CH9350_OP_RESET_DELAY       0x84    // 复位延迟命令（状态1）
#define CH9350_OP_WORK_STATE_CHG    0x85    // 工作状态改变命令（状态0/1）
#define CH9350_OP_DEV_DISCONNECT    0x86    // 设备断开命令（状态1）
#define CH9350_OP_GET_VERSION       0x87    // 获取版本号命令（状态0/1/2/3/4）
#define CH9350_OP_KEY_DATA0         0x88    // 状态0有效键值帧
#define CH9350_OP_TIMEOUT_CFG       0x89    // 超时配置命令（状态1/2/3/4）
#define CH9350_OP_STATUS_CHG        0x80    // 状态改变命令（状态2/3/4，需应答）
#define CH9350_OP_KEYBOARD          0x01    // 状态2/3/4键盘数据帧
#define CH9350_OP_MOUSE_REL         0x02    // 状态2相对鼠标数据帧
#define CH9350_OP_MOUSE_ABS         0x04    // 状态3/4绝对鼠标数据帧

/* 上位机→下位机操作码定义 */
#define CH9350_OP_ACK               0x12    // 应答状态帧（状态1/2/3/4）
#define CH9350_OP_STATE_SWITCH      0x40    // 工作状态切换命令（状态2/3/4）
#define CH9350_OP_VID_PID           0x10    // VID/PID修改命令（状态2/3/4）

/* 工作状态定义 */
#define CH9350_STATE_0              0x00
#define CH9350_STATE_1              0x01
#define CH9350_STATE_2              0x02
#define CH9350_STATE_3              0x03
#define CH9350_STATE_4              0x04

/* 状态0/1数据帧标识字节位定义（手册7.2.4.11） */
#define CH9350_ID_DEV_MASK          0x30    // Bit5&4: 设备类型
#define CH9350_ID_DEV_KEYBOARD      0x10    // 01 = 键盘
#define CH9350_ID_DEV_MOUSE         0x20    // 10 = 鼠标
#define CH9350_ID_DEV_MULTIMEDIA    0x30    // 11 = 多媒体
#define CH9350_ID_DEV_OTHER         0x00    // 00 = 其他
#define CH9350_ID_HID_TYPE_MASK     0x06    // Bit2&1: HID类型
#define CH9350_ID_HID_TYPE_HID      0x02    // 01 = HID
#define CH9350_ID_HID_TYPE_BIOS     0x04    // 10 = BIOS
#define CH9350_ID_PORT_MASK         0x01    // Bit0: 端口
#define CH9350_ID_PORT_1            0x00
#define CH9350_ID_PORT_2            0x01

/* 应答帧固定长度 */
#define CH9350_ACK_FRAME_LEN        11      // 57 AB 12 + 8字节数据

/* 键盘Report值位定义（应答帧） */
#define CH9350_REPORT_NUM_LOCK      0x01
#define CH9350_REPORT_CAPS_LOCK     0x02
#define CH9350_REPORT_SCROLL_LOCK   0x04

// 设备类型定义（解析后的统一类型）
#define CH9350_DEV_NONE             0x00    // 无设备
#define CH9350_DEV_KEYBOARD         0x01    // 键盘设备
#define CH9350_DEV_MOUSE_REL        0x02    // 相对鼠标
#define CH9350_DEV_MOUSE_ABS        0x03    // 绝对鼠标
#define CH9350_DEV_MULTIMEDIA       0x04    // 多媒体设备
#define CH9350_DEV_SCAN_GUN         0x05    // 扫描枪设备

/* 帧接收超时时间（ms） */
#define CH9350_FRAME_TIMEOUT_MS     10

/* 串口发送超时（循环计数上限，约10ms@100MHz） */
#define CH9350_SEND_TIMEOUT         1000000

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
    volatile uint16_t rx_len;                 // 当前已接收字节数（中断修改）
    volatile uint8_t frame_ready;             // 完整帧接收完成标志（1=就绪）
    volatile uint8_t frame_error;             // 帧错误标志（校验/溢出/格式错误）

    // 核心有效数据字段
    uint8_t current_data[CH9350_DATA_MAX_LEN]; // 当前解析后的完整有效数据
    uint8_t data_len;                          // 有效数据长度
    uint8_t device_type;                       // 当前数据对应的设备类型

    // 解析后的标准化设备数据
    ch9350_keyboard_t keyboard_data;
    ch9350_mouse_rel_t mouse_rel_data;
    ch9350_mouse_abs_t mouse_abs_data;

    // 上一次设备数据（用于变化检测，避免刷屏）
    ch9350_keyboard_t prev_keyboard_data;
    ch9350_mouse_rel_t prev_mouse_rel_data;
    ch9350_mouse_abs_t prev_mouse_abs_data;

    // 工作状态与事件跟踪
    volatile uint8_t work_state;              // 当前CH9350工作状态（0~4）
    volatile uint8_t dev_connected;           // 设备连接标志（收到0x81置1）
    volatile uint8_t dev_disconnected;        // 设备断开脉冲（收到0x86置1，读取后由应用清零）
    uint8_t connected_dev_type;               // 连接的设备类型（键盘/鼠标，由DEV_CONNECT帧解析）
    uint8_t version;                          // 下位机版本号（收到0x87更新）

    // 应答帧配置
    uint8_t ack_report;                       // 键盘Report值（Num/Caps/Scroll Lock状态）
    uint16_t pid1;                            // 端口1 PID
    uint16_t pid2;                            // 端口2 PID

    // 新增：应答标志（避免在中断中阻塞发送）
    volatile uint8_t ack_pending;             // 1=有应答待发送
    volatile uint8_t rx_timeout_cnt;          // 帧接收超时计数器（ms，主循环每1ms+1）
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

// 主循环轮询入口：超时检测、应答发送、帧解析、printf输出
void CH9350_Process(ch9350_t *ch9350);

// 应答帧发送（状态0/1收到0x82、状态2/3/4收到0x80时自动调用，也可手动调用）
void CH9350_Send_Ack(ch9350_t *ch9350);

// 上位机主动切换CH9350工作状态（发送0x40命令）
void CH9350_Set_Work_State(ch9350_t *ch9350, uint8_t state);

// 重发当前外接 HID（USB）键盘/鼠标连接状态给 Display（供进页拉取一次）
void CH9350_ReportHidStatusToDisplay(ch9350_t *ch9350);

// USB HID键码转按键名称（已知键返回名称，未知返回NULL）
const char *CH9350_KeyCode_Name(uint8_t keycode);

#endif
