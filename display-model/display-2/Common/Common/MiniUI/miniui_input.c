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
#include "../Touch/touch_matrix.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Platform Timing — TIM2 free-running 1 kHz tick
 *
 *  NOTE: SysTick->CNT cannot be used here: the WCH Delay_Us/Delay_Ms
 *  helpers reprogram the SysTick counter on every call, so a CNT-derived
 *  millisecond value is meaningless (always ~0).  Platform_Time runs on
 *  TIM2 and is immune to that.
 *=============================================================================*/

#include "ch32v30x.h"
#include "../platform_time.h"

uint32_t ui_get_real_ms(void)
{
    return Platform_Millis();
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
    Platform_Time_Init();   /* start the free-running 1 kHz tick */
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
 *  Internal: synthesize CLICK / DOUBLE_CLICK / SWIPE / LONG_PRESS
 *---------------------------------------------------------------------------*/
#define GESTURE_DOUBLE_CLICK_TIME_MS 300
#define GESTURE_DOUBLE_CLICK_DIST_PX 20

static void ui_input_synthesize_gesture(uint8_t id, ui_touch_state_t *t)
{
    static uint32_t s_last_click_time = 0;
    static int16_t  s_last_click_x = -1000;
    static int16_t  s_last_click_y = -1000;

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
            /* Click — check for double click (time + distance window) */
            int16_t cdx = t->x - s_last_click_x;
            int16_t cdy = t->y - s_last_click_y;
            if (cdx < 0) cdx = -cdx;
            if (cdy < 0) cdy = -cdy;
            if ((now - s_last_click_time) < GESTURE_DOUBLE_CLICK_TIME_MS &&
                cdx < GESTURE_DOUBLE_CLICK_DIST_PX &&
                cdy < GESTURE_DOUBLE_CLICK_DIST_PX) {
                e.type = UI_EVENT_DOUBLE_CLICK;
                s_last_click_time = 0;      /* require 3rd tap for next DC */
            } else {
                e.type = UI_EVENT_CLICK;
                s_last_click_time = now;
                s_last_click_x = t->x;
                s_last_click_y = t->y;
            }
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

/*=============================================================================
 *  Mouse State (external HID mouse forwarded by Core)
 *=============================================================================*/

static bool    s_mouse_connected = false;  /* external mouse present */
static uint8_t s_mouse_buttons = 0;        /* current button bitmask */
static int16_t s_scroll_accum = 0;         /* pending wheel delta (batched) */

/* Set by a widget/page that acts on a right-button click, to suppress the
 * global "right-click = Back" fallback for that click (see ui_input_feed_mouse). */
static bool    s_rc_consumed = false;
void ui_input_consume_rightclick(void) { s_rc_consumed = true; }

/* Nonlinear mouse acceleration (same feel as Display-1):
 * |raw| <= 1  →  1 px  (precise)
 * |raw| == 2  →  4 px
 * |raw| == 3  →  7 px
 * |raw| >= 4  →  raw * raw / 2  (aggressive) */
static int16_t mouse_accel(int8_t raw)
{
    if (raw == 0) return 0;
    int16_t sign = (raw > 0) ? 1 : -1;
    int16_t mag = (raw > 0) ? raw : -raw;

    int16_t out;
    if (mag <= 1) {
        out = 1;
    } else if (mag <= 3) {
        out = (mag * mag + mag) / 2;   /* quadratic ramp: 2→4, 3→7 */
    } else {
        out = (mag * mag) / 2;         /* aggressive: mag^2 / 2 */
    }
    return sign * out;
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
    /* Auto-enable mouse state when any mouse data is received
     * (CH9350 external mouse or BLE HID mouse, forwarded by Core) */
    if (!s_mouse_connected) {
        s_mouse_connected = true;
    }

    /* Move the shared cursor (same pointer the touchpad drives) */
    if (dx != 0 || dy != 0) {
        TouchMatrix_MoveCursor(mouse_accel(dx), mouse_accel(dy));
    }

    uint8_t prev_buttons = s_mouse_buttons;
    s_mouse_buttons = buttons;

    int16_t cx = TouchMatrix_GetCursorX();
    int16_t cy = TouchMatrix_GetCursorY();

    /* --- Left button: touch injection (click / drag via gesture synth) --- */
    bool left_down = (buttons & UI_MOUSE_BTN_LEFT) != 0;
    bool prev_left_down = (prev_buttons & UI_MOUSE_BTN_LEFT) != 0;

    if (left_down && !prev_left_down) {
        ui_input_inject_touch(0, cx, cy, 1);           /* TOUCH_DOWN at cursor */
    } else if (!left_down && prev_left_down) {
        ui_input_inject_touch(0, cx, cy, 0);           /* TOUCH_UP → CLICK */
    } else if (left_down && (dx != 0 || dy != 0)) {
        ui_input_inject_touch(0, cx, cy, 1);           /* drag → TOUCH_MOVE */
    }

    /* --- Right button: click delivers a MOUSE CLICK with button mask
     *     (apps use it for context menus, same as Display-1) --- */
    bool right_down = (buttons & UI_MOUSE_BTN_RIGHT) != 0;
    bool prev_right_down = (prev_buttons & UI_MOUSE_BTN_RIGHT) != 0;

    if (!right_down && prev_right_down) {
        s_rc_consumed = false;
        ui_event_t e;
        memset(&e, 0, sizeof(e));
        e.type = UI_EVENT_CLICK;
        e.source = UI_INPUT_MOUSE;
        e.touch.x = cx;
        e.touch.y = cy;
        e.touch.id = UI_TOUCH_ID_NONE;
        e.mouse_buttons = UI_MOUSE_BTN_RIGHT;
        ui_input_deliver_event(0, &e);

        /* Unified "right-click = Back": if no widget/page claimed the click
         * (via ui_input_consume_rightclick), treat it as a Back action — the
         * current page's on_page_event gets first say (context-sensitive back,
         * e.g. e-book closes the book), otherwise pop the page. */
        if (!s_rc_consumed) {
            ui_page_t *pg = ui_page_current();
            ui_event_t back;
            memset(&back, 0, sizeof(back));
            back.type = UI_EVENT_KEY_BACK;
            back.source = UI_INPUT_KEYBOARD;
            back.key.code = UI_KEY_BACK;
            bool handled = (pg && pg->on_page_event) ? pg->on_page_event(pg, &back) : false;
            if (!handled && ui_page_can_go_back()) ui_page_pop();
        }
    }

    /* --- Wheel: accumulate, delivered batched once per tick in
     *     ui_input_process() (keeps e-paper refreshes coalesced) --- */
    if (scroll != 0) {
        s_scroll_accum += scroll;
        if (s_scroll_accum > 512) s_scroll_accum = 512;
        if (s_scroll_accum < -512) s_scroll_accum = -512;
    }
}

/*=============================================================================
 *  Keyboard Input Processing
 *
 *  Full HID keyboard support (reports forwarded by Core from CH9350 USB
 *  keyboard / Keyboard module / BLE HID keyboard):
 *    - newly-pressed keys emit their mapped event immediately
 *    - held keys auto-repeat after UI_KEY_REPEAT_DELAY_MS, every
 *      UI_KEY_REPEAT_INTERVAL_MS (Escape excluded)
 *    - navigation keys become dedicated UI events; printable keys carry
 *      their ASCII in char_code (shift-aware) so text-editing apps work
 *=============================================================================*/

#define UI_KEY_REPEAT_DELAY_MS     500
#define UI_KEY_REPEAT_INTERVAL_MS  100

static struct {
    uint8_t  hid_code;        /* 0 = empty slot */
    uint8_t  modifiers;       /* modifiers captured at press time */
    uint32_t press_ms;
    uint32_t last_repeat_ms;
} s_held_keys[UI_MAX_KEYBOARD_KEYS];

/* Map a HID usage ID to its UI event. Returns false for unmappable keys. */
static bool build_key_event(uint8_t kc, uint8_t modifiers, ui_event_t *e)
{
    /* HID usage -> (unshifted, shifted) ASCII for punctuation */
    static const char s_punct_map[][2] = {
        {0x2D, 0x5F},   /* 0x2D: - _ */
        {0x3D, 0x2B},   /* 0x2E: = + */
        {0x5B, 0x7B},   /* 0x2F: [ { */
        {0x5D, 0x7D},   /* 0x30: ] } */
        {0x5C, 0x7C},   /* 0x31: \ | */
        {0x3B, 0x3A},   /* 0x33: ; : */
        {0x27, 0x22},   /* 0x34: ' " */
        {0x60, 0x7E},   /* 0x35: ` ~ */
        {0x2C, 0x3C},   /* 0x36: , < */
        {0x2E, 0x3E},   /* 0x37: . > */
        {0x2F, 0x3F},   /* 0x38: / ? */
    };
    static const char s_digit_shift[] = ")!@#$%^&*(";

    bool shift = (modifiers & (UI_MOD_LSHIFT | UI_MOD_RSHIFT)) != 0;

    memset(e, 0, sizeof(*e));
    e->source = UI_INPUT_KEYBOARD;
    e->key.modifiers = modifiers;

    switch (kc) {
    /* --- Navigation / control keys -> dedicated events --- */
    case 0x28:  /* Enter */
        e->type = UI_EVENT_KEY_OK;
        e->key.code = UI_KEY_OK;
        e->char_code = 0x0D;   /* CR, matches Display-1 (apps act on KEY_OK) */
        return true;
    case 0x29:  /* Escape */
        e->type = UI_EVENT_KEY_BACK;
        e->key.code = UI_KEY_BACK;
        return true;
    case 0x2A:  /* Backspace */
        e->type = UI_EVENT_KEY_DOWN;
        e->char_code = 0x08;
        return true;
    case 0x2B:  /* Tab */
        e->type = UI_EVENT_KEY_DOWN;
        e->key.code = UI_KEY_TAB;
        e->char_code = 0x09;
        return true;
    case 0x2C:  /* Space */
        e->type = UI_EVENT_KEY_DOWN;
        e->char_code = ' ';
        return true;
    case 0x4C:  /* Delete (forward) */
        e->type = UI_EVENT_KEY_DOWN;
        e->char_code = 0x7F;
        return true;
    case 0x4A:  /* Home */
        e->type = UI_EVENT_KEY_DOWN;
        e->key.code = UI_KEY_HOME;
        return true;
    case 0x4D:  /* End */
        e->type = UI_EVENT_KEY_DOWN;
        e->key.code = UI_KEY_END;
        return true;
    case 0x4B:  /* PageUp -> swipe up (page back) */
        e->type = UI_EVENT_SWIPE_UP;
        return true;
    case 0x4E:  /* PageDown -> swipe down (page forward) */
        e->type = UI_EVENT_SWIPE_DOWN;
        return true;
    case 0x4F:  /* Right arrow */
        e->type = UI_EVENT_KEY_RIGHT_ARROW;
        e->key.code = UI_KEY_RIGHT;
        return true;
    case 0x50:  /* Left arrow (USB HID 0x50) */
        e->type = UI_EVENT_KEY_LEFT_ARROW;
        e->key.code = UI_KEY_LEFT;
        return true;
    case 0x51:  /* Down arrow (USB HID 0x51) */
        e->type = UI_EVENT_KEY_DOWN_ARROW;
        e->key.code = UI_KEY_DOWN;
        return true;
    case 0x52:  /* Up arrow */
        e->type = UI_EVENT_KEY_UP_ARROW;
        e->key.code = UI_KEY_UP;
        return true;
    case 0x65:  /* Application (Menu) key */
        e->type = UI_EVENT_KEY_DOWN;
        e->key.code = UI_KEY_MENU;
        return true;
    default:
        /* --- Printable characters --- */
        if (kc >= 0x04 && kc <= 0x1D) {         /* a-z */
            char c = (char)('a' + (kc - 0x04));
            if (shift) c = (char)(c - 'a' + 'A');
            e->type = UI_EVENT_KEY_DOWN;
            e->char_code = (uint8_t)c;
        } else if (kc >= 0x1E && kc <= 0x26) {  /* 1-9 */
            char c = shift ? s_digit_shift[kc - 0x1E + 1]
                           : (char)('1' + (kc - 0x1E));
            e->type = UI_EVENT_KEY_DOWN;
            e->char_code = (uint8_t)c;
        } else if (kc == 0x27) {                /* 0 */
            e->type = UI_EVENT_KEY_DOWN;
            e->char_code = (uint8_t)(shift ? ')' : '0');
        } else if (kc >= 0x2D && kc <= 0x31) {  /* - = [ ] \ */
            e->type = UI_EVENT_KEY_DOWN;
            e->char_code = (uint8_t)s_punct_map[kc - 0x2D][shift ? 1 : 0];
        } else if (kc >= 0x33 && kc <= 0x38) {  /* ; ' ` , . / */
            e->type = UI_EVENT_KEY_DOWN;
            e->char_code = (uint8_t)s_punct_map[5 + (kc - 0x33)][shift ? 1 : 0];
        }
        break;
    }

    return e->type != UI_EVENT_NONE;
}

/* Deliver a key event: page handler first; if not consumed, broadcast to
 * all visible+enabled widgets (mirrors Display-1's non-pointer dispatch).
 * This lets key-originated swipes (PageUp/PageDown) and arrow events reach
 * list/content widgets that handle scrolling at the widget level. */
static void deliver_key_event(ui_event_t *e)
{
    ui_page_t *page = ui_page_current();
    if (!page) return;

    if (page->on_page_event) {
        if (page->on_page_event(page, e)) return;
    }

    /* TAB cycles keyboard focus (PRESSED highlight) among the page's focusable
     * widgets; Shift+TAB reverses.  KEY_OK then activates the focused button /
     * toggles a switch, arrow keys adjust the focused slider. */
    if (e->type == UI_EVENT_KEY_DOWN && e->key.code == UI_KEY_TAB) {
        bool reverse = (e->key.modifiers & (UI_MOD_LSHIFT | UI_MOD_RSHIFT)) != 0;
        int16_t n = (int16_t)page->widget_count;
        int16_t dir = reverse ? -1 : 1;
        int16_t cur = -1;
        for (int16_t i = 0; i < n; i++) {
            if (page->widgets[i] && (page->widgets[i]->flags & UI_WIDGET_FLAG_PRESSED)) {
                cur = i; break;
            }
        }
        int16_t base = (cur >= 0) ? cur : (reverse ? n : -1);
        for (int16_t step = 1; step <= n; step++) {
            int16_t idx = (int16_t)((((base + dir * step) % n) + n) % n);
            ui_widget_t *cand = page->widgets[idx];
            if (cand && (cand->flags & UI_WIDGET_FLAG_FOCUSABLE) &&
                (cand->flags & UI_WIDGET_FLAG_VISIBLE) &&
                (cand->flags & UI_WIDGET_FLAG_ENABLED)) {
                if (cur >= 0 && page->widgets[cur]) {
                    page->widgets[cur]->flags &= ~UI_WIDGET_FLAG_PRESSED;
                    ui_widget_invalidate(page->widgets[cur]);
                }
                cand->flags |= UI_WIDGET_FLAG_PRESSED;
                ui_widget_invalidate(cand);
                break;
            }
        }
        return;
    }

    for (int16_t i = 0; i < (int16_t)page->widget_count; i++) {
        ui_widget_t *w = page->widgets[i];
        if (w && (w->flags & UI_WIDGET_FLAG_VISIBLE) && (w->flags & UI_WIDGET_FLAG_ENABLED)) {
            ui_widget_event(w, e);
        }
    }
}

void ui_input_feed_keyboard(uint8_t modifiers, const uint8_t key_codes[6])
{
    if (!key_codes) return;

    uint32_t now = ui_get_real_ms();

    /* Diff: release held keys that disappeared from the report */
    for (uint8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        if (s_held_keys[i].hid_code == 0) continue;
        bool still_held = false;
        for (uint8_t j = 0; j < UI_MAX_KEYBOARD_KEYS; j++) {
            if (key_codes[j] == s_held_keys[i].hid_code) {
                still_held = true;
                break;
            }
        }
        if (!still_held) s_held_keys[i].hid_code = 0;
    }

    /* Diff: emit events only for newly pressed keys (HID reports carry the
     * full pressed set; without this diff every report would re-fire all
     * held keys) */
    for (uint8_t j = 0; j < UI_MAX_KEYBOARD_KEYS; j++) {
        uint8_t kc = key_codes[j];
        if (kc == 0) continue;

        bool already_held = false;
        for (uint8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
            if (s_held_keys[i].hid_code == kc) {
                already_held = true;
                break;
            }
        }
        if (already_held) continue;

        int8_t slot = -1;
        for (uint8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
            if (s_held_keys[i].hid_code == 0) { slot = (int8_t)i; break; }
        }
        if (slot < 0) continue;   /* no free slot: drop extra keys */

        s_held_keys[slot].hid_code = kc;
        s_held_keys[slot].modifiers = modifiers;
        s_held_keys[slot].press_ms = now;
        s_held_keys[slot].last_repeat_ms = now;

        ui_event_t e;
        if (build_key_event(kc, modifiers, &e)) {
            deliver_key_event(&e);
        }
    }
}

void ui_input_feed_core_key(uint8_t key_id, uint8_t action)
{
    /* Core keys (per protocol): 0x01 = '+', 0x02 = '-', 0x03 = Enter
     * Map to navigation events on e-ink display */
    if (action != 0x01) return;  /* Only handle press */

    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = UI_INPUT_CORE_KEY;

    switch (key_id) {
    case 0x01:  /* + key → swipe up / next */
        e.type = UI_EVENT_SWIPE_UP;
        e.key.code = UI_KEY_CORE_PLUS;
        deliver_key_event(&e);
        break;
    case 0x02:  /* - key → swipe down / prev */
        e.type = UI_EVENT_SWIPE_DOWN;
        e.key.code = UI_KEY_CORE_MINUS;
        deliver_key_event(&e);
        break;
    case 0x03:  /* Enter key → click at the current cursor position */
        e.type = UI_EVENT_CLICK;
        e.key.code = UI_KEY_CORE_ENTER;
        e.touch.x = TouchMatrix_GetCursorX();
        e.touch.y = TouchMatrix_GetCursorY();
        e.touch.id = UI_TOUCH_ID_NONE;
        ui_input_deliver_event(0, &e);
        break;
    default:
        return;
    }
}

void ui_input_set_mouse_connected(bool connected)
{
    if (s_mouse_connected == connected) return;
    s_mouse_connected = connected;

    if (!connected) {
        /* Mouse disconnected: release a held left button so the injected
         * touch capture cannot get stuck, and drop pending wheel deltas.
         * The shared cursor stays visible — the local touchpad still
         * needs it (unlike Display-1, which hides the cursor). */
        if (s_mouse_buttons & UI_MOUSE_BTN_LEFT) {
            ui_input_inject_touch(0, TouchMatrix_GetCursorX(),
                                  TouchMatrix_GetCursorY(), 0);
        }
        s_mouse_buttons = 0;
        s_scroll_accum = 0;
    }
}

/*=============================================================================
 *  Periodic Input Processing — called every UI tick
 *=============================================================================*/

void ui_input_process(void)
{
    /* 1. Deliver the accumulated mouse wheel delta as ONE batched scroll
     * event per tick.  A fast wheel spin bursts many UART reports into a
     * single main-loop iteration; delivering each report directly would
     * trigger one ~300ms e-paper refresh per notch.  Batching collapses
     * the burst into a single larger scroll step and a single refresh. */
    if (s_scroll_accum != 0) {
        int16_t delta = s_scroll_accum;
        s_scroll_accum = 0;
        if (delta > 127) delta = 127;
        if (delta < -128) delta = -128;

        ui_event_t e;
        memset(&e, 0, sizeof(e));
        e.type = UI_EVENT_MOVE;   /* scroll is a MOVE variant with scroll_delta */
        e.source = UI_INPUT_MOUSE;
        e.touch.x = TouchMatrix_GetCursorX();
        e.touch.y = TouchMatrix_GetCursorY();
        e.touch.id = UI_TOUCH_ID_NONE;
        e.scroll_delta = (int8_t)delta;
        e.mouse_buttons = s_mouse_buttons;
        ui_input_deliver_event(0, &e);
    }

    /* 2. Key auto-repeat: re-emit held keys after the initial delay.
     * KEY_DOWN events become KEY_LONG_REPEAT for apps that distinguish;
     * dedicated events (arrows, OK, ...) repeat as-is.  Escape is excluded
     * so a held ESC cannot walk back multiple pages. */
    uint32_t now = ui_get_real_ms();
    for (uint8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        if (s_held_keys[i].hid_code == 0) continue;
        if (s_held_keys[i].hid_code == 0x29) continue;   /* Escape */
        if ((uint32_t)(now - s_held_keys[i].press_ms) < UI_KEY_REPEAT_DELAY_MS)
            continue;
        if ((uint32_t)(now - s_held_keys[i].last_repeat_ms) < UI_KEY_REPEAT_INTERVAL_MS)
            continue;

        s_held_keys[i].last_repeat_ms = now;

        ui_event_t e;
        if (build_key_event(s_held_keys[i].hid_code, s_held_keys[i].modifiers, &e)) {
            if (e.type == UI_EVENT_KEY_DOWN)
                e.type = UI_EVENT_KEY_LONG_REPEAT;
            deliver_key_event(&e);
        }
    }
}
