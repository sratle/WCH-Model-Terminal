/********************************** (C) COPYRIGHT  *******************************
 * File Name          : hardware.c
 * Author             :
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : This file provides all the hardware firmware functions.
 *******************************************************************************/
#include "hardware.h"

#include "CS43131/cs43131.h"
#include "Submodels/submodels.h"
#include "Power/power.h"
#include "Keyboard/keyboard.h"
#include "Display/display.h"
#include "CH9350/ch9350.h"
#include "CH378/CH378.h"
#include "CH585F/ch585f.h"
#include "Key/key.h"
#include "CLI/CLI.h"
#include "Config/config.h"
#include "Protocol/protocol.h"

/**
 * Global variables
 */
cs43131_t CS43131_g;
ch378_t ch378_g;
ch585f_t ch585f_g;
display_t display_g;

power_t power_g;
keyboard_t keyboard_g;
ch9350_t ch9350_g;
submodels_t submodels_g[3];

hardware_t hardware_g;

/*=============================================================================
 *  心跳 / 模块在线检测
 *=============================================================================*/

/* 心跳槽位初始化表：module_id 对应协议中的模块 ID */
static const uint8_t hb_module_ids[HB_MAX_SLOTS] = {
    MODULE_ID_DISPLAY,
    MODULE_ID_KEYBOARD,
    MODULE_ID_POWER,
    MODULE_ID_SUBMODEL_1,
    MODULE_ID_SUBMODEL_2,
    MODULE_ID_SUBMODEL_3,
};

/* 心跳槽位名称（用于日志） */
static const char *hb_slot_names[HB_MAX_SLOTS] = {
    "Display",
    "Keyboard",
    "Power",
    "Submodel1",
    "Submodel2",
    "Submodel3",
};

static void hb_init_slots(void)
{
    uint8_t i;

    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        hardware_g.hb_slots[i].module_id  = hb_module_ids[i];
        hardware_g.hb_slots[i].miss_count = 0;
        hardware_g.hb_slots[i].status     = HB_STATUS_UNKNOWN;
        hardware_g.hb_slots[i].type       = MODULE_TYPE_RESERVED;
        hardware_g.hb_slots[i].subtype    = 0;
    }
    hardware_g.hb_tick = 0;
}

/* 向指定槽位发送 CMD_GET_TYPE */
static void hb_send_get_type(uint8_t slot_idx)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;
    uint8_t dst = hardware_g.hb_slots[slot_idx].module_id;

    len = Protocol_PackFrame(MODULE_ID_CORE, dst, CMD_GET_TYPE, NULL, 0,
                             buf, sizeof(buf));
    if (len == 0)
        return;

    switch (slot_idx)
    {
        case 0: Display_Send_Data(&display_g, buf, len); break;
        case 1: Keyboard_Send_Data(&keyboard_g, buf, len); break;
        case 2: Power_Send_Data(&power_g, buf, len); break;
        case 3: Submodels_Send_Data(&submodels_g[0], buf, len); break;
        case 4: Submodels_Send_Data(&submodels_g[1], buf, len); break;
        case 5: Submodels_Send_Data(&submodels_g[2], buf, len); break;
        default: break;
    }
}

/*********************************************************************
 * @fn      Hardware_Heartbeat
 *
 * @brief   心跳函数，每秒向所有槽位发送 CMD_GET_TYPE，
 *          5 次无回应标记为失联。需在主循环中以 1ms 间隔调用。
 *
 * @return  none
 *********************************************************************/
void Hardware_Heartbeat(void)
{
    uint8_t i;

    hardware_g.hb_tick++;

    if (hardware_g.hb_tick < HB_INTERVAL_MS)
        return;

    hardware_g.hb_tick = 0;

    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        /* 检查上轮是否收到回应 */
        if (hardware_g.hb_slots[i].miss_count > 0)
        {
            hardware_g.hb_slots[i].miss_count++;

            /* 达到失联阈值 */
            if (hardware_g.hb_slots[i].miss_count >= HB_TIMEOUT_LIMIT &&
                hardware_g.hb_slots[i].status != HB_STATUS_OFFLINE)
            {
                hardware_g.hb_slots[i].status = HB_STATUS_OFFLINE;
                printf("[HB] %s OFFLINE\r\n", hb_slot_names[i]);
            }
        }

        /* 发送新一轮 GET_TYPE */
        hb_send_get_type(i);

        /* 设置待确认标记 */
        if (hardware_g.hb_slots[i].miss_count == 0)
            hardware_g.hb_slots[i].miss_count = 1;
    }

    /* 检查是否有 pending 的 RGB 配置需要发送给已在线的 RGB 子模块 */
    if (hardware_g.config_applied)
    {
        for (i = 3; i < HB_MAX_SLOTS; i++)
        {
            if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_SUBMODEL_RGB &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
            {
                uint8_t idx = i - 3;

                /* 如果有自定义帧待传输，优先发送帧数据 */
                if (hardware_g.rgb_frame.pending)
                {
                    uint8_t next_frame = hardware_g.rgb_frame.next_frame_idx;
                    if (next_frame < hardware_g.rgb_frame.frame_count)
                    {
                        Submodels_RGB_SendFrame(&submodels_g[idx], next_frame,
                                                 hardware_g.rgb_frame.frame_data[next_frame]);
                        hardware_g.rgb_frame.next_frame_idx = next_frame + 1;
                    }
                    else
                    {
                        /* 所有帧已发送，发送播放命令 */
                        Submodels_RGB_PlayAnimation(&submodels_g[idx],
                                                    hardware_g.rgb_frame.frame_count,
                                                    hardware_g.rgb_frame.frame_interval);

                        /* 发送 SetMode 切换到自定义模式 */
                        Submodels_RGB_SetMode(&submodels_g[idx],
                                              hardware_g.rgb_config.mode,
                                              hardware_g.rgb_config.r,
                                              hardware_g.rgb_config.g,
                                              hardware_g.rgb_config.b,
                                              hardware_g.rgb_config.brightness,
                                              hardware_g.rgb_config.speed);

                        hardware_g.rgb_frame.pending = 0;
                        hardware_g.rgb_frame.next_frame_idx = 0;
                        hardware_g.rgb_config.pending = 0;
                    }
                    break;
                }

                /* 没有自定义帧待传输，直接发送 SetMode */
                if (hardware_g.rgb_config.pending)
                {
                    Submodels_RGB_SetMode(&submodels_g[idx],
                                          hardware_g.rgb_config.mode,
                                          hardware_g.rgb_config.r,
                                          hardware_g.rgb_config.g,
                                          hardware_g.rgb_config.b,
                                          hardware_g.rgb_config.brightness,
                                          hardware_g.rgb_config.speed);
                    hardware_g.rgb_config.pending = 0;
                }
                break;
            }
        }

        /* 检查是否有 pending 的 SubDisplay 命令需要发送 */
        if (hardware_g.subdisp_req.pending)
        {
            for (i = 3; i < HB_MAX_SLOTS; i++)
            {
                if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY &&
                    hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
                {
                    uint8_t idx = i - 3;

                    switch (hardware_g.subdisp_req.cmd)
                    {
                        case SUBDISP_CMD_SET_MODE:
                            Submodels_SubDisp_SetDisplayMode(&submodels_g[idx],
                                                              hardware_g.subdisp_req.mode);
                            break;
                        case SUBDISP_CMD_REFRESH_STATUS:
                            Submodels_SubDisp_SendSysStatus(&submodels_g[idx]);
                            break;
                        case SUBDISP_CMD_SEND_BMP:
                            Submodels_SubDisp_SendBMP(&submodels_g[idx],
                                                       hardware_g.subdisp_req.filename);
                            /* 自动切换到图片模式 */
                            Submodels_SubDisp_SetDisplayMode(&submodels_g[idx],
                                                              SUBDISP_MODE_IMAGE);
                            break;
                        default:
                            break;
                    }
                    break;  /* 只需发送给第一个在线的 SubDisplay */
                }
            }
            hardware_g.subdisp_req.pending = 0;
        }
    }   /* end if (hardware_g.config_applied) */
}

/*********************************************************************
 * @fn      Hardware_Hb_MarkOnline
 *
 * @brief   由各模块 Process 在收到 GET_TYPE ACK 时调用，
 *          标记对应槽位在线。
 *
 * @param   module_id  - 协议模块 ID
 * @param   type       - 模块类型 (MODULE_TYPE_xxx)
 * @param   subtype    - 模块子类型
 *
 * @return  none
 *********************************************************************/
void Hardware_Hb_MarkOnline(uint8_t module_id, uint8_t type, uint8_t subtype)
{
    uint8_t i;

    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        if (hardware_g.hb_slots[i].module_id == module_id)
        {
            hardware_g.hb_slots[i].miss_count = 0;
            hardware_g.hb_slots[i].type       = type;
            hardware_g.hb_slots[i].subtype    = subtype;

            if (hardware_g.hb_slots[i].status != HB_STATUS_ONLINE)
            {
                hardware_g.hb_slots[i].status = HB_STATUS_ONLINE;
                printf("[HB] %s ONLINE type=0x%02X subtype=0x%02X\r\n",
                       hb_slot_names[i], type, subtype);
            }

            /* RGB 子模块上线时，设置 pending 让心跳统一发送 */
            if (type == MODULE_TYPE_SUBMODEL &&
                subtype == MODULE_SUBTYPE_SUBMODEL_RGB)
            {
                hardware_g.rgb_config.pending = 1;
            }

            /* SubDisplay 子模块上线时，主动发送一次当前状态 */
            if (type == MODULE_TYPE_SUBMODEL &&
                subtype == MODULE_SUBTYPE_SUBMODEL_SUB_DISPLAY)
            {
                hardware_g.subdisp_req.pending = 1;
                hardware_g.subdisp_req.cmd = SUBDISP_CMD_REFRESH_STATUS;
            }
            return;
        }
    }
}

/*********************************************************************
 * @fn      Hardware_Init
 *
 * @brief   Hardware entry - V5F 统一初始化所有模块.
 *
 * @return  none
 *********************************************************************/
void Hardware_Init(void)
{
    memset(&hardware_g, 0, sizeof(hardware_t));

    /* 音频 DAC */
    CS43131_init(&CS43131_g);
    hardware_g.hardware_init_flag |= 0x01;

    /* 文件管理芯片 */
    CH378_Init(&ch378_g);
    hardware_g.hardware_init_flag |= 0x02;

    CH378_Device_Select(&ch378_g, CH378_Device_USB);

    /* 配置系统 */
    Config_Init();

    /* 命令行接口 */
    CLI_Init();

    /* 蓝牙无线芯片 */
    CH585F_Init(&ch585f_g);
    hardware_g.hardware_init_flag |= 0x04;

    /* 屏幕模块 */
    Display_Init(&display_g);
    hardware_g.hardware_init_flag |= 0x08;

    /* 核心按键 */
    Key_Init();
    hardware_g.hardware_init_flag |= 0x10;

    /* 供电模块 */
    Power_Init(&power_g);
    hardware_g.hardware_init_flag |= 0x20;

    /* HID 转串口 */
    CH9350_Init(&ch9350_g);
    hardware_g.hardware_init_flag |= 0x40;

    /* 键盘模块 */
    Keyboard_Init(&keyboard_g);
    hardware_g.hardware_init_flag |= 0x80;

    /* 子模块 */
    Submodels_Init(submodels_g);

    /* 心跳槽位初始化 */
    hb_init_slots();
}
