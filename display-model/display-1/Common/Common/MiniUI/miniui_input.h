/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.h
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2026/05/22
* Description        : MiniUI unified input system.
*                      Touch / Mouse / Keyboard / Core-key event abstraction,
*                      gesture recognition (click, double-click, long-press,
*                      swipe), multi-touch, multi-key support.
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

/* Gesture timing thresholds */
#define UI_CLICK_MAX_MS            300     /* Max press duration for a click */
#define UI_CLICK_MAX_MOVE          15      /* Max movement for a click (px) */
#define UI_DOUBLE_CLICK_INTERVAL   200     /* Max interval between two clicks (ms) */
#define UI_LONG_PRESS_DELAY_MS     500     /* Hold time before first LONG_PRESS */
#define UI_LONG_PRESS_REPEAT_MS    100     /* Interval between LONG_PRESS_REPEAT */
#define UI_SWIPE_THRESHOLD         30      /* Min distance for swipe (px) */
#define UI_SWIPE_MAX_START_MOVE    20      /* Max initial movement before swipe lock */

/* Queue & tracking */
#define UI_INPUT_QUEUE_SIZE        32
#define UI_MAX_TOUCH_POINTS        5
#define UI_MAX_KEYBOARD_KEYS       6       /* Max simultaneous non-modifier keys (USB HID) */

/*=============================================================================
 *  Per-Pointer Tracking State (Touch / Mouse)
 *=============================================================================*/

typedef struct {
    bool active;                /* Currently pressed */
    ui_point_t pos;             /* Current position */
    ui_point_t start;           /* Position at press time */
    uint32_t press_time;        /* Timestamp of press (ms) */
    uint32_t last_long_time;    /* Timestamp of last LONG_PRESS_REPEAT */
    bool long_press_sent;       /* First LONG_PRESS already emitted */
    bool swipe_locked;          /* Swipe direction determined */
    int8_t swipe_dir;           /* 0=up 1=down 2=left 3=right, valid when swipe_locked */
    uint32_t last_click_time;   /* Timestamp of last CLICK (for double-click) */
    ui_point_t last_click_pos;  /* Position of last CLICK */
    bool click_pending;         /* Waiting to see if double-click follows */
} ui_pointer_state_t;

/*=============================================================================
 *  Per-Key Tracking State
 *=============================================================================*/

typedef struct {
    uint8_t key_code;           /* USB HID usage ID or internal key code */
    bool pressed;               /* Currently held */
    uint32_t press_time;        /* Timestamp of press */
    uint32_t last_long_time;    /* Timestamp of last KEY_LONG_REPEAT */
    bool long_press_sent;       /* First KEY_LONG_PRESS emitted */
    uint32_t last_click_time;   /* Timestamp of last KEY_CLICK */
    bool click_pending;         /* Waiting for double-click window */
} ui_key_state_t;

/*=============================================================================
 *  Input System Global State
 *=============================================================================*/

typedef struct {
    /* Touch: per-finger tracking */
    ui_pointer_state_t touches[UI_MAX_TOUCH_POINTS];

    /* Mouse: single pointer + button state + scroll */
    ui_pointer_state_t mouse;
    bool mouse_present;
    uint8_t mouse_buttons;      /* Current button bitmask */
    int8_t scroll_accum;        /* Accumulated scroll delta */

    /* Keyboard: modifier + tracked keys */
    uint8_t key_modifiers;      /* Current modifier bitmask */
    ui_key_state_t keys[UI_MAX_KEYBOARD_KEYS];

    /* Core keys: + / - / Enter */
    ui_key_state_t core_keys[3]; /* 0=+, 1=-, 2=Enter */

    /* Focus / capture */
    ui_widget_t *focused_widget;
    ui_widget_t *capture_widget;
    uint8_t capture_touch_id;   /* Which touch ID holds capture */
} ui_input_state_t;

/*=============================================================================
 *  Input System API — Initialization
 *=============================================================================*/

void ui_input_init(void);

/*=============================================================================
 *  Raw Input Feeds — called by drivers (touch, UART, etc.)
 *=============================================================================*/

/* Touch: feed raw multi-touch point from GT911 */
void ui_input_feed_touch(uint8_t touch_id, bool pressed, int16_t x, int16_t y);

/* Mouse: feed relative movement + button state from Core protocol */
void ui_input_feed_mouse(int8_t dx, int8_t dy, uint8_t buttons, int8_t scroll);

/* Keyboard: feed USB HID keyboard report from Core protocol */
void ui_input_feed_keyboard(uint8_t modifiers, const uint8_t key_codes[6]);

/* Core key: feed onboard button event from Core protocol */
void ui_input_feed_core_key(uint8_t key_id, uint8_t action);

/*=============================================================================
 *  Event Polling — called by UI_Tick
 *=============================================================================*/

/* Process raw inputs, recognize gestures, and return next event (or NULL) */
ui_event_t* ui_input_poll(void);

/* Called every tick to check time-based gestures (long-press, click timeout) */
void ui_input_tick(void);

/*=============================================================================
 *  State Access
 *=============================================================================*/

const ui_input_state_t* ui_input_get_state(void);
void ui_input_set_focus(ui_widget_t *widget);
void ui_input_set_capture(ui_widget_t *widget, uint8_t touch_id);
ui_widget_t* ui_input_get_capture(void);
uint8_t ui_input_get_capture_touch_id(void);

/*=============================================================================
 *  Mouse Cursor
 *=============================================================================*/

/* 获取鼠标光标当前位置（屏幕坐标） */
ui_point_t ui_input_get_mouse_pos(void);

/* 鼠标光标是否可见（外接鼠标已连接） */
bool ui_input_is_mouse_cursor_visible(void);

/* 设置外接鼠标连接状态（由 UART 协议处理调用） */
void ui_input_set_mouse_connected(bool connected);

/* 标记鼠标光标脏区域（旧位置 + 新位置），在 ui_page_draw 之前调用 */
void ui_input_invalidate_cursor(void);

/* 渲染完成后调用，更新已渲染的光标位置跟踪 */
void ui_input_cursor_rendered(void);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_INPUT_H */
