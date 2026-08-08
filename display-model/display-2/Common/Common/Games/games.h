/********************************** (C) COPYRIGHT *******************************
* File Name          : games.h
* Author             : E-ink Model Team
* Version            : V2.0.0
* Date               : 2026/07/19
* Description        : Game module header.
*                      Trimmed for E-ink resource budget: only 2048 and
*                      Minesweeper are kept (turn-based, partial-refresh
*                      friendly).  Action games were removed to free FLASH.
********************************************************************************/
#ifndef __GAMES_H
#define __GAMES_H

#include "../MiniUI/miniui.h"

void games_init_all(void);

ui_page_t *game_2048_get_page(void);
ui_page_t *game_minesweeper_get_page(void);

/* ---- 音效联动（经 CLI 直通在 Core 播放，ch0=BGM / ch1=SFX） ----
 * 均受 config.json 0102 的 operationsound / gamebgm 开关门控 */
void games_sfx_dir(void);    /* 方向性动作音 /SOUND/SOUND-GEACTION.wav */
void games_sfx_hit(void);    /* 挖雷等命中反馈 /SOUND/SOUND-HIT.wav */
void games_bgm_start(void);  /* 游戏页 on_enter 调用（幂等，会话式） */
void games_bgm_leave(void);  /* 游戏页 on_exit 调用（延迟停止） */

#endif
