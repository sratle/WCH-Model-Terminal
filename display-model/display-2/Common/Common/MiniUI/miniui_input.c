/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI input handling for E-ink display.
*                      Adapted for CH32V307 platform using systick for timing.
********************************************************************************/
#include "miniui_input.h"
#include "miniui_widget.h"
#include "miniui_page.h"
#include "miniui_render.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Platform Timing — CH32V307 SysTick
 *=============================================================================*/

#include "ch32v30x.h"

uint32_t ui_get_real_ms(void)
{
    /* CH32V30x: SysTick is 24-bit downcounter at HCLK frequency.
     * We use the millisecond counter maintained by the system. */
    return (uint32_t)(SysTick->CNT / (SystemCoreClock / 1000));
}

/*=============================================================================
 *  Touch State
 *=============================================================================*/

/* Use UI_MAX_TOUCH_POINTS from miniui_types.h (default 5) */

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t pressed;
    uint8_t id;
} ui_touch_state_t;

static ui_touch_state_t s_touch[UI_MAX_TOUCH_POINTS];
static ui_widget_t *s_capture[UI_MAX_TOUCH_POINTS];

/*=============================================================================
 *  Input Implementation
 *=============================================================================*/

void ui_input_init(void)
{
    memset(s_touch, 0, sizeof(s_touch));
    memset(s_capture, 0, sizeof(s_capture));
}

void ui_input_set_capture(ui_widget_t *w, uint8_t touch_id)
{
    if (touch_id < UI_MAX_TOUCH_POINTS) {
        s_capture[touch_id] = w;
    }
}

ui_widget_t* ui_input_get_capture(uint8_t touch_id)
{
    if (touch_id < UI_MAX_TOUCH_POINTS) return s_capture[touch_id];
    return NULL;
}

void ui_input_inject_touch(uint8_t id, int16_t x, int16_t y, uint8_t pressed)
{
    if (id >= UI_MAX_TOUCH_POINTS) return;

    ui_touch_state_t *t = &s_touch[id];
    ui_page_t *page = ui_page_current();
    if (!page) return;

    bool was_pressed = t->pressed;
    t->x = x;
    t->y = y;
    t->pressed = pressed;
    t->id = id;

    /* Build event */
    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.touch.x = x;
    e.touch.y = y;
    e.touch.id = id;

    if (!was_pressed && pressed) {
        e.type = UI_EVENT_TOUCH_DOWN;
    } else if (was_pressed && !pressed) {
        e.type = UI_EVENT_TOUCH_UP;
    } else if (was_pressed && pressed) {
        e.type = UI_EVENT_TOUCH_MOVE;
    } else {
        return;  /* No state change */
    }

    /* Deliver to capture widget first */
    if (s_capture[id]) {
        ui_widget_event(s_capture[id], &e);
        if (e.type == UI_EVENT_TOUCH_UP) {
            s_capture[id] = NULL;
        }
        return;
    }

    /* Deliver to page event handler */
    if (page->on_page_event) {
        page->on_page_event(page, &e);
    }

    /* Deliver to widgets (reverse order for top-most first) */
    for (int16_t i = (int16_t)page->widget_count - 1; i >= 0; i--) {
        ui_widget_t *w = page->widgets[i];
        if (w && (w->flags & UI_WIDGET_FLAG_VISIBLE) && (w->flags & UI_WIDGET_FLAG_ENABLED)) {
            if (ui_widget_hit_test(w, x, y)) {
                ui_widget_event(w, &e);
                if (e.type == UI_EVENT_TOUCH_DOWN) {
                    s_capture[id] = w;
                }
                break;
            }
        }
    }

    /* Check sidebar */
    ui_sidebar_event_cb_t sidebar_cb = ui_page_get_sidebar_event_cb();
    if (sidebar_cb && x < 80) {  /* sidebar width threshold */
        sidebar_cb(&e);
    }
}

void ui_input_process(void)
{
    /* Polling-based input processing.
     * Touch events are injected externally via ui_input_inject_touch().
     * This function can be used for periodic polling if needed. */
}

/*=============================================================================
 *  Raw Input Feeds — called by UART protocol handler
 *=============================================================================*/

void ui_input_feed_touch(uint8_t touch_id, bool pressed, int16_t x, int16_t y)
{
    ui_input_inject_touch(touch_id, x, y, pressed ? 1 : 0);
}

void ui_input_feed_mouse(int8_t dx, int8_t dy, uint8_t buttons, int8_t scroll)
{
    /* E-ink display: mouse support is limited.
     * Map mouse movement to touch events for basic navigation. */
    (void)scroll;
    static int16_t mx = 200, my = 150;
    static bool was_pressed = false;

    mx += dx;
    my += dy;
    if (mx < 0) mx = 0;
    if (mx >= UI_SCREEN_WIDTH) mx = UI_SCREEN_WIDTH - 1;
    if (my < 0) my = 0;
    if (my >= UI_SCREEN_HEIGHT) my = UI_SCREEN_HEIGHT - 1;

    bool pressed = (buttons & 0x01) != 0;
    if (pressed && !was_pressed) {
        ui_input_inject_touch(0, mx, my, 1);
    } else if (!pressed && was_pressed) {
        ui_input_inject_touch(0, mx, my, 0);
    } else if (pressed) {
        ui_input_inject_touch(0, mx, my, 1);
    }
    was_pressed = pressed;
}

void ui_input_feed_keyboard(uint8_t modifiers, const uint8_t key_codes[6])
{
    /* E-ink display: keyboard input mapped to core key events.
     * Enter → core key 2 (confirm), Escape → back, arrows → core keys 0/1 */
    (void)modifiers;
    if (!key_codes) return;

    for (uint8_t i = 0; i < 6; i++) {
        uint8_t kc = key_codes[i];
        if (kc == 0) continue;

        switch (kc) {
        case 0x28:  /* Enter */
            ui_input_feed_core_key(2, 0x01);
            break;
        case 0x29:  /* Escape */
            ui_page_pop();
            break;
        case 0x52:  /* Up */
            ui_input_feed_core_key(0, 0x01);
            break;
        case 0x51:  /* Down */
            ui_input_feed_core_key(1, 0x01);
            break;
        default:
            break;
        }
    }
}

void ui_input_feed_core_key(uint8_t key_id, uint8_t action)
{
    /* Core keys: 0=+, 1=-, 2=Enter
     * Map to navigation events on e-ink display */
    if (action != 0x01) return;  /* Only handle press */

    ui_page_t *page = ui_page_current();
    if (!page) return;

    ui_event_t e;
    memset(&e, 0, sizeof(e));

    switch (key_id) {
    case 0:  /* + key → swipe up / next */
        e.type = UI_EVENT_SWIPE_UP;
        break;
    case 1:  /* - key → swipe down / prev */
        e.type = UI_EVENT_SWIPE_DOWN;
        break;
    case 2:  /* Enter key → click at center */
        e.type = UI_EVENT_CLICK;
        e.pos.x = UI_SCREEN_WIDTH / 2;
        e.pos.y = UI_SCREEN_HEIGHT / 2;
        break;
    default:
        return;
    }

    /* Deliver to page event handler */
    if (page->on_page_event) {
        page->on_page_event(page, &e);
    }
}

void ui_input_set_mouse_connected(bool connected)
{
    (void)connected;
    /* E-ink: no mouse cursor rendering, just accept the call */
}
