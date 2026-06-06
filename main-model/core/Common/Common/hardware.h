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
 *  RGB 配置
 *=============================================================================*/

typedef struct {
    uint8_t pending;        /* 1=有新的 RGB 配置待发送 */
    uint8_t mode;           /* RGB 模式: 0=自定义, 1=常亮, 2=呼吸, 3=跑马灯 */
    uint8_t r, g, b;       /* 基础颜色 RGB888 */
    uint8_t brightness;    /* 亮度 0-255 */
    uint8_t speed;         /* 速度 0-255 */
} rgb_config_t;

/*=============================================================================
 *  RGB 自定义帧传输
 *=============================================================================*/

#define RGB_LED_COUNT           49
#define RGB_FRAME_DATA_SIZE     (RGB_LED_COUNT * 3)  /* 147 bytes RGB888 */
#define RGB_MAX_CUSTOM_FRAMES   20

typedef struct {
    uint8_t pending;                        /* 1=有帧数据待传输 */
    uint8_t frame_count;                    /* 总帧数 (1~20) */
    uint8_t next_frame_idx;                 /* 下一帧待发送索引 */
    uint16_t frame_interval;                /* 帧间隔 (ms) */
    uint8_t frame_data[RGB_MAX_CUSTOM_FRAMES][RGB_FRAME_DATA_SIZE]; /* 帧数据 */
} rgb_frame_transfer_t;

/*=============================================================================
 *  SubDisplay 命令
 *=============================================================================*/

#define SUBDISP_CMD_NONE           0
#define SUBDISP_CMD_SET_MODE       1   /* 切换显示模式 */
#define SUBDISP_CMD_REFRESH_STATUS 2   /* 刷新状态数据 */
#define SUBDISP_CMD_SEND_BMP       3   /* 发送 BMP 图片 */

#define SUBDISP_FILENAME_MAX_LEN   32

typedef struct {
    uint8_t  pending;        /* 1=有新命令待处理 */
    uint8_t  cmd;            /* 命令类型 (SUBDISP_CMD_xxx) */
    uint8_t  mode;           /* 显示模式 (SUBDISP_CMD_SET_MODE 时使用) */
    char     filename[SUBDISP_FILENAME_MAX_LEN]; /* BMP 文件名 */
} subdisp_request_t;

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

/*=============================================================================
 *  系统全局状态（原跨核共享，现由 V5F 独占管理）
 *=============================================================================*/

typedef struct
{
    uint8_t hardware_state;     /* Hardware state */
    uint8_t hardware_init_flag; /* Hardware init flag */
    hb_slot_t hb_slots[HB_MAX_SLOTS]; /* 心跳槽位 */
    uint32_t hb_tick;           /* 心跳计时计数器 (ms) */
    uint8_t  config_applied;    /* Config_Apply 已执行，1=配置已就绪 */
    rgb_config_t    rgb_config; /* RGB 配置 */
    rgb_frame_transfer_t rgb_frame; /* RGB 自定义帧传输 */
    subdisp_request_t subdisp_req; /* SubDisplay 命令 */
} hardware_t;

extern hardware_t hardware_g;

void Hardware_Init(void);

/* 心跳函数，需在主循环中以 1ms 间隔调用 */
void Hardware_Heartbeat(void);

/* 由各模块 Process 在收到 GET_TYPE ACK 时调用，标记槽位在线 */
void Hardware_Hb_MarkOnline(uint8_t module_id, uint8_t type, uint8_t subtype);

#ifdef __cplusplus
}
#endif

#endif
