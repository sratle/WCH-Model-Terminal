/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_color.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI color definitions for E-ink B/W display.
*                      1bpp color: 0 = white, 1 = black.
********************************************************************************/
#ifndef __MINIUI_COLOR_H
#define __MINIUI_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"

/*=============================================================================
 *  1bpp Color Constants for E-ink
 *=============================================================================*/

#define UI_COLOR_WHITE        0   /* bit=0 → white pixel */
#define UI_COLOR_BLACK        1   /* bit=1 → black pixel */
#define UI_COLOR_TRANSPARENT  2   /* Special: don't draw (skip pixel) */

/*=============================================================================
 *  Semantic Color Aliases (mapped to B/W)
 *=============================================================================*/

/* Primary theme */
#define UI_COLOR_PRIMARY       UI_COLOR_BLACK
#define UI_COLOR_SECONDARY     UI_COLOR_WHITE
#define UI_COLOR_ACCENT        UI_COLOR_BLACK

/* Backgrounds */
#define UI_COLOR_BG_MAIN       UI_COLOR_WHITE
#define UI_COLOR_BG_CARD       UI_COLOR_WHITE
#define UI_COLOR_BG_SIDEBAR    UI_COLOR_WHITE

/* Text */
#define UI_COLOR_TEXT_PRIMARY   UI_COLOR_BLACK
#define UI_COLOR_TEXT_SECONDARY UI_COLOR_BLACK
#define UI_COLOR_TEXT_DISABLED  UI_COLOR_WHITE

/* Status */
#define UI_COLOR_STATUS_ON     UI_COLOR_BLACK
#define UI_COLOR_STATUS_OFF    UI_COLOR_WHITE

/* Widget defaults */
#define UI_COLOR_DANGER        UI_COLOR_BLACK
#define UI_COLOR_WARNING       UI_COLOR_BLACK

/* Track / fill */
#define UI_COLOR_LIGHT_GRAY    UI_COLOR_WHITE
#define UI_COLOR_DARK_GRAY     UI_COLOR_BLACK
#define UI_COLOR_GRAY          UI_COLOR_BLACK

/*=============================================================================
 *  Color Utility (simplified for 1bpp)
 *=============================================================================*/

/* Invert color (B ↔ W) */
static inline ui_color_t ui_color_invert(ui_color_t c)
{
    if (c == UI_COLOR_BLACK) return UI_COLOR_WHITE;
    if (c == UI_COLOR_WHITE) return UI_COLOR_BLACK;
    return c;
}

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_COLOR_H */
