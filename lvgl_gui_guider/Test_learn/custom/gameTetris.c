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
// 主界面的行数列数定义
#define MATRIX_COLUMN 8
#define MATRIX_ROW 16
#define MAP_LENGTH (MATRIX_COLUMN * MATRIX_ROW + MATRIX_ROW)

// 下一个物件显示区域的定义，该区域是4*4
#define NEXT_MATRIX_COLUMN 4
#define NEXT_MATRIX_ROW 4
#define NEXT_MAP_LENGTH (NEXT_MATRIX_COLUMN * NEXT_MATRIX_ROW + NEXT_MATRIX_ROW)

// 定义俄罗斯方块按键组在不同形状下的颜色
#define BUTTON_COLOR_EMPTY lv_color_hex(0x8B7355)
#define BUTTON_COLOR_I lv_color_hex(0x9ED9D8)   // 四个竖着的那个
#define BUTTON_COLOR_O lv_color_hex(0xFFE194)   // 四个组成正方形
#define BUTTON_COLOR_T lv_color_hex(0xD4A5D4)   // 四个组成T形
#define BUTTON_COLOR_S lv_color_hex(0xB4E7CE)   // 四个组成S形
#define BUTTON_COLOR_Z lv_color_hex(0xF67280)   // Z形
#define BUTTON_COLOR_L lv_color_hex(0xFFCBA4)   // 四个组成L形
#define BUTTON_COLOR_J lv_color_hex(0xC06C84)   // J形
#define BUTTON_COLOR_ROW lv_color_hex(0xFFF9E6) // 消除的时候闪烁一下再变回EMPTY

// 游戏配置
#define STAGE_SCORE_STEP 600  // 每个阶段需要的分数
#define MAX_STAGE 10           // 最大阶段数
#define BASE_FALL_SPEED 800   // 基础下落速度(ms)

typedef enum
{
    TETRIS_BLOCK_EMPTY,
    TETRIS_BLOCK_I,
    TETRIS_BLOCK_O,
    TETRIS_BLOCK_T,
    TETRIS_BLOCK_S,
    TETRIS_BLOCK_Z,
    TETRIS_BLOCK_L,
    TETRIS_BLOCK_J,
} tetris_block_type;

// 方块结构定义
typedef struct {
    uint8_t shape[4][4];  // 方块形状矩阵
    int x, y;             // 方块位置
    tetris_block_type type; // 方块类型
    uint8_t rotation;     // 旋转状态(0-3)
    bool active;          // 是否有活动方块
} tetris_brick_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void init_matrix(void);
static void init_map(void);
static void init_shapes(void);
static void update_btnm_map(char *btnm_map[], uint32_t display_matrix[MATRIX_ROW][MATRIX_COLUMN]);
static void update_next_btnm_map(char *btnm_map[], uint32_t next_matrix[NEXT_MATRIX_ROW][NEXT_MATRIX_COLUMN]);
static void btnm_event_cb(lv_event_t *e);
static void next_btnm_event_cb(lv_event_t *e);
static void spawn_new_brick(void);
static void generate_next_brick(void);
static bool check_collision(int offset_x, int offset_y, uint8_t rotation);
static void merge_brick_to_fixed(void);
static void check_and_clear_lines(void);
static void get_current_shape(uint8_t shape_out[4][4]);
static void timer_cb(lv_timer_t *timer);
static void update_display_matrix(void);
static bool is_game_over(void); 
static bool is_win(void);
static bool move_left(void);
static bool move_right(void);
static bool rotate_brick(void);
static bool move_down(void);
static lv_color_t get_brick_color(tetris_block_type type);
static void update_ui_labels(lv_ui *ui);

/**********************
 *  STATIC VARIABLES
 **********************/

// 双层存储机制：固定层和活动方块分离
static uint32_t fixed_matrix[MATRIX_ROW][MATRIX_COLUMN];    // 固定层：已落地的方块
static uint32_t display_matrix[MATRIX_ROW][MATRIX_COLUMN];  // 显示层：固定层+活动方块合并
static uint32_t next_display_matrix[NEXT_MATRIX_ROW][NEXT_MATRIX_COLUMN]; // 下一个方块显示

// 活动方块
static tetris_brick_t current_brick;
static tetris_brick_t next_brick;

// 7种俄罗斯方块形状定义（每种4个旋转状态）
static uint8_t brick_shapes[7][4][4][4];

// 由矩阵数据转换为的map数据
static char *btnm_map[MAP_LENGTH];
static char *next_btnm_map[NEXT_MAP_LENGTH];

// 游戏状态
static bool game_state;        // false=停止, true=运行
static uint32_t game_stage;    // 当前阶段(1-10)
static lv_timer_t *fall_timer; // 下落定时器
static lv_ui *game_ui;         // UI指针

static uint32_t score = 0;
static uint32_t best_score = 0;

/**********************
 *  FUNCTIONS
 **********************/

void init_gameTetris(lv_ui *ui)
{
    game_ui = ui;
    init_shapes();
    init_matrix();
    init_map();
    
    game_state = false;
    game_stage = 1;
    score = 0;
    current_brick.active = false;
    fall_timer = NULL;
    
    // 将按键组信息设置到按键组中
    lv_buttonmatrix_set_map(ui->gameTetris_btnm_tetris, (const char *const *)btnm_map);
    lv_buttonmatrix_set_map(ui->gameTetris_btnm_next, (const char *const *)next_btnm_map);

    // 启用按键组的回调函数和事件信号
    lv_obj_add_flag(ui->gameTetris_btnm_tetris, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(ui->gameTetris_btnm_tetris, btnm_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    
    lv_obj_add_flag(ui->gameTetris_btnm_next, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(ui->gameTetris_btnm_next, next_btnm_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    
    // 初始化UI标签
    lv_label_set_text_fmt(ui->gameTetris_label_score, "0");
    lv_label_set_text_fmt(ui->gameTetris_label_best_score, "0");
    lv_label_set_text_fmt(ui->gameTetris_label_stage, "1");
    lv_bar_set_value(ui->gameTetris_bar_stage, 0, LV_ANIM_OFF);
}

void new_gameTetris(lv_ui *ui)
{
    // 停止旧定时器
    if (fall_timer) {
        lv_timer_delete(fall_timer);
        fall_timer = NULL;
    }
    
    // 重置游戏状态
    init_matrix();
    score = 0;
    game_stage = 1;
    game_state = true;
    current_brick.active = false;
    
    // 生成下一个方块和当前方块
    generate_next_brick();
    spawn_new_brick();
    
    // 创建下落定时器
    uint32_t fall_speed = BASE_FALL_SPEED - (game_stage - 1) * 100;
    if (fall_speed < 150) fall_speed = 150;
    fall_timer = lv_timer_create(timer_cb, fall_speed, ui);
    
    // 更新显示
    update_display_matrix();
    update_btnm_map(btnm_map, display_matrix);
    lv_buttonmatrix_set_map(ui->gameTetris_btnm_tetris, (const char *const *)btnm_map);
    
    update_ui_labels(ui);
}

void check_gameTetris(lv_ui *ui, tetris_move_direction direction)
{
    if (!game_state || !current_brick.active) return;
    
    bool moved = false;
    switch (direction)
    {
        case TETRIS_MOVE_LEFT:
            moved = move_left();
            break;
        case TETRIS_MOVE_RIGHT:
            moved = move_right();
            break;
        case TETRIS_MOVE_UP:
            moved = rotate_brick();
            break;
        case TETRIS_MOVE_DOWN:
            moved = move_down();
            break;
        default:
            break;
    }
    
    if (moved) {
        update_display_matrix();
        update_btnm_map(btnm_map, display_matrix);
        lv_buttonmatrix_set_map(ui->gameTetris_btnm_tetris, (const char *const *)btnm_map);
    }
}

/**********************
 *  STATIC FUNCTIONS
 **********************/

static void init_matrix(void)
{
    for (uint32_t i = 0; i < MATRIX_ROW; i++)
    {
        for (uint32_t j = 0; j < MATRIX_COLUMN; j++)
        {
            fixed_matrix[i][j] = 0;
            display_matrix[i][j] = 0;
        }
    }
    for (uint32_t i = 0; i < NEXT_MATRIX_ROW; i++)
    {
        for (uint32_t j = 0; j < NEXT_MATRIX_COLUMN; j++)
        {
            next_display_matrix[i][j] = 0;
        }
    }
}

static void init_map(void)
{
    // 在LVGL中，按钮矩阵使用特殊的分隔符："\n" 表示换行（开始新的一行按钮）,"" 表示数组结束，需要一个字符串数组，每次都需要malloc一些空间
    for (int index = 0; index < MAP_LENGTH; index++)
    {

        if (((index + 1) % (MATRIX_COLUMN + 1)) == 0)
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
    for (int index = 0; index < NEXT_MAP_LENGTH; index++)
    {
        if (((index + 1) % (NEXT_MATRIX_COLUMN + 1)) == 0)
        {
            next_btnm_map[index] = lv_malloc(2);
            if ((index + 1) == NEXT_MAP_LENGTH)
                lv_strcpy(next_btnm_map[index], "");
            else
                lv_strcpy(next_btnm_map[index], "\n");
        }
        else
        {
            next_btnm_map[index] = lv_malloc(5);
            lv_strcpy(next_btnm_map[index], " ");
        }
    }
}

static void update_btnm_map(char *btnm_map[], uint32_t main_matrix[MATRIX_ROW][MATRIX_COLUMN])
{
    uint32_t btn_index = 0; // 按钮映射表索引

    for (uint32_t row = 0; row < MATRIX_ROW; row++)
    {
        for (uint32_t col = 0; col < MATRIX_COLUMN; col++)
        {
            // 跳过每行的换行符
            if (btn_index % (MATRIX_COLUMN + 1) == MATRIX_COLUMN)
            {
                btn_index++; // 跳过换行符
            }

            // 不用更新数字，俄罗斯方块只需要更新颜色
            lv_strcpy(btnm_map[btn_index], " ");

            btn_index++;
        }
    }
}

static void update_next_btnm_map(char *btnm_map[], uint32_t next_matrix[NEXT_MATRIX_ROW][NEXT_MATRIX_COLUMN])
{
    uint32_t btn_index = 0; // 按钮映射表索引

    for (uint32_t row = 0; row < NEXT_MATRIX_ROW; row++)
    {
        for (uint32_t col = 0; col < NEXT_MATRIX_COLUMN; col++)
        {
            // 跳过每行的换行符
            if (btn_index % (NEXT_MATRIX_COLUMN + 1) == NEXT_MATRIX_COLUMN)
            {
                btn_index++; // 跳过换行符
            }

            // 不用更新数字，俄罗斯方块只需要更新颜色
            lv_strcpy(btnm_map[btn_index], " ");

            btn_index++;
        }
    }
}

static void btnm_event_cb(lv_event_t *e)
{
    lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc->part == LV_PART_ITEMS) {
        if (base_dsc->id1 >= 0) {
            uint32_t x = (uint32_t)(base_dsc->id1) / MATRIX_COLUMN;
            uint32_t y = (base_dsc->id1) % MATRIX_COLUMN;
            tetris_block_type block_type = (tetris_block_type)display_matrix[x][y];
            
            lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
            if (fill_draw_dsc) {
                fill_draw_dsc->radius = 3;
                fill_draw_dsc->color = get_brick_color(block_type);
            }
        }
    }
}

static void next_btnm_event_cb(lv_event_t *e)
{
    lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc->part == LV_PART_ITEMS) {
        if (base_dsc->id1 >= 0) {
            uint32_t x = (uint32_t)(base_dsc->id1) / NEXT_MATRIX_COLUMN;
            uint32_t y = (base_dsc->id1) % NEXT_MATRIX_COLUMN;
            tetris_block_type block_type = (tetris_block_type)next_display_matrix[x][y];
            
            lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
            if (fill_draw_dsc) {
                fill_draw_dsc->radius = 3;
                fill_draw_dsc->color = get_brick_color(block_type);
            }
        }
    }
}

// 初始化方块形状数据
static void init_shapes(void)
{
    // 清空
    memset(brick_shapes, 0, sizeof(brick_shapes));
    
    // I形 - 竖条
    brick_shapes[0][0][1][0] = 1; brick_shapes[0][0][1][1] = 1;
    brick_shapes[0][0][1][2] = 1; brick_shapes[0][0][1][3] = 1;
    brick_shapes[0][1][0][1] = 1; brick_shapes[0][1][1][1] = 1;
    brick_shapes[0][1][2][1] = 1; brick_shapes[0][1][3][1] = 1;
    brick_shapes[0][2][2][0] = 1; brick_shapes[0][2][2][1] = 1;
    brick_shapes[0][2][2][2] = 1; brick_shapes[0][2][2][3] = 1;
    brick_shapes[0][3][0][2] = 1; brick_shapes[0][3][1][2] = 1;
    brick_shapes[0][3][2][2] = 1; brick_shapes[0][3][3][2] = 1;
    
    // O形 - 正方形
    for (int r = 0; r < 4; r++) {
        brick_shapes[1][r][1][1] = 1; brick_shapes[1][r][1][2] = 1;
        brick_shapes[1][r][2][1] = 1; brick_shapes[1][r][2][2] = 1;
    }
    
    // T形
    brick_shapes[2][0][1][1] = 1; brick_shapes[2][0][2][0] = 1;
    brick_shapes[2][0][2][1] = 1; brick_shapes[2][0][2][2] = 1;
    brick_shapes[2][1][1][1] = 1; brick_shapes[2][1][2][1] = 1;
    brick_shapes[2][1][2][2] = 1; brick_shapes[2][1][3][1] = 1;
    brick_shapes[2][2][1][0] = 1; brick_shapes[2][2][1][1] = 1;
    brick_shapes[2][2][1][2] = 1; brick_shapes[2][2][2][1] = 1;
    brick_shapes[2][3][1][1] = 1; brick_shapes[2][3][2][0] = 1;
    brick_shapes[2][3][2][1] = 1; brick_shapes[2][3][3][1] = 1;
    
    // S形
    brick_shapes[3][0][1][1] = 1; brick_shapes[3][0][1][2] = 1;
    brick_shapes[3][0][2][0] = 1; brick_shapes[3][0][2][1] = 1;
    brick_shapes[3][1][1][1] = 1; brick_shapes[3][1][2][1] = 1;
    brick_shapes[3][1][2][2] = 1; brick_shapes[3][1][3][2] = 1;
    brick_shapes[3][2][2][1] = 1; brick_shapes[3][2][2][2] = 1;
    brick_shapes[3][2][3][0] = 1; brick_shapes[3][2][3][1] = 1;
    brick_shapes[3][3][0][0] = 1; brick_shapes[3][3][1][0] = 1;
    brick_shapes[3][3][1][1] = 1; brick_shapes[3][3][2][1] = 1;
    
    // Z形
    brick_shapes[4][0][1][0] = 1; brick_shapes[4][0][1][1] = 1;
    brick_shapes[4][0][2][1] = 1; brick_shapes[4][0][2][2] = 1;
    brick_shapes[4][1][1][2] = 1; brick_shapes[4][1][2][1] = 1;
    brick_shapes[4][1][2][2] = 1; brick_shapes[4][1][3][1] = 1;
    brick_shapes[4][2][2][0] = 1; brick_shapes[4][2][2][1] = 1;
    brick_shapes[4][2][3][1] = 1; brick_shapes[4][2][3][2] = 1;
    brick_shapes[4][3][0][1] = 1; brick_shapes[4][3][1][0] = 1;
    brick_shapes[4][3][1][1] = 1; brick_shapes[4][3][2][0] = 1;
    
    // L形
    brick_shapes[5][0][1][1] = 1; brick_shapes[5][0][2][1] = 1;
    brick_shapes[5][0][3][1] = 1; brick_shapes[5][0][3][2] = 1;
    brick_shapes[5][1][1][1] = 1; brick_shapes[5][1][1][2] = 1;
    brick_shapes[5][1][1][3] = 1; brick_shapes[5][1][2][1] = 1;
    brick_shapes[5][2][1][0] = 1; brick_shapes[5][2][1][1] = 1;
    brick_shapes[5][2][2][1] = 1; brick_shapes[5][2][3][1] = 1;
    brick_shapes[5][3][2][1] = 1; brick_shapes[5][3][2][2] = 1;
    brick_shapes[5][3][2][3] = 1; brick_shapes[5][3][3][3] = 1;
    
    // J形
    brick_shapes[6][0][1][1] = 1; brick_shapes[6][0][2][1] = 1;
    brick_shapes[6][0][3][0] = 1; brick_shapes[6][0][3][1] = 1;
    brick_shapes[6][1][1][1] = 1; brick_shapes[6][1][2][1] = 1;
    brick_shapes[6][1][2][2] = 1; brick_shapes[6][1][2][3] = 1;
    brick_shapes[6][2][1][1] = 1; brick_shapes[6][2][1][2] = 1;
    brick_shapes[6][2][2][1] = 1; brick_shapes[6][2][3][1] = 1;
    brick_shapes[6][3][1][1] = 1; brick_shapes[6][3][1][2] = 1;
    brick_shapes[6][3][1][3] = 1; brick_shapes[6][3][2][3] = 1;
}

// 生成下一个方块
static void generate_next_brick(void)
{
    static bool initialized = false;
    if (!initialized) {
        lv_rand_set_seed(lv_tick_get());
        initialized = true;
    }
    
    next_brick.type = (tetris_block_type)(lv_rand(1, 7));
    next_brick.rotation = 0;
    next_brick.x = 0;
    next_brick.y = 0;
    next_brick.active = true;
    
    // 复制形状
    memcpy(next_brick.shape, brick_shapes[next_brick.type - 1][0], sizeof(next_brick.shape));
    
    // 更新下一个方块显示区域
    for (int i = 0; i < NEXT_MATRIX_ROW; i++) {
        for (int j = 0; j < NEXT_MATRIX_COLUMN; j++) {
            if (next_brick.shape[i][j]) {
                next_display_matrix[i][j] = next_brick.type;
            } else {
                next_display_matrix[i][j] = TETRIS_BLOCK_EMPTY;
            }
        }
    }
    
    update_next_btnm_map(next_btnm_map, next_display_matrix);
    if (game_ui) {
        lv_buttonmatrix_set_map(game_ui->gameTetris_btnm_next, (const char *const *)next_btnm_map);
    }
}

// 生成新的活动方块
static void spawn_new_brick(void)
{
    // 将next_brick移动到当前方块
    current_brick = next_brick;
    current_brick.x = MATRIX_COLUMN / 2 - 2;
    current_brick.y = 0;
    current_brick.active = true;
    
    // 生成下一个
    generate_next_brick();
    
    // 检查是否游戏结束
    if (check_collision(0, 0, current_brick.rotation)) {
        game_state = false;
        current_brick.active = false;
        if (fall_timer) {
            lv_timer_delete(fall_timer);
            fall_timer = NULL;
        }
        // 显示游戏结束消息
        if (game_ui) {
            lv_obj_remove_flag(game_ui->gameTetris_cont_msgbox, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(game_ui->gameTetris_label_result_title, "Game Over");
            lv_label_set_text_fmt(game_ui->gameTetris_label_result_score, 
                "Score: %" LV_PRIu32 "\nBest: %" LV_PRIu32, score, best_score);
        }
    }
}

// 获取当前方块形状
static void get_current_shape(uint8_t shape_out[4][4])
{
    memcpy(shape_out, brick_shapes[current_brick.type - 1][current_brick.rotation], 16);
}

// 碰撞检测
static bool check_collision(int offset_x, int offset_y, uint8_t rotation)
{
    uint8_t shape[4][4];
    memcpy(shape, brick_shapes[current_brick.type - 1][rotation], sizeof(shape));
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (shape[i][j]) {
                int new_x = current_brick.x + j + offset_x;
                int new_y = current_brick.y + i + offset_y;
                
                // 边界检测
                if (new_x < 0 || new_x >= MATRIX_COLUMN || new_y >= MATRIX_ROW) {
                    return true;
                }
                
                // 与固定层碰撞检测
                if (new_y >= 0 && fixed_matrix[new_y][new_x] != 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

// 将活动方块合并到固定层
static void merge_brick_to_fixed(void)
{
    uint8_t shape[4][4];
    get_current_shape(shape);
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (shape[i][j]) {
                int x = current_brick.x + j;
                int y = current_brick.y + i;
                if (y >= 0 && y < MATRIX_ROW && x >= 0 && x < MATRIX_COLUMN) {
                    fixed_matrix[y][x] = current_brick.type;
                }
            }
        }
    }
    current_brick.active = false;
}

// 检查并消除满行
static void check_and_clear_lines(void)
{
    int lines_cleared = 0;
    
    for (int row = MATRIX_ROW - 1; row >= 0; row--) {
        bool is_full = true;
        for (int col = 0; col < MATRIX_COLUMN; col++) {
            if (fixed_matrix[row][col] == 0) {
                is_full = false;
                break;
            }
        }
        
        if (is_full) {
            lines_cleared++;
            // 上方所有行下移
            for (int r = row; r > 0; r--) {
                memcpy(fixed_matrix[r], fixed_matrix[r-1], sizeof(fixed_matrix[0]));
            }
            memset(fixed_matrix[0], 0, sizeof(fixed_matrix[0]));
            row++; // 重新检查当前行
        }
    }
    
    if (lines_cleared > 0) {
        // 计算分数
        uint32_t line_score = 100 * lines_cleared * lines_cleared;
        score += line_score;
        best_score = (score > best_score) ? score : best_score;
        
        // 检查阶段升级
        uint32_t new_stage = score / STAGE_SCORE_STEP + 1;
        if (new_stage > game_stage && new_stage <= MAX_STAGE) {
            game_stage = new_stage;
            // 调整下落速度
            if (fall_timer) {
                uint32_t fall_speed = BASE_FALL_SPEED - (game_stage - 1) * 100;
                if (fall_speed < 150) fall_speed = 150;
                lv_timer_set_period(fall_timer, fall_speed);
            }
        }
        
        // 检查胜利
        if (game_stage >= MAX_STAGE && score >= MAX_STAGE * STAGE_SCORE_STEP) {
            game_state = false;
            if (fall_timer) {
                lv_timer_delete(fall_timer);
                fall_timer = NULL;
            }
            if (game_ui) {
                lv_obj_remove_flag(game_ui->gameTetris_cont_msgbox, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(game_ui->gameTetris_label_result_title, "You Win!");
                lv_label_set_text_fmt(game_ui->gameTetris_label_result_score, 
                    "Score: %" LV_PRIu32 "\nBest: %" LV_PRIu32, score, best_score);
            }
        }
        
        update_ui_labels(game_ui);
    }
}

// 更新显示矩阵
static void update_display_matrix(void)
{
    // 复制固定层到显示层
    memcpy(display_matrix, fixed_matrix, sizeof(display_matrix));
    
    // 叠加活动方块
    if (current_brick.active) {
        uint8_t shape[4][4];
        get_current_shape(shape);
        
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (shape[i][j]) {
                    int x = current_brick.x + j;
                    int y = current_brick.y + i;
                    if (y >= 0 && y < MATRIX_ROW && x >= 0 && x < MATRIX_COLUMN) {
                        display_matrix[y][x] = current_brick.type;
                    }
                }
            }
        }
    }
}

// 定时器回调 - 自动下落
static void timer_cb(lv_timer_t *timer)
{
    if (!game_state || !current_brick.active) return;
    
    // 尝试下移
    if (!check_collision(0, 1, current_brick.rotation)) {
        current_brick.y++;
    } else {
        // 触底：固定方块
        merge_brick_to_fixed();
        check_and_clear_lines();
        spawn_new_brick();
    }
    
    // 更新显示
    update_display_matrix();
    update_btnm_map(btnm_map, display_matrix);
    if (game_ui) {
        lv_buttonmatrix_set_map(game_ui->gameTetris_btnm_tetris, (const char *const *)btnm_map);
    }
}

// 移动操作
static bool move_left(void)
{
    if (!current_brick.active) return false;
    
    if (!check_collision(-1, 0, current_brick.rotation)) {
        current_brick.x--;
        return true;
    }
    return false;
}

static bool move_right(void)
{
    if (!current_brick.active) return false;
    
    if (!check_collision(1, 0, current_brick.rotation)) {
        current_brick.x++;
        return true;
    }
    return false;
}

static bool rotate_brick(void)
{
    if (!current_brick.active) return false;
    
    uint8_t new_rotation = (current_brick.rotation + 1) % 4;
    if (!check_collision(0, 0, new_rotation)) {
        current_brick.rotation = new_rotation;
        memcpy(current_brick.shape, brick_shapes[current_brick.type - 1][new_rotation], 
               sizeof(current_brick.shape));
        return true;
    }
    return false;
}

static bool move_down(void)
{
    if (!current_brick.active) return false;
    
    if (!check_collision(0, 1, current_brick.rotation)) {
        current_brick.y++;
        return true;
    }
    return false;
}

// 获取方块颜色
static lv_color_t get_brick_color(tetris_block_type type)
{
    switch (type) {
        case TETRIS_BLOCK_I: return BUTTON_COLOR_I;
        case TETRIS_BLOCK_O: return BUTTON_COLOR_O;
        case TETRIS_BLOCK_T: return BUTTON_COLOR_T;
        case TETRIS_BLOCK_S: return BUTTON_COLOR_S;
        case TETRIS_BLOCK_Z: return BUTTON_COLOR_Z;
        case TETRIS_BLOCK_L: return BUTTON_COLOR_L;
        case TETRIS_BLOCK_J: return BUTTON_COLOR_J;
        default: return BUTTON_COLOR_EMPTY;
    }
}

// 更新UI标签
static void update_ui_labels(lv_ui *ui)
{
    if (!ui) return;
    
    lv_label_set_text_fmt(ui->gameTetris_label_score, "%" LV_PRIu32, score);
    lv_label_set_text_fmt(ui->gameTetris_label_best_score, "%" LV_PRIu32, best_score);
    lv_label_set_text_fmt(ui->gameTetris_label_stage, "%" LV_PRIu32, game_stage);
    
    // 更新阶段进度条
    uint32_t stage_progress = (score % STAGE_SCORE_STEP) * 100 / STAGE_SCORE_STEP;
    lv_bar_set_value(ui->gameTetris_bar_stage, stage_progress, LV_ANIM_ON);
}

static bool is_game_over(void)
{
    return !game_state;
}

static bool is_win(void)
{
    return (game_stage >= MAX_STAGE && score >= MAX_STAGE * STAGE_SCORE_STEP);
}
