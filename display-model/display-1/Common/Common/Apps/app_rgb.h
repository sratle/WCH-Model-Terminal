/********************************** (C) COPYRIGHT *******************************
* File Name          : app_rgb.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : RGB lighting control app — mode/color/brightness/speed
*                      via CLI passthrough. Color preview, custom animation load.
********************************************************************************/
#ifndef __APP_RGB_H
#define __APP_RGB_H

#include "../MiniUI/miniui.h"

void app_rgb_init(void);
ui_page_t *app_rgb_get_page(void);

#endif
