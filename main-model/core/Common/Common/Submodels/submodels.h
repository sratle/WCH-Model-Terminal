#ifndef __SUBMODELS_H
#define __SUBMODELS_H

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "../Protocol/protocol.h"

#define SUBMODELS_UART_BAUDRATE 230400

/* ============================================================================
 * RGB Submodel Constants
 * (also defined in hardware.h for shared memory; guarded to avoid conflicts)
 * ============================================================================ */
#ifndef RGB_LED_COUNT
#define RGB_LED_COUNT           49      /* 7x7 matrix */
#endif
#ifndef RGB_FRAME_DATA_SIZE
#define RGB_FRAME_DATA_SIZE     (RGB_LED_COUNT * 3)  /* 147 bytes RGB888 */
#endif
#ifndef RGB_MAX_CUSTOM_FRAMES
#define RGB_MAX_CUSTOM_FRAMES   20
#endif

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

/* ============================================================================
 * RGB Submodel Control API (type = 0x05)
 * Core → RGB submodel-5 命令发送函数
 * ============================================================================ */

/**
 * @brief  设置 RGB 灯效模式
 * @param  submodel   目标 submodel 实例指针（需为 RGB 类型）
 * @param  mode       模式: 0=自定义, 1=常亮, 2=呼吸, 3=跑马灯
 * @param  r, g, b    基础颜色 RGB888（自定义模式下忽略）
 * @param  brightness 全局亮度 0-255
 * @param  speed      动画速度 0-255
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_RGB_SetMode(submodels_t *submodel, uint8_t mode,
                               uint8_t r, uint8_t g, uint8_t b,
                               uint8_t brightness, uint8_t speed);

/**
 * @brief  传输一帧自定义 RGB888 动画数据
 * @param  submodel   目标 submodel 实例指针
 * @param  frame_idx  帧序号 (0 ~ RGB_MAX_CUSTOM_FRAMES-1)
 * @param  rgb_data   RGB888 数据, 49*3=147 字节
 *                     排列: [LED0_R, LED0_G, LED0_B, LED1_R, ..., LED48_B]
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_RGB_SendFrame(submodels_t *submodel, uint8_t frame_idx,
                                 const uint8_t *rgb_data);

/**
 * @brief  启动自定义帧动画播放
 * @param  submodel       目标 submodel 实例指针
 * @param  frame_count    总帧数 (1 ~ RGB_MAX_CUSTOM_FRAMES)
 * @param  frame_interval 帧间隔 (ms, 50~1000)
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_RGB_PlayAnimation(submodels_t *submodel, uint8_t frame_count,
                                     uint16_t frame_interval);

/**
 * @brief  查询 RGB 当前状态（异步，响应由 Submodels_Process 处理）
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_RGB_QueryStatus(submodels_t *submodel);

/**
 * @brief  向 submodel 发送通用命令（fire-and-forget）
 * @param  submodel  目标 submodel 实例指针
 * @param  cmd       操作码
 * @param  data      数据域（可为 NULL）
 * @param  data_len  数据域长度
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_SendCommand(submodels_t *submodel, uint8_t cmd,
                               const uint8_t *data, uint8_t data_len);

/**
 * @brief  在 submodels_g[0..2] 中查找 RGB 类型 submodel
 * @return RGB submodel 指针, 未找到返回 NULL
 */
submodels_t *Submodels_FindRgbSlot(void);

/* ============================================================================
 * SubDisplay (副屏, type=0x07) Control API
 * Core → SubDisp 命令发送函数
 * ============================================================================ */

/**
 * @brief  发送 BMP 图片到副屏（多帧传输）
 *         从 CH378 的 \BMP 目录读取指定 BMP 文件，解析为 1bit 位图后传输
 * @param  submodel   目标 submodel 实例指针（需为 SubDisplay 类型）
 * @param  filename   BMP 文件名（不含路径和后缀，如 "logo"）
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_SubDisp_SendBMP(submodels_t *submodel, const char *filename);

/**
 * @brief  发送 BMP 文件名到副屏（在图片传输前调用）
 * @param  submodel   目标 submodel 实例指针
 * @param  filename   BMP 文件名（不含路径和后缀）
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_SubDisp_SendBmpName(submodels_t *submodel, const char *filename);

/**
 * @brief  发送设备列表到副屏（多帧传输）
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_SubDisp_SendLsDev(submodels_t *submodel);

/**
 * @brief  发送系统状态到副屏（多帧传输）
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_SubDisp_SendSysStatus(submodels_t *submodel);

/**
 * @brief  切换副屏显示模式
 * @param  submodel   目标 submodel 实例指针
 * @param  mode       显示模式: 0=状态显示(双页轮换), 1=图片显示
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_SubDisp_SetDisplayMode(submodels_t *submodel, uint8_t mode);

/**
 * @brief  在 submodels_g[0..2] 中查找 SubDisplay 类型 submodel
 * @return SubDisplay submodel 指针, 未找到返回 NULL
 */
submodels_t *Submodels_FindSubDispSlot(void);

/* ============================================================================
 * Fingerprint (指纹识别, type=0x01) Control API
 * Core → Fingerprint 命令发送函数
 * ============================================================================ */

/**
 * @brief  在 submodels_g[0..2] 中查找 Fingerprint 类型 submodel
 * @return Fingerprint submodel 指针, 未找到返回 NULL
 */
submodels_t *Submodels_FindFpSlot(void);

/**
 * @brief  开始注册新指纹
 * @param  submodel   目标 submodel 实例指针（需为 Fingerprint 类型）
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_EnrollStart(submodels_t *submodel);

/**
 * @brief  取消注册
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_EnrollCancel(submodels_t *submodel);

/**
 * @brief  删除指定指纹
 * @param  submodel   目标 submodel 实例指针
 * @param  fp_id      指纹 ID (0~255)
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_Delete(submodels_t *submodel, uint8_t fp_id);

/**
 * @brief  清空所有指纹
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_DeleteAll(submodels_t *submodel);

/**
 * @brief  查询指纹数量（异步，响应由 Submodels_Process 处理）
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_QueryCount(submodels_t *submodel);

/**
 * @brief  查询指纹列表（异步，响应由 Submodels_Process 处理）
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_QueryList(submodels_t *submodel);

/**
 * @brief  设置 LED 灯效
 * @param  submodel   目标 submodel 实例指针
 * @param  func       功能码 (FP_LED_BREATH, FP_LED_ON, etc.)
 * @param  color      颜色 (FP_COLOR_BLUE, etc.)
 * @param  speed      速度 0~255
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_SetLED(submodels_t *submodel, uint8_t func,
                             uint8_t color, uint8_t speed);

/**
 * @brief  设置安全等级
 * @param  submodel   目标 submodel 实例指针
 * @param  level      安全等级 (FP_SECURITY_LOW/MEDIUM/HIGH)
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_SetSecurity(submodels_t *submodel, uint8_t level);

/**
 * @brief  传感器休眠
 * @param  submodel   目标 submodel 实例指针
 * @return 1=发送成功, 0=失败
 */
uint8_t Submodels_FP_Sleep(submodels_t *submodel);

#endif
