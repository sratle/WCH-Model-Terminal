/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_splash.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/05/25
* Description        : System splash screen header.
*                      Fullscreen startup page with logo and init text.
********************************************************************************/
#ifndef __UI_SPLASH_H
#define __UI_SPLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_splash_init(void);

extern ui_page_t page_splash;

#ifdef __cplusplus
}
#endif

#endif /* __UI_SPLASH_H */
