#ifndef __GAMES_H
#define __GAMES_H

#include "../MiniUI/miniui.h"

void games_init_all(void);

ui_page_t *game_tetris_get_page(void);
ui_page_t *game_2048_get_page(void);
ui_page_t *game_snake_get_page(void);
ui_page_t *game_airplane_get_page(void);
ui_page_t *game_minesweeper_get_page(void);

/* ---- RGB 灯效联动（经 Core 转发给 RGB Submodel，发送即忘） ----
 * speed 为档位 1~10 */
void games_rgb_wave(uint8_t direction, uint8_t speed);   /* direction: RGB_WAVE_DIR_* */
void games_rgb_ripple(uint8_t speed);

/* 逻辑方向（各游戏共用），内部映射为 RGB 波浪方向 */
typedef enum {
    GAMES_DIR_LEFT = 0,
    GAMES_DIR_RIGHT,
    GAMES_DIR_UP,
    GAMES_DIR_DOWN,
} games_dir_t;

void games_rgb_wave_dir(games_dir_t dir, uint8_t speed);

/* 各游戏约定的联动参数 */
#define GAMES_RGB_WAVE_SPEED        8   /* tetris/2048/snake 方向输入 */
#define GAMES_RGB_RIPPLE_SPEED_HIT  10  /* airplane 击中敌机 */
#define GAMES_RGB_RIPPLE_SPEED_DIG  6   /* minesweeper 挖雷 */

/* ---- 音效联动（经 CLI 直通在 Core 播放，ch0=BGM / ch1=SFX） ----
 * 均受 config.json 0101 的 operationsound / gamebgm 开关门控 */
void games_sfx_dir(void);    /* 方向性动作音 /SOUND/SOUND-GEACTION.wav */
void games_sfx_hit(void);    /* 击中/吃食/挖雷 /SOUND/SOUND-HIT.wav */
void games_bgm_poll(void);   /* BGM 循环守护，游戏页 update 中调用 */
void games_bgm_start(void);  /* 游戏页 on_enter 调用（幂等，会话式） */
void games_bgm_leave(void);  /* 游戏页 on_exit 调用（延迟停止） */

#endif
