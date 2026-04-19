/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : MiniUI input system API.
*                      Touch/mouse/keyboard event abstraction and mapping.
********************************************************************************/
#ifndef __MINIUI_INPUT_H
#define __MINIUI_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"

/*=============================================================================
 *  Input System Configuration
 *=============================================================================*/

#define UI_SWIPE_THRESHOLD      30
#define UI_INPUT_QUEUE_SIZE     8

/*=============================================================================
 *  Input State
 *=============================================================================*/

typedef struct {
    bool touch_pressed;
    ui_point_t touch_pos;
    ui_point_t touch_start;
    bool mouse_present;
    ui_point_t mouse_pos;
    bool mouse_pressed;
    ui_widget_t *focused_widget;
    ui_widget_t *capture_widget;
} ui_input_state_t;

/*=============================================================================
 *  Input System API
 *=============================================================================*/

void ui_input_init(void);
void ui_input_touch_raw(bool pressed, int16_t x, int16_t y);
void ui_input_mouse_raw(int16_t dx, int16_t dy, bool left_pressed);
void ui_input_keyboard_raw(uint8_t key_code);
ui_event_t* ui_input_poll(void);
const ui_input_state_t* ui_input_get_state(void);
void ui_input_set_focus(ui_widget_t *widget);
void ui_input_set_capture(ui_widget_t *widget);
ui_widget_t* ui_input_get_capture(void);

/*=============================================================================
 *  Key Codes
 *=============================================================================*/

#define UI_KEY_UP       0x01
#define UI_KEY_DOWN     0x02
#define UI_KEY_LEFT     0x03
#define UI_KEY_RIGHT    0x04
#define UI_KEY_OK       0x05
#define UI_KEY_BACK     0x06
#define UI_KEY_HOME     0x07
#define UI_KEY_MENU     0x08

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_INPUT_H */
