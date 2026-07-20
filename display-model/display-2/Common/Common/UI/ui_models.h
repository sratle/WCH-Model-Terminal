/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_models.h
* Author             : E-ink Model Team
* Version            : V4.0.0
* Date               : 2026/06/12
* Description        : Models page header.
********************************************************************************/
#ifndef __UI_MODELS_H
#define __UI_MODELS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void ui_models_init(void);
void ui_models_enter(ui_page_t *page);

/* Called by the UART module when a module insert/remove event arrives;
 * re-fetches lsdev if the Models page is currently visible. */
void ui_models_notify_module_change(void);

#ifdef __cplusplus
}
#endif

#endif /* __UI_MODELS_H */
