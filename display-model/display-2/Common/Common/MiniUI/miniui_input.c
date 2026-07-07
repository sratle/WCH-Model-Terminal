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
    /* Gesture tracking */
    int16_t start_x;
    int16_t start_y;
    uint32_t start_time;
    int16_t max_dx;       /* Max absolute delta from start */
    int16_t max_dy;
} ui_touch_state_t;

static ui_touch_state_t s_touch[UI_MAX_TOUCH_POINTS];
static ui_widget_t *s_capture[UI_MAX_TOUCH_POINTS];

/*=============================================================================
 *  Gesture Thresholds
 *=============================================================================*/

#define GESTURE_CLICK_TIME_MS      200
#define GESTURE_LONG_PRESS_TIME_MS 500
#define GESTURE_SWIPE_THRESHOLD    60    /* pixels of movement to be a swipe */
#define GESTURE_CLICK_THRESHOLD    15    /* pixels of movement max for click */

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

/*---------------------------------------------------------------------------
 *  Internal: deliver an event through the standard chain
 *    capture widget -> sidebar -> page handler -> hit-tested widget
 *---------------------------------------------------------------------------*/
static void ui_input_deliver_event(uint8_t id, ui_event_t *e)
{
    ui_page_t *page = ui_page_current();
    if (!page) return;

    int16_t x = e->touch.x;
    int16_t y = e->touch.y;

    /* 1. Deliver to capture widget first */
    if (s_capture[id]) {
        ui_widget_event(s_capture[id], e);
        return;
    }

    /* 2. Sidebar gets priority for events in its area (skipped on
     *    fullscreen pages, where the sidebar is not drawn and the
     *    whole screen is owned by the page — e.g. game pages whose
     *    Back/LAUNCH buttons live at x < SIDEBAR_WIDTH). */
    bool is_fullscreen = (page->flags & UI_PAGE_FLAG_FULLSCREEN) != 0;
    int16_t sb_w = ui_page_get_sidebar_width();
    ui_sidebar_event_cb_t sidebar_cb = ui_page_get_sidebar_event_cb();
    if (!is_fullscreen && sidebar_cb && sb_w > 0 && x < sb_w) {
        if (sidebar_cb(e)) return;
    }

    /* 3. Deliver to page event handler */
    if (page->on_page_event) {
        if (page->on_page_event(page, e)) return;
    }

    /* 4. Deliver to hit-tested widgets (reverse order for top-most first) */
    for (int16_t i = (int16_t)page->widget_count - 1; i >= 0; i--) {
        ui_widget_t *w = page->widgets[i];
        if (w && (w->flags & UI_WIDGET_FLAG_VISIBLE) && (w->flags & UI_WIDGET_FLAG_ENABLED)) {
            if (ui_widget_hit_test(w, x, y)) {
                ui_widget_event(w, e);
                break;
            }
        }
    }
}

/*---------------------------------------------------------------------------
 *  Internal: synthesize CLICK / SWIPE / LONG_PRESS from touch sequence
 *---------------------------------------------------------------------------*/
static void ui_input_synthesize_gesture(uint8_t id, ui_touch_state_t *t)
{
    uint32_t now = ui_get_real_ms();
    uint32_t duration = now - t->start_time;
    int16_t dx = t->x - t->start_x;
    int16_t dy = t->y - t->start_y;
    int16_t abs_dx = (dx < 0) ? -dx : dx;
    int16_t abs_dy = (dy < 0) ? -dy : dy;
    int16_t max_move = (abs_dx > abs_dy) ? abs_dx : abs_dy;

    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = UI_INPUT_TOUCH;
    e.touch.x = t->x;
    e.touch.y = t->y;
    e.touch.id = id;

    if (max_move < GESTURE_CLICK_THRESHOLD) {
        /* Small movement — determine click vs long press by time */
        if (duration >= GESTURE_LONG_PRESS_TIME_MS) {
            e.type = UI_EVENT_LONG_PRESS;
        } else {
            e.type = UI_EVENT_CLICK;
        }
    } else if (max_move >= GESTURE_SWIPE_THRESHOLD) {
        /* Large movement — determine swipe direction */
        if (abs_dx > abs_dy) {
            e.type = (dx > 0) ? UI_EVENT_SWIPE_RIGHT : UI_EVENT_SWIPE_LEFT;
        } else {
            e.type = (dy > 0) ? UI_EVENT_SWIPE_DOWN : UI_EVENT_SWIPE_UP;
        }
    } else {
        /* Movement between thresholds — treat as click */
        e.type = UI_EVENT_CLICK;
    }

    printf("[input] gesture: type=%d dur=%lums dx=%d dy=%d at (%d,%d)\r\n",
           e.type, duration, dx, dy, t->x, t->y);

    ui_input_deliver_event(id, &e);
}

void ui_input_inject_touch(uint8_t id, int16_t x, int16_t y, uint8_t pressed)
{
    if (id >= UI_MAX_TOUCH_POINTS) return;

    ui_touch_state_t *t = &s_touch[id];
    ui_page_t *page = ui_page_current();
    if (!page) return;

    bool was_pressed = t->pressed;
    uint32_t now = ui_get_real_ms();

    /* Track gesture info */
    if (!was_pressed && pressed) {
        /* TOUCH_DOWN: record start */
        t->start_x = x;
        t->start_y = y;
        t->start_time = now;
        t->max_dx = 0;
        t->max_dy = 0;
    } else if (was_pressed && pressed) {
        /* TOUCH_MOVE: track max movement */
        int16_t dx = (x - t->start_x < 0) ? -(x - t->start_x) : (x - t->start_x);
        int16_t dy = (y - t->start_y < 0) ? -(y - t->start_y) : (y - t->start_y);
        if (dx > t->max_dx) t->max_dx = dx;
        if (dy > t->max_dy) t->max_dy = dy;
    }

    t->x = x;
    t->y = y;
    t->pressed = pressed;
    t->id = id;

    /* Build event */
    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = UI_INPUT_TOUCH;
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

    /* Deliver the raw touch event */
    ui_input_deliver_event(id, &e);

    /* On TOUCH_UP, also synthesize a gesture event */
    if (e.type == UI_EVENT_TOUCH_UP) {
        /* Clear capture on release */
        s_capture[id] = NULL;
        /* Synthesize gesture only if the event wasn't consumed by a capture widget */
        ui_input_synthesize_gesture(id, t);
    } else if (e.type == UI_EVENT_TOUCH_DOWN) {
        /* Set capture to the widget that received DOWN (if any) */
        /* Capture is set inside ui_input_deliver_event via hit testing */
        /* We need to re-check which widget was hit.
         * On fullscreen pages the sidebar is inactive, so we always
         * allow widget capture regardless of x position. */
        int16_t sb_w = ui_page_get_sidebar_width();
        bool is_fullscreen = (page->flags & UI_PAGE_FLAG_FULLSCREEN) != 0;
        if (is_fullscreen || x >= sb_w || sb_w == 0) {
            for (int16_t i = (int16_t)page->widget_count - 1; i >= 0; i--) {
                ui_widget_t *w = page->widgets[i];
                if (w && (w->flags & UI_WIDGET_FLAG_VISIBLE) && (w->flags & UI_WIDGET_FLAG_ENABLED)) {
                    if (ui_widget_hit_test(w, x, y)) {
                        s_capture[id] = w;
                        break;
                    }
                }
            }
        }
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
        e.touch.x = UI_SCREEN_WIDTH / 2;
        e.touch.y = UI_SCREEN_HEIGHT / 2;
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
