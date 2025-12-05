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
    
}

void init_game2048(lv_ui *ui)
{
    init_matrix();
    init_map();
    // 由当前的数字矩阵更新按键组信息
    update_btnm_map(btnm_map, num_matrix);
    // 将按键组信息设置到按键组中
    lv_buttonmatrix_set_map(ui->game2048_btnm_2048, (const char *const *)btnm_map);

    // 启用按键组的回调函数和事件信号
    lv_obj_add_flag(ui->game2048_btnm_2048, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);                 // 当这个对象有绘图任务时，发送相应的事件
    lv_obj_add_event_cb(ui->game2048_btnm_2048, btnm_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL); // 接收事件，表示"绘图任务已添加"，设置回调函数以实时更新按钮的外观状态
}

void new_game2048(lv_ui *ui)
{
    init_matrix();

    score = 0;
    moves = 0;

    lv_label_set_text_fmt(ui->game2048_label_score, "%" LV_PRIu32, score);
    update_btnm_map(btnm_map, num_matrix);
    lv_buttonmatrix_set_map(ui->game2048_btnm_2048, (const char * const *)btnm_map);
}

void movement_check(lv_ui *ui, gg_2048_move_direction direction)
{
    bool is_moved = false;
    switch (direction)
    {
        case GG_2048_MOVE_LEFT:
            is_moved = move_left();
            break;
        case GG_2048_MOVE_RIGHT:
            is_moved = move_right();
            break;
        case GG_2048_MOVE_UP:
            is_moved = move_up();
            break;
        case GG_2048_MOVE_DOWN:
            is_moved = move_down();
            break;
        default:
            break;
    }
    if (is_moved) {
        moves += 1;
        best_score = (score > best_score) ? score : best_score;
        lv_label_set_text_fmt(ui->game2048_label_score,"%" LV_PRIu32, score);
        lv_label_set_text_fmt(ui->game2048_label_best, "%" LV_PRIu32, best_score);

        add_rand_num(num_matrix);
        update_btnm_map(btnm_map, num_matrix);
        lv_buttonmatrix_set_map(ui->game2048_btnm_2048, (const char * const *)btnm_map);

        if (is_win()) {
            lv_obj_remove_flag(ui->game2048_cont_msgbox, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui->game2048_label_warning, "You Win!");
            lv_label_set_text_fmt(ui->game2048_label_results, "%" LV_PRIu32"\n%" LV_PRIu32"\n%" LV_PRIu32, moves, score, best_score);

        } else if (is_game_over()) {
            lv_obj_remove_flag(ui->game2048_cont_msgbox, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ui->game2048_label_warning, "Game Over");
            lv_label_set_text_fmt(ui->game2048_label_results, "%" LV_PRIu32"\n%" LV_PRIu32"\n%" LV_PRIu32, moves, score, best_score);
        }
    }
}

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void init_matrix(void)
{
    for (uint32_t i = 0; i < MATRIX_DIMEN; i++)
    {
        for (uint32_t j = 0; j < MATRIX_DIMEN; j++)
        {
            num_matrix[i][j] = 0;
        }
    }

    add_rand_num(num_matrix);
    add_rand_num(num_matrix);
}

static void init_map(void)
{
    uint32_t index;
    // 在LVGL中，按钮矩阵使用特殊的分隔符："\n" 表示换行（开始新的一行按钮）,"" 表示数组结束，需要一个字符串数组，每次都需要malloc一些空间
    for (index = 0; index < MAP_LENGTH; index++)
    {

        if (((index + 1) % 5) == 0)
        {
            btnm_map[index] = lv_malloc(2);
            if ((index + 1) == MAP_LENGTH)
                lv_strcpy(btnm_map[index], "");
            else
                lv_strcpy(btnm_map[index], "\n");
        }
        else
        {
            btnm_map[index] = lv_malloc(5);
            lv_strcpy(btnm_map[index], " ");
        }
    }
}

static void update_btnm_map(char *btnm_map[], uint32_t num_matrix[MATRIX_DIMEN][MATRIX_DIMEN])
{
    uint32_t btn_index = 0; // 按钮映射表索引

    for (uint32_t row = 0; row < MATRIX_DIMEN; row++)
    {
        for (uint32_t col = 0; col < MATRIX_DIMEN; col++)
        {
            // 跳过每行的换行符（每5个元素中的第5个）
            if (btn_index % 5 == 4)
            {
                btn_index++; // 跳过换行符
            }

            if (num_matrix[row][col] != 0)
            {
                sprintf(btnm_map[btn_index], "%lu", (unsigned long)num_matrix[row][col]);
            }
            else
            {
                lv_strcpy(btnm_map[btn_index], " ");
            }
            btn_index++;
        }
    }
}

// 2048每次游戏移动完之后，要添加一个随机的数字进去，可能是2或者4
static void add_rand_num(uint32_t num_matrix[MATRIX_DIMEN][MATRIX_DIMEN])
{
    static bool initialized = false;
    uint32_t x, y;
    uint32_t rnum, len = 0;
    uint32_t n, empty[MATRIX_DIMEN * MATRIX_DIMEN][2]; // empty用于统计空的按钮的位置

    if (!initialized)
    {
        lv_rand_set_seed(rand());
        initialized = true;
    }

    for (x = 0; x < MATRIX_DIMEN; x++)
    {
        for (y = 0; y < MATRIX_DIMEN; y++)
        {
            if (num_matrix[x][y] == 0)
            {
                empty[len][0] = x;
                empty[len][1] = y;
                len++;
            }
        }
    }

    if (len > 0)
    {
        rnum = lv_rand(0, len - 1); // 在所有空的按钮中随机选取一个
        x = empty[rnum][0];         // 获取随机到的按钮的x
        y = empty[rnum][1];
        n = ((lv_rand(0, 2) / 2) == 0) ? 2 : 4;
        num_matrix[x][y] = n;
    }
}

static bool is_game_over(void)
{
    for (int i = 0; i < MATRIX_DIMEN; i++)
    {
        for (int j = 0; j < MATRIX_DIMEN; j++)
        {
            if (num_matrix[i][j] == 0)
            {
                // 还有空位，所以没有结束
                return false;
            }
        }
    }

    for (int i = 0; i < MATRIX_DIMEN; i++)
    {
        for (int j = 0; j < MATRIX_DIMEN; j++)
        {
            // 有可以合并的，所以没有结束
            if ((j < MATRIX_DIMEN - 1 && num_matrix[i][j] == num_matrix[i][j + 1]) ||
                (i < MATRIX_DIMEN - 1 && num_matrix[i][j] == num_matrix[i + 1][j]))
            {
                return false;
            }
        }
    }
    
    return true;
}

static bool is_win(void)
{
    // 游戏获胜条件是获得一个2048
    for (int i = 0; i < MATRIX_DIMEN; i++) {
        for (int j = 0; j < MATRIX_DIMEN; j++) {
            if (num_matrix[i][j] == 2048) {
                return true;
            }
        }
    }
    return false;
}

static bool move_left(void)
{
    bool moved = false;
    for (int i = 0; i < MATRIX_DIMEN; i++) {
        // 合并符合条件的按钮
        for (int j = 0; j < MATRIX_DIMEN-1; j++) {
            if (num_matrix[i][j] != 0) {
                for (int k = j+1; k < MATRIX_DIMEN; k++) {
                    if (num_matrix[i][k] != 0) {
                        if (num_matrix[i][j] == num_matrix[i][k]) {
                            num_matrix[i][j] *= 2;
                            score += num_matrix[i][j];
                            num_matrix[i][k] = 0;
                            moved = true;
                        }
                        break;
                    }
                }
            }
        }
        // 将所有数字向左边移动
        for (int j = 0; j < MATRIX_DIMEN-1; j++) {
            if (num_matrix[i][j] == 0) {
                for (int k = j+1; k < MATRIX_DIMEN; k++) {
                    
                    if (num_matrix[i][k] != 0) {
                        num_matrix[i][j] = num_matrix[i][k];
                        num_matrix[i][k] = 0;
                        moved = true;
                        break;
                    }
                }
            }
        }
    }
    return moved;
}

static bool move_right(void)
{
    bool moved = false;
    for (int i = 0; i < MATRIX_DIMEN; i++) {
        for (int j = MATRIX_DIMEN-1; j > 0; j--) {
            if (num_matrix[i][j] != 0) {
                for (int k = j-1; k >= 0; k--) {
                    if (num_matrix[i][k] != 0) {
                        if (num_matrix[i][j] == num_matrix[i][k]) {
                            num_matrix[i][j] *= 2;
                            score += num_matrix[i][j];
                            num_matrix[i][k] = 0;
                            moved = true;
                        }
                        break;
                    }
                }
            }
        }

        for (int j = MATRIX_DIMEN-1; j > 0; j--) {
            if (num_matrix[i][j] == 0) {
                for (int k = j-1; k >= 0; k--) {
                    if (num_matrix[i][k] != 0) {
                        num_matrix[i][j] = num_matrix[i][k];
                        num_matrix[i][k] = 0;
                        moved = true;
                        break;
                    }
                }
            }
        }
    }
    return moved;
}

static bool move_up(void)
{
    bool moved = false;
    for (int j = 0; j < MATRIX_DIMEN; j++) {
        for (int i = 0; i < MATRIX_DIMEN-1; i++) {
            if (num_matrix[i][j] != 0) {
                for (int k = i+1; k < MATRIX_DIMEN; k++) {
                    if (num_matrix[k][j] != 0) {
                        if (num_matrix[i][j] == num_matrix[k][j]) {
                            num_matrix[i][j] *= 2;
                            score += num_matrix[i][j];
                            num_matrix[k][j] = 0;
                            moved = true;
                        }
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MATRIX_DIMEN-1; i++) {
            if (num_matrix[i][j] == 0) {
                for (int k = i+1; k < MATRIX_DIMEN; k++) {
                    if (num_matrix[k][j] != 0) {
                        num_matrix[i][j] = num_matrix[k][j];
                        num_matrix[k][j] = 0;
                        moved = true;
                        break;
                    }
                }
            }
        }
    }
    return moved;
}

static bool move_down(void)
{
    bool moved = false;
    for (int j = 0; j < MATRIX_DIMEN; j++) {
        for (int i = MATRIX_DIMEN-1; i > 0; i--) {
            if (num_matrix[i][j] != 0) {
                for (int k = i-1; k >= 0; k--) {
                    if (num_matrix[k][j] != 0) {
                        if (num_matrix[i][j] == num_matrix[k][j]) {
                            num_matrix[i][j] *= 2;
                            score += num_matrix[i][j];
                            num_matrix[k][j] = 0;
                            moved = true;
                        }
                        break;
                    }
                }
            }
        }

        for (int i = MATRIX_DIMEN-1; i > 0; i--) {
            if (num_matrix[i][j] == 0) {
                for (int k = i-1; k >= 0; k--) {
                    if (num_matrix[k][j] != 0) {
                        num_matrix[i][j] = num_matrix[k][j];
                        num_matrix[k][j] = 0;
                        moved = true;
                        break;
                    }
                }
            }
        }
    }
    return moved;
}

static lv_color_t get_btn_color(uint32_t num)
{
    lv_color_t color;

    switch (num)
    {
    case 0:
        color = BUTTON_COLOR_EMPTY;
        break;
    case 2:
        color = BUTTON_COLOR_2;
        break;
    case 4:
        color = BUTTON_COLOR_4;
        break;
    case 8:
        color = BUTTON_COLOR_8;
        break;
    case 16:
        color = BUTTON_COLOR_16;
        break;   
    case 32:
        color = BUTTON_COLOR_32;
        break;
    case 64:
        color = BUTTON_COLOR_64;
        break;
    case 128:
        color = BUTTON_COLOR_128;
        break;
    case 256:
        color = BUTTON_COLOR_256;
        break;
    case 512:
        color = BUTTON_COLOR_512;
        break;
    case 1024:
        color = BUTTON_COLOR_1024;
        break;
    case 2048:
        color = BUTTON_COLOR_2048;
        break;
    
    default:
        break;
    }
    return color;
}

static void btnm_event_cb(lv_event_t * e)
{
    lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc->part == LV_PART_ITEMS) {
        if (base_dsc->id1 >= 0) {
            static uint32_t x, y, num;
            x = (uint32_t)(base_dsc->id1) / 4;
            y = (base_dsc->id1) % 4;
            num = num_matrix[x][y];
            lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
            if (fill_draw_dsc) {
                fill_draw_dsc->radius = 10;
                fill_draw_dsc->color = get_btn_color(num);
            }
            lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
            if (label_draw_dsc) {
                if (num >= 8) label_draw_dsc->color = lv_color_white();
                else label_draw_dsc->color = lv_color_hex(0x756452);
            }
        }
    }
}
