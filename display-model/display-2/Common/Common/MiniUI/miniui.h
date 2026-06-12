/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI main header for E-ink B/W display.
*                      Single include for all MiniUI modules.
********************************************************************************/
#ifndef __MINIUI_H
#define __MINIUI_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 *  Core Types & Callbacks (defined in miniui_types.h)
 *=============================================================================*/

#include "miniui_types.h"
#include "miniui_color.h"

/*=============================================================================
 *  Alignment
 *=============================================================================*/

#define UI_ALIGN_LEFT    0
#define UI_ALIGN_CENTER  1
#define UI_ALIGN_RIGHT   2

/*=============================================================================
 *  Module Headers
 *=============================================================================*/

#include "miniui_render.h"
#include "miniui_page.h"
#include "miniui_widget.h"
#include "miniui_input.h"

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_H */
