/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.h
* Author             : LCD Model Team
* Version            : V1.0.0
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

#define UI_SWIPE_THRESHOLD      30      /* Minimum pixels for swipe detection */
#define UI_INPUT_QUEUE_SIZE     8       /* Event queue size */

/*=============================================================================
 *  Input State
 *=============================================================================*/

typedef struct {
    /* Touch state */
    bool touch_pressed;
    ui_point_t touch_pos;
    ui_point_t touch_start;
    
    /* Mouse state */
    bool mouse_present;
    ui_point_t mouse_pos;
    bool mouse_pressed;
    
    /* Keyboard focus */
    ui_widget_t *focused_widget;
} ui_input_state_t;

/*=============================================================================
 *  Input System API
 *=============================================================================*/

/* Initialize input system */
void ui_input_init(void);

/* Process raw touch input (called from touch driver) */
void ui_input_touch_raw(bool pressed, int16_t x, int16_t y);

/* Process raw mouse input (called from UART handler) */
void ui_input_mouse_raw(int16_t dx, int16_t dy, bool left_pressed);

/* Process raw keyboard input (called from UART handler) */
void ui_input_keyboard_raw(uint8_t key_code);

/* Poll for input events - returns next event or NULL if none */
ui_event_t* ui_input_poll(void);

/* Get current input state */
const ui_input_state_t* ui_input_get_state(void);

/* Set focused widget for keyboard navigation */
void ui_input_set_focus(ui_widget_t *widget);

/*=============================================================================
 *  Key Codes (for UART keyboard input)
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
