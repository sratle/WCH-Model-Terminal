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

extern volatile hardware_t hardware_g;

/*=============================================================================
 *  心跳 / 模块在线检测
 *=============================================================================*/

#define HB_MAX_SLOTS        6   /* Display, Keyboard, Power, Submodel1~3 */
#define HB_TIMEOUT_LIMIT    5   /* 连续 N 次无回应标记失联 */
#define HB_INTERVAL_MS      1000 /* 心跳发送间隔 (ms) */

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
} hardware_t;

void Hardware_V5F_Init(void);
void Hardware_V3F_Init(void);

/* 心跳函数，需在主循环中以 1ms 间隔调用 */
void Hardware_Heartbeat(void);

/* 由各模块 Process 在收到 GET_TYPE ACK 时调用，标记槽位在线 */
void Hardware_Hb_MarkOnline(uint8_t module_id, uint8_t type, uint8_t subtype);

#ifdef __cplusplus
}
#endif

#endif 





