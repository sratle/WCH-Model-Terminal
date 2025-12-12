#ifndef __GAMETETRIS_H_
#define __GAMETETRIS_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include "gui_guider.h"

void init_gameTetris(lv_ui *ui);

void new_gameTetris(lv_ui *ui);

void check_gameTetris(lv_ui *ui, tetris_move_direction direction);

#ifdef __cplusplus
}
#endif
#endif