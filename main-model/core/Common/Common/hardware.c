/********************************** (C) COPYRIGHT  *******************************
 * File Name          : hardware.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : This file provides all the hardware firmware functions.
 *******************************************************************************/
#include "hardware.h"
#include "shared.h"
#include "ch32h417_hsem.h"

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
// V5F hardware
cs43131_t CS43131_g;
ch378_t ch378_g;
ch585f_t ch585f_g;
display_t display_g;

// V3F hardware
power_t power_g;
keyboard_t keyboard_g;
ch9350_t ch9350_g;
submodels_t submodels_g[3];

__attribute__((section(".shared_data")))
volatile hardware_t hardware_g;

void Shared_Init(void)
{
    volatile hardware_t *p = &hardware_g;
    uint8_t *dst = (uint8_t *)p;
    uint32_t len = sizeof(hardware_t);
    while (len--)
        *dst++ = 0;
}

/*=============================================================================
 *  V3F → V5F Core 按键事件队列
 *=============================================================================*/

void Hardware_KeyQueue_Push(uint8_t key_id, uint8_t event)
{
    uint8_t next;

    if (HSEM_FastTake(HSEM_ID1) != READY)
        return;

    next = (hardware_g.key_queue.head + 1) % CORE_KEY_QUEUE_SIZE;
    if (next == hardware_g.key_queue.tail)
    {
        HSEM_ReleaseOneSem(HSEM_ID1, 0);
        return;
    }

    hardware_g.key_queue.queue[hardware_g.key_queue.head].key_id = key_id;
    hardware_g.key_queue.queue[hardware_g.key_queue.head].event = event;
    hardware_g.key_queue.head = next;

    HSEM_ReleaseOneSem(HSEM_ID1, 0);
}

uint8_t Hardware_KeyQueue_Pop(core_key_event_t *evt)
{
    if (HSEM_FastTake(HSEM_ID1) != READY)
        return 0;

    if (hardware_g.key_queue.tail == hardware_g.key_queue.head)
    {
        HSEM_ReleaseOneSem(HSEM_ID1, 0);
        return 0;
    }

    evt->key_id = hardware_g.key_queue.queue[hardware_g.key_queue.tail].key_id;
    evt->event  = hardware_g.key_queue.queue[hardware_g.key_queue.tail].event;
    hardware_g.key_queue.tail = (hardware_g.key_queue.tail + 1) % CORE_KEY_QUEUE_SIZE;

    HSEM_ReleaseOneSem(HSEM_ID1, 0);
    return 1;
}

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

/* 心跳槽位对应的初始化标志位 */
static const uint8_t hb_init_flags[HB_MAX_SLOTS] = {
    0x08,   /* Display  -> V5F bit3 */
    0x20,   /* Keyboard -> V3F bit5 */
    0x10,   /* Power    -> V3F bit4 */
    0x80,   /* Submodel1 -> V3F bit7 */
    0x80,   /* Submodel2 -> V3F bit7 */
    0x80,   /* Submodel3 -> V3F bit7 */
};

#define HB_INIT_MAGIC  0xDEADDEADu

static void hb_init_slots(void)
{
    uint8_t i;

    /* 防止双核重复初始化：V3F 先调用写入 magic，V5F 检测并跳过 */
    if (hardware_g.hb_tick == HB_INIT_MAGIC)
        return;

    for (i = 0; i < HB_MAX_SLOTS; i++)
    {
        hardware_g.hb_slots[i].module_id  = hb_module_ids[i];
        hardware_g.hb_slots[i].miss_count = 0;
        hardware_g.hb_slots[i].status     = HB_STATUS_UNKNOWN;
        hardware_g.hb_slots[i].type       = MODULE_TYPE_RESERVED;
        hardware_g.hb_slots[i].subtype    = 0;
    }
    hardware_g.hb_tick = HB_INIT_MAGIC;
}

/* 向指定槽位发送 CMD_GET_TYPE（仅当模块已初始化时） */
static void hb_send_get_type(uint8_t slot_idx)
{
    uint8_t buf[PROTO_MAX_FRAME_LEN];
    uint16_t len;
    uint8_t dst = hardware_g.hb_slots[slot_idx].module_id;

    /* 模块未初始化则跳过 */
    if (!(hardware_g.hardware_init_flag & hb_init_flags[slot_idx]))
        return;

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
        /* 模块未初始化则跳过 */
        if (!(hardware_g.hardware_init_flag & hb_init_flags[i]))
            continue;

        /* 检查上轮是否收到回应：
         * MarkOnline 会将 miss_count 清零，
         * 如果 miss_count > 0 说明上轮无回应 */
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

        /* 设置待确认标记：miss_count==0 表示上轮收到了回应，
         * 设为 1 表示"等待本轮回应"，MarkOnline 会清零 */
        if (hardware_g.hb_slots[i].miss_count == 0)
            hardware_g.hb_slots[i].miss_count = 1;
    }

    /* 检查是否有 pending 的 RGB 配置需要发送给已在线的 RGB 子模块 */
    if (hardware_g.rgb_config.pending)
    {
        for (i = 3; i < HB_MAX_SLOTS; i++)
        {
            if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_SUBMODEL_RGB &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
            {
                uint8_t idx = i - 3;
                Submodels_RGB_SetMode(&submodels_g[idx],
                                      hardware_g.rgb_config.mode,
                                      hardware_g.rgb_config.r,
                                      hardware_g.rgb_config.g,
                                      hardware_g.rgb_config.b,
                                      hardware_g.rgb_config.brightness,
                                      hardware_g.rgb_config.speed);
                hardware_g.rgb_config.pending = 0;
                break;
            }
        }
    }

    /* 检查是否有 pending 的 RGB 自定义帧需要传输 */
    if (hardware_g.rgb_frame.pending)
    {
        for (i = 3; i < HB_MAX_SLOTS; i++)
        {
            if (hardware_g.hb_slots[i].subtype == MODULE_SUBTYPE_SUBMODEL_RGB &&
                hardware_g.hb_slots[i].status == HB_STATUS_ONLINE)
            {
                uint8_t idx = i - 3;
                uint8_t f;

                /* 逐帧发送自定义帧数据 */
                for (f = 0; f < hardware_g.rgb_frame.frame_count; f++)
                {
                    Submodels_RGB_SendFrame(&submodels_g[idx], f,
                                             hardware_g.rgb_frame.frame_data[f]);
                }

                /* 发送播放命令 */
                Submodels_RGB_PlayAnimation(&submodels_g[idx],
                                            hardware_g.rgb_frame.frame_count,
                                            hardware_g.rgb_frame.frame_interval);

                hardware_g.rgb_frame.pending = 0;
                break;
            }
        }
    }
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

                /* RGB 子模块上线时，发送当前 RGB 配置（无论 pending 状态）
                 * 确保从 OFFLINE 恢复后也能重新设置模式 */
                if (type == MODULE_TYPE_SUBMODEL &&
                    subtype == MODULE_SUBTYPE_SUBMODEL_RGB)
                {
                    uint8_t idx = i - 3; /* slot 3,4,5 → submodels_g[0,1,2] */
                    if (idx < 3) {
                        Submodels_RGB_SetMode(&submodels_g[idx],
                                              hardware_g.rgb_config.mode,
                                              hardware_g.rgb_config.r,
                                              hardware_g.rgb_config.g,
                                              hardware_g.rgb_config.b,
                                              hardware_g.rgb_config.brightness,
                                              hardware_g.rgb_config.speed);
                        hardware_g.rgb_config.pending = 0;
                    }
                }
            }
            return;
        }
    }
}

/*********************************************************************
 * @fn      Hardware_init
 *
 * @brief   Hardware entry.
 *
 * @return  none
 *********************************************************************/
void Hardware_V3F_Init(void)
{
    Key_Init();
    Power_Init(&power_g);
    hardware_g.hardware_init_flag |= 0x10;

    Keyboard_Init(&keyboard_g);
    hardware_g.hardware_init_flag |= 0x20;

    CH9350_Init(&ch9350_g);
    hardware_g.hardware_init_flag |= 0x40;

    Submodels_Init(submodels_g);
    hardware_g.hardware_init_flag |= 0x80;

    hb_init_slots();

    // while (hardware_g.hardware_init_flag != 0xFF);
}

void Hardware_V5F_Init(void)
{
    CS43131_init(&CS43131_g);
    hardware_g.hardware_init_flag |= 0x01;

    CH378_Init(&ch378_g);
    hardware_g.hardware_init_flag |= 0x02;

    CH378_Device_Select(&ch378_g, CH378_Device_USB);

    Config_Init();

    CLI_Init();

    CH585F_Init(&ch585f_g);
    hardware_g.hardware_init_flag |= 0x04;

    Display_Init(&display_g);
    hardware_g.hardware_init_flag |= 0x08;

    hb_init_slots();

    // while (hardware_g.hardware_init_flag != 0xFF);
}