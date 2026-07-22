/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI input handling API for E-ink display.
********************************************************************************/
#ifndef __MINIUI_INPUT_H
#define __MINIUI_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"

/*=============================================================================
 *  Input API
 *=============================================================================*/

void ui_input_init(void);
void ui_input_process(void);
void ui_input_inject_touch(uint8_t id, int16_t x, int16_t y, uint8_t pressed);
void ui_input_set_capture(ui_widget_t *w, uint8_t touch_id);
ui_widget_t* ui_input_get_capture(uint8_t touch_id);
uint32_t ui_get_real_ms(void);

/* Raw input feeds — called by UART protocol handler */
void ui_input_feed_touch(uint8_t touch_id, bool pressed, int16_t x, int16_t y);
void ui_input_feed_mouse(int8_t dx, int8_t dy, uint8_t buttons, int8_t scroll);
void ui_input_feed_keyboard(uint8_t modifiers, const uint8_t key_codes[6]);
void ui_input_feed_core_key(uint8_t key_id, uint8_t action);
void ui_input_set_mouse_connected(bool connected);

/* Called by a widget/page that acts on a right-button click, so the global
 * "right-click = Back" fallback is suppressed for that click. */
void ui_input_consume_rightclick(void);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_INPUT_H */
