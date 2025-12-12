#ifndef __GAME2048_H_
#define __GAME2048_H_
#ifdef __cplusplus
extern "C"
{
#endif

#include "gui_guider.h"

void init_game2048(lv_ui *ui);

void new_game2048(lv_ui *ui);

void check_game2048(lv_ui *ui, gg_2048_move_direction direction);

#ifdef __cplusplus
}
#endif
#endif