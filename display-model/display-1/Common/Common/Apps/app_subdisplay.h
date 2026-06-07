/********************************** (C) COPYRIGHT *******************************
* File Name          : app_subdisplay.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : Sub-display control app — mode switching, status refresh,
*                      BMP file browsing and transfer to SubDisplay via CLI.
********************************************************************************/
#ifndef __APP_SUBDISPLAY_H
#define __APP_SUBDISPLAY_H

#include "../MiniUI/miniui.h"

void app_subdisplay_init(void);
ui_page_t *app_subdisplay_get_page(void);

#endif
