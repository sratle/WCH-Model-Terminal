/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : MiniUI input system implementation.
*                      Touch/mouse/keyboard event abstraction and mapping.
********************************************************************************/
#include "miniui_input.h"
#include "miniui_page.h"
#include <string.h>

/*=============================================================================
 *  Internal State
 *=============================================================================*/

static ui_input_state_t s_input_state;
static ui_event_t s_event_queue[UI_INPUT_QUEUE_SIZE];
static uint8_t s_queue_head = 0;
static uint8_t s_queue_tail = 0;
static uint8_t s_queue_count = 0;

/*=============================================================================
 *  Queue Helpers
 *=============================================================================*/

static bool queue_push(ui_event_t *e)
{
    if (s_queue_count >= UI_INPUT_QUEUE_SIZE) return false;

    s_event_queue[s_queue_tail] = *e;
    s_queue_tail = (s_queue_tail + 1) % UI_INPUT_QUEUE_SIZE;
    s_queue_count++;
    return true;
}

static bool queue_pop(ui_event_t *e)
{
    if (s_queue_count == 0) return false;

    *e = s_event_queue[s_queue_head];
    s_queue_head = (s_queue_head + 1) % UI_INPUT_QUEUE_SIZE;
    s_queue_count--;
    return true;
}

/*=============================================================================
 *  Input System Implementation
 *=============================================================================*/

void ui_input_init(void)
{
    memset(&s_input_state, 0, sizeof(s_input_state));
    memset(s_event_queue, 0, sizeof(s_event_queue));
    s_queue_head = 0;
    s_queue_tail = 0;
    s_queue_count = 0;
    s_input_state.mouse_pos.x = UI_SCREEN_WIDTH / 2;
    s_input_state.mouse_pos.y = UI_SCREEN_HEIGHT / 2;
}

/*=============================================================================
 *  Touch Input Processing
 *=============================================================================*/

void ui_input_touch_raw(bool pressed, int16_t x, int16_t y)
{
    ui_event_t e;
    e.source = UI_INPUT_TOUCH;

    if (pressed) {
        if (!s_input_state.touch_pressed) {
            s_input_state.touch_pressed = true;
            s_input_state.touch_pos.x = x;
            s_input_state.touch_pos.y = y;
            s_input_state.touch_start.x = x;
            s_input_state.touch_start.y = y;

            e.type = UI_EVENT_PRESS;
            e.pos.x = x;
            e.pos.y = y;
            e.delta.x = 0;
            e.delta.y = 0;
            queue_push(&e);
        } else {
            int16_t dx = x - s_input_state.touch_pos.x;
            int16_t dy = y - s_input_state.touch_pos.y;

            s_input_state.touch_pos.x = x;
            s_input_state.touch_pos.y = y;

            e.type = UI_EVENT_DRAG;
            e.pos.x = x;
            e.pos.y = y;
            e.delta.x = dx;
            e.delta.y = dy;
            queue_push(&e);
        }
    } else {
        if (s_input_state.touch_pressed) {
            int16_t dx = s_input_state.touch_pos.x - s_input_state.touch_start.x;
            int16_t dy = s_input_state.touch_pos.y - s_input_state.touch_start.y;
            int16_t abs_dx = (dx < 0) ? -dx : dx;
            int16_t abs_dy = (dy < 0) ? -dy : dy;

            s_input_state.touch_pressed = false;

            if (abs_dx > UI_SWIPE_THRESHOLD || abs_dy > UI_SWIPE_THRESHOLD) {
                if (abs_dx > abs_dy) {
                    e.type = (dx > 0) ? UI_EVENT_SWIPE_RIGHT : UI_EVENT_SWIPE_LEFT;
                } else {
                    e.type = (dy > 0) ? UI_EVENT_SWIPE_DOWN : UI_EVENT_SWIPE_UP;
                }
                e.pos = s_input_state.touch_pos;
                e.delta.x = dx;
                e.delta.y = dy;
                queue_push(&e);
            }

            e.type = UI_EVENT_RELEASE;
            e.pos = s_input_state.touch_pos;
            e.delta.x = 0;
            e.delta.y = 0;
            queue_push(&e);
        }
    }
}

/*=============================================================================
 *  Mouse Input Processing
 *=============================================================================*/

void ui_input_mouse_raw(int16_t dx, int16_t dy, bool left_pressed)
{
    ui_event_t e;
    e.source = UI_INPUT_MOUSE;

    s_input_state.mouse_pos.x += dx;
    s_input_state.mouse_pos.y += dy;

    if (s_input_state.mouse_pos.x < 0) s_input_state.mouse_pos.x = 0;
    if (s_input_state.mouse_pos.x >= UI_SCREEN_WIDTH) s_input_state.mouse_pos.x = UI_SCREEN_WIDTH - 1;
    if (s_input_state.mouse_pos.y < 0) s_input_state.mouse_pos.y = 0;
    if (s_input_state.mouse_pos.y >= UI_SCREEN_HEIGHT) s_input_state.mouse_pos.y = UI_SCREEN_HEIGHT - 1;

    s_input_state.mouse_present = true;

    if (left_pressed && !s_input_state.mouse_pressed) {
        s_input_state.mouse_pressed = true;
        e.type = UI_EVENT_PRESS;
        e.pos = s_input_state.mouse_pos;
        e.delta.x = dx;
        e.delta.y = dy;
        queue_push(&e);
    } else if (!left_pressed && s_input_state.mouse_pressed) {
        s_input_state.mouse_pressed = false;
        e.type = UI_EVENT_RELEASE;
        e.pos = s_input_state.mouse_pos;
        e.delta.x = dx;
        e.delta.y = dy;
        queue_push(&e);
    } else if (dx != 0 || dy != 0) {
        e.type = UI_EVENT_DRAG;
        e.pos = s_input_state.mouse_pos;
        e.delta.x = dx;
        e.delta.y = dy;
        queue_push(&e);
    }
}

/*=============================================================================
 *  Keyboard Input Processing
 *=============================================================================*/

void ui_input_keyboard_raw(uint8_t key_code)
{
    ui_event_t e;
    e.source = UI_INPUT_KEYBOARD;
    e.pos.x = 0;
    e.pos.y = 0;
    e.delta.x = 0;
    e.delta.y = 0;

    switch (key_code) {
        case UI_KEY_UP:    e.type = UI_EVENT_KEY_UP; break;
        case UI_KEY_DOWN:  e.type = UI_EVENT_KEY_DOWN; break;
        case UI_KEY_LEFT:  e.type = UI_EVENT_KEY_LEFT; break;
        case UI_KEY_RIGHT: e.type = UI_EVENT_KEY_RIGHT; break;
        case UI_KEY_OK:    e.type = UI_EVENT_KEY_OK; break;
        case UI_KEY_BACK:  e.type = UI_EVENT_KEY_BACK; break;
        default: return;
    }

    queue_push(&e);
}

/*=============================================================================
 *  Event Polling
 *=============================================================================*/

ui_event_t* ui_input_poll(void)
{
    static ui_event_t e;
    if (queue_pop(&e)) return &e;
    return NULL;
}

/*=============================================================================
 *  State Access
 *=============================================================================*/

const ui_input_state_t* ui_input_get_state(void)
{
    return &s_input_state;
}

void ui_input_set_focus(ui_widget_t *widget)
{
    s_input_state.focused_widget = widget;
}

void ui_input_set_capture(ui_widget_t *widget)
{
    s_input_state.capture_widget = widget;
}

ui_widget_t* ui_input_get_capture(void)
{
    return s_input_state.capture_widget;
}
