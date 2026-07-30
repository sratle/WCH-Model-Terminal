/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_font.h
* Description        : Semantic font slots for MiniUI.
*                      Applications reference UI_FONT_* slots instead of a
*                      concrete font, so a future size change only needs to
*                      remap the slots below (kept as macros so they remain
*                      valid constant initializers for static tables).
********************************************************************************/
#ifndef __UI_FONT_H
#define __UI_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "font_montserrat_16.h"
#include "font_montserrat_24.h"

/* Semantic slots:
 *   UI_FONT_TITLE   - page/app titles, primary list text   (24 px)
 *   UI_FONT_BODY    - default widget/body text             (16 px)
 *   UI_FONT_CAPTION - secondary/annotation text            (16 px) */
#define UI_FONT_TITLE    (&font_montserrat_24)
#define UI_FONT_BODY     (&font_montserrat_16)
#define UI_FONT_CAPTION  (&font_montserrat_16)

#ifdef __cplusplus
}
#endif

#endif /* __UI_FONT_H */
