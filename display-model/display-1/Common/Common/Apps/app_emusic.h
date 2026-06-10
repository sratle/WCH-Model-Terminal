/********************************** (C) COPYRIGHT *******************************
* File Name          : app_emusic.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/06/09
* Description        : Electronic Music app — receives Keyboard-3 real-time
*                      music events (piano keys, buttons, faders) from Core
*                      and renders them with partial refresh.
********************************************************************************/
#ifndef __APP_EMUSIC_H
#define __APP_EMUSIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void app_emusic_init(void);
ui_page_t *app_emusic_get_page(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_EMUSIC_H */
