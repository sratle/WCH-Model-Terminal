#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "../Protocol/protocol.h"

#define DISPLAY_UART_BAUDRATE 921600
#define DISPLAY_UART USART4
#define DISPLAY_UART_IRQn USART4_IRQn

/* Display UART gpio definitions UART4 */
#define DISPLAY_UART_TX_PORT GPIOF
#define DISPLAY_UART_TX_PIN GPIO_Pin_4
#define DISPLAY_UART_TX_AF GPIO_AF7

#define DISPLAY_UART_RX_PORT GPIOF
#define DISPLAY_UART_RX_PIN GPIO_Pin_3
#define DISPLAY_UART_RX_AF GPIO_AF7

/* Bulk transfer default block size (20KB) */
#define DISPLAY_BULK_BLOCK_SIZE  20480

/* Bulk transfer state */
typedef enum {
    BULK_IDLE = 0,
    BULK_READING,   /* Core -> Display file read in progress */
    BULK_WRITING    /* Display -> Core file write in progress */
} bulk_state_t;

typedef struct
{
    uint8_t type_id;              /* 屏幕类型编号 */
    uint8_t type_requested;       /* CMD_GET_TYPE 已发送标志 */
    uint8_t type_received;        /* CMD_GET_TYPE ACK 已收到标志 */
    module_identity_t identity;   /* Display 模块身份信息 */
    protocol_rx_ctx_t rx_ctx;     /* 协议接收状态机上下文 */
    /* Bulk transfer state */
    bulk_state_t bulk_state;
    uint32_t bulk_total_size;     /* Total file size for current bulk transfer */
    uint32_t bulk_received;       /* Bytes received so far in current bulk transfer */
    uint16_t bulk_block_size;     /* Block size for current bulk transfer */
} display_t;

/* 初始化屏幕结构体，初始化串口 */
void Display_Init(display_t *display);

/* 获取Display类型信息 */
void Display_Get_Type(display_t *display);

/* 发送原始数据到Display UART */
void Display_Send_Data(display_t *display, const uint8_t *data, uint16_t length);

/* 主循环帧处理（在主循环中调用） */
void Display_Process(display_t *display);

/* UART 中断处理函数（在中断服务函数中调用） */
void Display_UART_IRQ_Handler(display_t *display);

/* ============================================================================
 * Core 主动发送函数（Core -> Display 方向的事件/状态上报）
 * ============================================================================ */

/* 发送输入事件给 Display（键盘/鼠标/Core按键/BLE HID） */
void Display_SendInputEvent(display_t *display, uint8_t dev_type,
                            const uint8_t *report, uint8_t report_len);

/* 发送外接 HID 设备连接/断开状态给 Display */
void Display_SendHidStatus(display_t *display, uint8_t dev_type, uint8_t connected);

/* 发送音乐播放状态给 Display */
void Display_SendMusicStatus(display_t *display);

/* 发送模块插拔/在线/离线事件给 Display */
void Display_SendModuleStatus(display_t *display, uint8_t evt_type,
                              uint8_t module_id, uint8_t module_type,
                              uint8_t module_subtype);

/* 发送完整模块列表给 Display */
void Display_SendModuleList(display_t *display);

/* 发送文件列表给 Display */
void Display_SendFileList(display_t *display, uint8_t status,
                          const uint8_t *list_data, uint16_t list_len);

/* 发送蓝牙事件给 Display */
void Display_SendBtEvent(display_t *display, uint8_t evt_type,
                         const uint8_t *evt_data, uint8_t evt_data_len);

/* 发送 Submodel 事件给 Display */
void Display_SendSubmodelEvent(display_t *display, uint8_t sub_type,
                               uint8_t sub_cmd,
                               const uint8_t *evt_data, uint8_t evt_data_len);

/* 发送电源事件给 Display */
void Display_SendPowerEvent(display_t *display, uint8_t evt_type,
                            const uint8_t *evt_data, uint8_t evt_data_len);

/* 发送配置结果给 Display */
void Display_SendConfigResult(display_t *display, uint8_t result,
                              uint8_t err_code,
                              const uint8_t *config_data, uint16_t config_len);

/* 发送状态栏更新给 Display */
void Display_SendUpdateStatus(display_t *display, const uint8_t *status_data,
                              uint8_t status_len);

/* 发送屏幕控制命令给 Display */
void Display_SendScreenControl(display_t *display, uint8_t ctrl_type,
                               uint8_t param);

/* 发送通知弹窗给 Display */
void Display_SendNotice(display_t *display, uint8_t priority,
                        const char *title, const char *content);

/* 发送副屏内容给 Display */
void Display_SendSubdispContent(display_t *display, uint8_t content_type,
                                const uint8_t *content, uint16_t content_len);

/* 发送 CLI 命令响应给 Display */
void Display_SendCLIResponse(display_t *display, const char *output, uint16_t output_len);

#endif
