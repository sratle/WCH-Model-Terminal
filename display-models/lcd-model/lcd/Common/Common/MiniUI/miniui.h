/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI main header file.
*                      Include this file to use the entire MiniUI library.
********************************************************************************/
#ifndef __MINIUI_H
#define __MINIUI_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 *  Core Modules
 *=============================================================================*/

#include "miniui_types.h"
#include "miniui_color.h"
#include "miniui_render.h"
#include "miniui_widget.h"
#include "miniui_page.h"
#include "miniui_input.h"
#include "miniui_anim.h"

/*=============================================================================
 *  Fonts
 *=============================================================================*/

#include "font/font_montserrat_12.h"
#include "font/font_montserrat_16.h"

/*=============================================================================
 *  Main UI API
 *=============================================================================*/

/* Initialize the entire UI system */
void UI_Init(void);

/* Main UI tick - process input, animations, and rendering */
void UI_Tick(void);

/* Full screen refresh */
void UI_FullRefresh(void);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_H */
