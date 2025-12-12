/*
 * Copyright 2024 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */
#ifndef __CUSTOM_H_
#define __CUSTOM_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include "gui_guider.h"

// 该文件需要包含一些全局的enum定义，event里面涉及的宏定义需要在此处包含，但是函数定义和实现可以分文件
typedef enum
{
    GG_2048_MOVE_LEFT,
    GG_2048_MOVE_RIGHT,
    GG_2048_MOVE_UP,
    GG_2048_MOVE_DOWN,
} gg_2048_move_direction;

typedef enum
{
    TETRIS_MOVE_LEFT,
    TETRIS_MOVE_RIGHT,
    TETRIS_MOVE_UP,
    TETRIS_MOVE_DOWN,
} tetris_move_direction;

void custom_init(lv_ui *ui);

#ifdef __cplusplus
}
#endif
#endif /* EVENT_CB_H_ */
