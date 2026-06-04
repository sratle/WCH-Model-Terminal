/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.h
* Author             : 
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : This file contains all the functions prototypes for the 
*                      hardware.
*******************************************************************************/
#ifndef __HARDWARE_H
#define __HARDWARE_H

#ifdef __cplusplus
 extern "C" {
#endif
#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"
#include "all_devices.h"

/* 模块全局变量声明（供中断服务函数等访问） */

extern power_t power_g;
extern keyboard_t keyboard_g;
extern ch9350_t ch9350_g;
extern submodels_t submodels_g[3];

extern cs43131_t CS43131_g;
extern ch378_t ch378_g;
extern ch585f_t ch585f_g;
extern display_t display_g;

/*=============================================================================
 *  V3F → V5F 跨核配置请求
 *=============================================================================*/

/* 配置请求类型 */
typedef enum {
    CONFIG_REQ_NONE = 0,        /* 无请求 */
    CONFIG_REQ_SET_INT,         /* 设置整型配置 */
    CONFIG_REQ_SET_STRING,      /* 设置字符串配置 */
    CONFIG_REQ_SAVE,            /* 请求保存到文件 */
    CONFIG_REQ_RESET,           /* 请求恢复默认值 */
    CONFIG_REQ_APPLY            /* 请求重新应用配置到硬件 */
} config_req_type_t;

/* 配置请求结构体（V3F 写入，V5F 读取并处理） */
typedef struct {
    config_req_type_t type;     /* 请求类型 */
    uint8_t  pending;           /* 1=有新请求待处理 */
    char     module_key[5];     /* 模块键 "TTSS" */
    char     key[16];           /* 配置键名 */
    int      int_val;           /* 整型值（SET_INT 时使用） */
    char     str_val[32];       /* 字符串值（SET_STRING 时使用） */
} config_request_t;

/*=============================================================================
 *  V5F → V3F RGB 配置传递
 *=============================================================================*/

typedef struct {
    uint8_t pending;        /* 1=V5F 有新的 RGB 配置待发送 */
    uint8_t mode;           /* RGB 模式: 0=自定义, 1=常亮, 2=呼吸, 3=跑马灯 */
    uint8_t r, g, b;       /* 基础颜色 RGB888 */
    uint8_t brightness;    /* 亮度 0-255 */
    uint8_t speed;         /* 速度 0-255 */
} rgb_config_t;

/*=============================================================================
 *  V3F → V5F Core 按键事件
 *=============================================================================*/

#define CORE_KEY_QUEUE_SIZE  8

typedef struct {
    uint8_t key_id;
    uint8_t event;
} core_key_event_t;

typedef struct {
    core_key_event_t queue[CORE_KEY_QUEUE_SIZE];
    volatile uint8_t head;
    volatile uint8_t tail;
} core_key_queue_t;

/*=============================================================================
 *  心跳 / 模块在线检测
 *=============================================================================*/

#define HB_MAX_SLOTS        6   /* Display, Keyboard, Power, Submodel1~3 */
#define HB_TIMEOUT_LIMIT    8   /* 连续 N 次无回应标记失联 */
#define HB_INTERVAL_MS      500 /* 心跳发送间隔 (ms) */

/* 模块在线状态 */
typedef enum {
    HB_STATUS_UNKNOWN = 0,  /* 初始/未探测 */
    HB_STATUS_ONLINE,       /* 在线（已收到 GET_TYPE ACK） */
    HB_STATUS_OFFLINE       /* 失联（连续 HB_TIMEOUT_LIMIT 次无回应） */
} hb_status_t;

/* 心跳槽位 */
typedef struct {
    uint8_t     module_id;      /* 协议模块 ID (MODULE_ID_DISPLAY 等) */
    uint8_t     miss_count;     /* 连续无回应计数 */
    hb_status_t status;         /* 当前在线状态 */
    uint8_t     type;           /* 模块类型 (MODULE_TYPE_xxx) */
    uint8_t     subtype;        /* 模块子类型 */
} hb_slot_t;

typedef struct
{
    uint8_t hardware_state;     /* Hardware state */
    uint8_t hardware_init_flag; /* Hardware init flag */
    hb_slot_t hb_slots[HB_MAX_SLOTS]; /* 心跳槽位 */
    uint32_t hb_tick;           /* 心跳计时计数器 (ms) */
    config_request_t config_req; /* V3F→V5F 跨核配置请求 */
    rgb_config_t    rgb_config; /* V5F→V3F RGB 配置传递 */
    core_key_queue_t key_queue;  /* V3F→V5F 核心按键事件队列 */
} hardware_t;

extern volatile hardware_t hardware_g;

void Hardware_V5F_Init(void);
void Hardware_V3F_Init(void);

/* 心跳函数，需在主循环中以 1ms 间隔调用 */
void Hardware_Heartbeat(void);

/* 由各模块 Process 在收到 GET_TYPE ACK 时调用，标记槽位在线 */
void Hardware_Hb_MarkOnline(uint8_t module_id, uint8_t type, uint8_t subtype);

void Hardware_KeyQueue_Push(uint8_t key_id, uint8_t event);
uint8_t Hardware_KeyQueue_Pop(core_key_event_t *evt);

#ifdef __cplusplus
}
#endif

#endif 





