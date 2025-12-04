/*
 * Copyright 2024 NXP
 * NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
 * accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
 * comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
 * terms, then you may not retain, install, activate or otherwise use the software.
 */

/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lvgl.h"
#include "custom.h"

/*********************
 *      DEFINES
 *********************/
#define MATRIX_DIMEN 4
#define MAP_LENGTH (MATRIX_DIMEN * MATRIX_DIMEN + MATRIX_DIMEN)

// 定义2048按键组在不同数字下的颜色
#define BUTTON_COLOR_EMPTY lv_color_hex(0xD8C9AC)
#define BUTTON_COLOR_2 lv_color_hex(0xEEE4DA)
#define BUTTON_COLOR_4 lv_color_hex(0xEBD8B6)
#define BUTTON_COLOR_8 lv_color_hex(0xF1AE72)
#define BUTTON_COLOR_16 lv_color_hex(0xF6925F)
#define BUTTON_COLOR_32 lv_color_hex(0xF67D61)
#define BUTTON_COLOR_64 lv_color_hex(0xF75F3B)
#define BUTTON_COLOR_128 lv_color_hex(0xF2CF54)
#define BUTTON_COLOR_256 lv_color_hex(0xEDCC61)
#define BUTTON_COLOR_512 lv_color_hex(0xEDC850)
#define BUTTON_COLOR_1024 lv_color_hex(0xEDC53F)
#define BUTTON_COLOR_2048 lv_color_hex(0xEDC22E)

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void init_matrix(void);
static void init_map(void);
static void update_btnm_map(char *btnm_map[], uint32_t num_matrix[MATRIX_DIMEN][MATRIX_DIMEN]);
static void add_rand_num(uint32_t num_matrix[MATRIX_DIMEN][MATRIX_DIMEN]);
static bool is_game_over(void);
static bool is_win(void);
static bool move_left(void);
static bool move_right(void);
static bool move_up(void);
static bool move_down(void);
static lv_color_t get_btn_color(uint32_t num);
static void btnm_event_cb(lv_event_t *e);

/**********************
 *  STATIC VARIABLES
 **********************/

// 内部存储的矩阵数据
static uint32_t num_matrix[MATRIX_DIMEN][MATRIX_DIMEN];
// 由矩阵数据转换为的map数据
static char *btnm_map[MAP_LENGTH];
static uint32_t score = 0;
static uint32_t best_score = 0;
static uint32_t moves = 0;

/**********************
 *  FUNCTIONS
 **********************/

void custom_init(lv_ui *ui)
{
    init_matrix();
    init_map();
    // 由当前的数字矩阵更新按键组信息
    update_btnm_map(btnm_map, num_matrix);
    // 将按键组信息设置到按键组中
    lv_buttonmatrix_set_map(ui->Game2048_btnm_2048, (const char *const *)btnm_map);

    // 启用按键组的回调函数和事件信号
    lv_obj_add_flag(ui->Game2048_btnm_2048, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);                 // 当这个对象有绘图任务时，发送相应的事件
    lv_obj_add_event_cb(ui->Game2048_btnm_2048, btnm_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL); // 接收事件，表示"绘图任务已添加"，设置回调函数以实时更新按钮的外观状态
}



void new_game(lv_ui *ui)
{

}

void movement_check(lv_ui *ui, gg_2048_move_direction direction)
{

}

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void init_matrix(void)
{
    for (uint32_t i = 0; i < MATRIX_DIMEN; i++) {
        for (uint32_t j = 0; j < MATRIX_DIMEN; j++) {
            num_matrix[i][j] = 0;
        }
    }

    add_rand_num(num_matrix);
    add_rand_num(num_matrix);
}

static void init_map(void)
{
    uint32_t index;
    for (index = 0; index < MAP_LENGTH; index++) {

        if (((index + 1) % 5) == 0) {
            btnm_map[index] = lv_malloc(2);
            if ((index + 1) == MAP_LENGTH)
                lv_strcpy(btnm_map[index], "");
            else
            lv_strcpy(btnm_map[index], "\n");
        } else {
            btnm_map[index] = lv_malloc(5);
            lv_strcpy(btnm_map[index], " ");
        }
    }
}
