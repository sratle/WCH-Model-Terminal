/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_input.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2026/05/22
* Description        : MiniUI unified input system implementation.
*                      Touch / Mouse / Keyboard / Core-key event processing,
*                      gesture recognition, multi-touch, multi-key.
********************************************************************************/
#include "miniui_input.h"
#include "miniui_page.h"
#include "miniui_render.h"
#include "miniui.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Internal State
 *=============================================================================*/

static ui_input_state_t s_state;
static ui_event_t s_event_queue[UI_INPUT_QUEUE_SIZE];
static uint8_t s_queue_head = 0;
static uint8_t s_queue_tail = 0;
static uint8_t s_queue_count = 0;

/* Previous keyboard key codes for diff detection */
static uint8_t s_prev_key_codes[UI_MAX_KEYBOARD_KEYS];
static uint8_t s_prev_modifiers = 0;

/* External mouse connection state (set by protocol handler) */
static bool s_ext_mouse_connected = false;

/* Previous mouse position for dirty region tracking */
static ui_point_t s_prev_mouse_pos = { UI_SCREEN_WIDTH / 2, UI_SCREEN_HEIGHT / 2 };

/*=============================================================================
 *  Queue Helpers
 *=============================================================================*/

static bool queue_push(const ui_event_t *e)
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
 *  Helpers
 *=============================================================================*/

static int16_t i16_abs(int16_t v) { return v < 0 ? -v : v; }

static void pointer_state_reset(ui_pointer_state_t *p)
{
    memset(p, 0, sizeof(*p));
}

static void key_state_reset(ui_key_state_t *k)
{
    memset(k, 0, sizeof(*k));
}

/* Map USB HID usage ID to internal UI_KEY_* code.
 * Returns UI_KEY_NONE for unmapped keys. */
static uint8_t hid_to_ui_key(uint8_t hid_code)
{
    switch (hid_code) {
        case 0x52: return UI_KEY_UP;        /* HID Up Arrow */
        case 0x51: return UI_KEY_DOWN;      /* HID Down Arrow */
        case 0x50: return UI_KEY_LEFT;      /* HID Left Arrow */
        case 0x4F: return UI_KEY_RIGHT;     /* HID Right Arrow */
        case 0x28: return UI_KEY_OK;        /* HID Enter */
        case 0x29: return UI_KEY_BACK;      /* HID Escape */
        case 0x4A: return UI_KEY_HOME;      /* HID Home */
        case 0x65: return UI_KEY_MENU;      /* HID Application */
        default:   return UI_KEY_NONE;
    }
}

/* Emit a convenience logical key event if the key maps to one */
static void emit_logical_key_event(uint8_t ui_key, ui_input_source_t source)
{
    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = source;
    e.key_code = ui_key;

    switch (ui_key) {
        case UI_KEY_UP:    e.type = UI_EVENT_KEY_UP_ARROW; break;
        case UI_KEY_DOWN:  e.type = UI_EVENT_KEY_DOWN_ARROW; break;
        case UI_KEY_LEFT:  e.type = UI_EVENT_KEY_LEFT_ARROW; break;
        case UI_KEY_RIGHT: e.type = UI_EVENT_KEY_RIGHT_ARROW; break;
        case UI_KEY_OK:    e.type = UI_EVENT_KEY_OK; break;
        case UI_KEY_BACK:  e.type = UI_EVENT_KEY_BACK; break;
        default: return;
    }
    queue_push(&e);
}

/*=============================================================================
 *  Initialization
 *=============================================================================*/

void ui_input_init(void)
{
    printf("[ui_input_init] start\r\n");
    memset(&s_state, 0, sizeof(s_state));
    memset(s_event_queue, 0, sizeof(s_event_queue));
    s_queue_head = 0;
    s_queue_tail = 0;
    s_queue_count = 0;

    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++)
        pointer_state_reset(&s_state.touches[i]);
    pointer_state_reset(&s_state.mouse);
    s_state.mouse.pos.x = UI_SCREEN_WIDTH / 2;
    s_state.mouse.pos.y = UI_SCREEN_HEIGHT / 2;

    for (int i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        key_state_reset(&s_state.keys[i]);
        s_prev_key_codes[i] = 0;
    }
    s_prev_modifiers = 0;

    /* Core keys: index 0 = +, 1 = -, 2 = Enter */
    s_state.core_keys[0].key_code = UI_KEY_CORE_PLUS;
    s_state.core_keys[1].key_code = UI_KEY_CORE_MINUS;
    s_state.core_keys[2].key_code = UI_KEY_CORE_ENTER;

    printf("[ui_input_init] done\r\n");
}

/*=============================================================================
 *  Touch Input Processing
 *=============================================================================*/

void ui_input_feed_touch(uint8_t touch_id, bool pressed, int16_t x, int16_t y)
{
    if (touch_id >= UI_MAX_TOUCH_POINTS) return;

    ui_pointer_state_t *tp = &s_state.touches[touch_id];
    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = UI_INPUT_TOUCH;
    e.touch_id = touch_id;

    if (pressed) {
        if (!tp->active) {
            /* Finger down */
            tp->active = true;
            tp->pos.x = x;
            tp->pos.y = y;
            tp->start.x = x;
            tp->start.y = y;
            tp->press_time = ui_get_real_ms();
            tp->long_press_sent = false;
            tp->swipe_locked = false;
            tp->swipe_dir = -1;

            e.type = UI_EVENT_DOWN;
            e.pos.x = x;
            e.pos.y = y;
            queue_push(&e);
        } else {
            /* Finger moved */
            int16_t dx = x - tp->pos.x;
            int16_t dy = y - tp->pos.y;
            tp->pos.x = x;
            tp->pos.y = y;

            /* Check if swipe direction should be locked */
            if (!tp->swipe_locked) {
                int16_t total_dx = tp->pos.x - tp->start.x;
                int16_t total_dy = tp->pos.y - tp->start.y;
                int16_t adx = i16_abs(total_dx);
                int16_t ady = i16_abs(total_dy);
                if (adx > UI_SWIPE_THRESHOLD || ady > UI_SWIPE_THRESHOLD) {
                    tp->swipe_locked = true;
                    if (adx > ady)
                        tp->swipe_dir = (total_dx > 0) ? 3 : 2; /* right : left */
                    else
                        tp->swipe_dir = (total_dy > 0) ? 1 : 0; /* down : up */
                }
            }

            e.type = UI_EVENT_MOVE;
            e.pos.x = x;
            e.pos.y = y;
            e.delta.x = dx;
            e.delta.y = dy;
            queue_push(&e);
        }
    } else {
        if (tp->active) {
            /* Finger up */
            tp->active = false;

            int16_t total_dx = tp->pos.x - tp->start.x;
            int16_t total_dy = tp->pos.y - tp->start.y;
            int16_t adx = i16_abs(total_dx);
            int16_t ady = i16_abs(total_dy);

            /* Always emit UP first so widgets clear PRESSED before CLICK fires */
            e.type = UI_EVENT_UP;
            e.pos = tp->pos;
            e.delta.x = 0;
            e.delta.y = 0;
            queue_push(&e);

            if (tp->swipe_locked && (adx > UI_SWIPE_THRESHOLD || ady > UI_SWIPE_THRESHOLD)) {
                /* Swipe gesture completed */
                e.pos = tp->pos;
                e.delta.x = total_dx;
                e.delta.y = total_dy;
                switch (tp->swipe_dir) {
                    case 0: e.type = UI_EVENT_SWIPE_UP; break;
                    case 1: e.type = UI_EVENT_SWIPE_DOWN; break;
                    case 2: e.type = UI_EVENT_SWIPE_LEFT; break;
                    case 3: e.type = UI_EVENT_SWIPE_RIGHT; break;
                }
                queue_push(&e);
            } else if (!tp->long_press_sent) {
                /* Short release — emit CLICK immediately */
                uint32_t now = ui_get_real_ms();
                uint32_t press_duration = now - tp->press_time;

                if (press_duration <= UI_CLICK_MAX_MS &&
                    adx <= UI_CLICK_MAX_MOVE && ady <= UI_CLICK_MAX_MOVE) {
                    /* Check double-click */
                    if (tp->click_pending &&
                        (now - tp->last_click_time) <= UI_DOUBLE_CLICK_INTERVAL) {
                        /* Double click */
                        tp->click_pending = false;
                        e.type = UI_EVENT_DOUBLE_CLICK;
                        e.pos = tp->pos;
                        queue_push(&e);
                    } else {
                        /* Single click — emit immediately, mark pending for double-click */
                        tp->click_pending = true;
                        tp->last_click_time = now;
                        tp->last_click_pos = tp->pos;
                        e.type = UI_EVENT_CLICK;
                        e.pos = tp->pos;
                        queue_push(&e);
                    }
                }
            }

            tp->long_press_sent = false;
            tp->swipe_locked = false;
        }
    }
}

/*=============================================================================
 *  Mouse Input Processing
 *=============================================================================*/

void ui_input_feed_mouse(int8_t dx, int8_t dy, uint8_t buttons, int8_t scroll)
{
    ui_pointer_state_t *mp = &s_state.mouse;
    s_state.mouse_present = true;

    /* Save previous position for dirty region */
    s_prev_mouse_pos = mp->pos;

    /* Update position */
    if (dx != 0 || dy != 0) {
        mp->pos.x += dx;
        mp->pos.y += dy;
        if (mp->pos.x < 0) mp->pos.x = 0;
        if (mp->pos.x >= UI_SCREEN_WIDTH) mp->pos.x = UI_SCREEN_WIDTH - 1;
        if (mp->pos.y < 0) mp->pos.y = 0;
        if (mp->pos.y >= UI_SCREEN_HEIGHT) mp->pos.y = UI_SCREEN_HEIGHT - 1;
    }

    uint8_t prev_buttons = s_state.mouse_buttons;
    s_state.mouse_buttons = buttons;

    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = UI_INPUT_MOUSE;
    e.touch_id = UI_TOUCH_ID_NONE;
    e.mouse_buttons = buttons;

    /* Left button transition */
    bool left_down = (buttons & UI_MOUSE_BTN_LEFT) != 0;
    bool prev_left_down = (prev_buttons & UI_MOUSE_BTN_LEFT) != 0;

    if (left_down && !prev_left_down) {
        /* Mouse button press */
        mp->active = true;
        mp->start = mp->pos;
        mp->pos = mp->pos; /* pos already updated above */
        mp->press_time = ui_get_real_ms();
        mp->long_press_sent = false;
        mp->swipe_locked = false;

        e.type = UI_EVENT_DOWN;
        e.pos = mp->pos;
        queue_push(&e);
    } else if (!left_down && prev_left_down) {
        /* Mouse button release */
        mp->active = false;

        int16_t total_dx = mp->pos.x - mp->start.x;
        int16_t total_dy = mp->pos.y - mp->start.y;
        int16_t adx = i16_abs(total_dx);
        int16_t ady = i16_abs(total_dy);

        /* Always emit UP first so widgets clear PRESSED before CLICK fires */
        e.type = UI_EVENT_UP;
        e.pos = mp->pos;
        queue_push(&e);

        if (adx > UI_SWIPE_THRESHOLD || ady > UI_SWIPE_THRESHOLD) {
            /* Swipe */
            e.pos = mp->pos;
            e.delta.x = total_dx;
            e.delta.y = total_dy;
            if (adx > ady)
                e.type = (total_dx > 0) ? UI_EVENT_SWIPE_RIGHT : UI_EVENT_SWIPE_LEFT;
            else
                e.type = (total_dy > 0) ? UI_EVENT_SWIPE_DOWN : UI_EVENT_SWIPE_UP;
            queue_push(&e);
        } else if (!mp->long_press_sent) {
            uint32_t now = ui_get_real_ms();
            uint32_t press_duration = now - mp->press_time;
            if (press_duration <= UI_CLICK_MAX_MS) {
                if (mp->click_pending &&
                    (now - mp->last_click_time) <= UI_DOUBLE_CLICK_INTERVAL) {
                    mp->click_pending = false;
                    e.type = UI_EVENT_DOUBLE_CLICK;
                    e.pos = mp->pos;
                    queue_push(&e);
                } else {
                    /* Single click — emit immediately, mark pending for double-click */
                    mp->click_pending = true;
                    mp->last_click_time = now;
                    mp->last_click_pos = mp->pos;
                    e.type = UI_EVENT_CLICK;
                    e.pos = mp->pos;
                    queue_push(&e);
                }
            }
        }

        mp->long_press_sent = false;
    } else if ((dx != 0 || dy != 0) && left_down) {
        /* Drag while pressed */
        e.type = UI_EVENT_MOVE;
        e.pos = mp->pos;
        e.delta.x = dx;
        e.delta.y = dy;
        queue_push(&e);
    } else if (dx != 0 || dy != 0) {
        /* Hover move (no button) */
        e.type = UI_EVENT_MOVE;
        e.pos = mp->pos;
        e.delta.x = dx;
        e.delta.y = dy;
        queue_push(&e);
    }

    /* Scroll wheel */
    if (scroll != 0) {
        ui_event_t se;
        memset(&se, 0, sizeof(se));
        se.source = UI_INPUT_MOUSE;
        se.type = UI_EVENT_MOVE; /* Scroll is a MOVE variant with scroll_delta */
        se.pos = mp->pos;
        se.scroll_delta = scroll;
        se.mouse_buttons = buttons;
        queue_push(&se);
    }
}

/*=============================================================================
 *  Keyboard Input Processing
 *=============================================================================*/

/* Find key slot by HID code, returns -1 if not found */
static int8_t find_key_slot(uint8_t hid_code)
{
    for (int8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        if (s_state.keys[i].pressed && s_state.keys[i].key_code == hid_code)
            return i;
    }
    return -1;
}

/* Find free key slot, returns -1 if full */
static int8_t find_free_key_slot(void)
{
    for (int8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        if (!s_state.keys[i].pressed)
            return i;
    }
    return -1;
}

void ui_input_feed_keyboard(uint8_t modifiers, const uint8_t key_codes[6])
{
    s_state.key_modifiers = modifiers;

    /* Detect newly released keys (in prev but not in current) */
    for (int8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        if (s_prev_key_codes[i] == 0) continue;
        bool still_pressed = false;
        for (int8_t j = 0; j < UI_MAX_KEYBOARD_KEYS; j++) {
            if (key_codes[j] == s_prev_key_codes[i]) {
                still_pressed = true;
                break;
            }
        }
        if (!still_pressed) {
            uint8_t released_hid = s_prev_key_codes[i];
            int8_t slot = find_key_slot(released_hid);
            if (slot >= 0) {
                ui_key_state_t *ks = &s_state.keys[slot];
                uint8_t ui_key = hid_to_ui_key(released_hid);

                /* Key UP event */
                ui_event_t e;
                memset(&e, 0, sizeof(e));
                e.source = UI_INPUT_KEYBOARD;
                e.type = UI_EVENT_KEY_UP;
                e.key_code = ui_key ? ui_key : released_hid;
                e.key_modifiers = modifiers;
                queue_push(&e);

                /* Check for click (short press) */
                if (!ks->long_press_sent) {
                    uint32_t now = ui_get_real_ms();
                    uint32_t duration = now - ks->press_time;
                    if (duration <= UI_CLICK_MAX_MS) {
                        if (ks->click_pending &&
                            (now - ks->last_click_time) <= UI_DOUBLE_CLICK_INTERVAL) {
                            /* Double click */
                            ks->click_pending = false;
                            e.type = UI_EVENT_KEY_DOUBLE_CLICK;
                            e.key_code = ui_key ? ui_key : released_hid;
                            e.key_modifiers = modifiers;
                            queue_push(&e);
                        } else {
                            ks->click_pending = true;
                            ks->last_click_time = now;
                        }
                    }
                }

                /* Emit logical key UP for mapped keys */
                if (ui_key) {
                    e.type = UI_EVENT_KEY_UP; /* logical UP already emitted via KEY_UP_ARROW etc. on down */
                }

                ks->pressed = false;
                ks->long_press_sent = false;
                /* NOTE: do NOT clear click_pending here!
                 * check_click_timeouts() needs it to fire KEY_CLICK after
                 * the double-click window expires. */
            }
        }
    }

    /* Detect newly pressed keys (in current but not in prev) */
    for (int8_t j = 0; j < UI_MAX_KEYBOARD_KEYS; j++) {
        if (key_codes[j] == 0) continue;
        bool was_pressed = false;
        for (int8_t i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
            if (s_prev_key_codes[i] == key_codes[j]) {
                was_pressed = true;
                break;
            }
        }
        if (!was_pressed) {
            uint8_t new_hid = key_codes[j];
            int8_t slot = find_free_key_slot();
            if (slot < 0) continue;

            ui_key_state_t *ks = &s_state.keys[slot];
            ks->key_code = new_hid;
            ks->pressed = true;
            ks->press_time = ui_get_real_ms();
            ks->long_press_sent = false;
            ks->click_pending = false;

            uint8_t ui_key = hid_to_ui_key(new_hid);

            /* Key DOWN event */
            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.source = UI_INPUT_KEYBOARD;
            e.type = UI_EVENT_KEY_DOWN;
            e.key_code = ui_key ? ui_key : new_hid;
            e.key_modifiers = modifiers;
            queue_push(&e);

            /* Convenience logical key event */
            if (ui_key) {
                emit_logical_key_event(ui_key, UI_INPUT_KEYBOARD);
            }
        }
    }

    /* Save current key codes for next diff */
    memcpy(s_prev_key_codes, key_codes, UI_MAX_KEYBOARD_KEYS);
    s_prev_modifiers = modifiers;
}

/*=============================================================================
 *  Core Key Input Processing
 *=============================================================================*/

void ui_input_feed_core_key(uint8_t key_id, uint8_t action)
{
    /* key_id: 0x01 = +, 0x02 = -, 0x03 = Enter (per protocol) */
    /* action: 0x00 = release, 0x01 = press, 0x02 = long-press */
    int8_t idx = -1;
    uint8_t ui_key = UI_KEY_NONE;

    switch (key_id) {
        case 0x01: idx = 0; ui_key = UI_KEY_CORE_PLUS;  break;
        case 0x02: idx = 1; ui_key = UI_KEY_CORE_MINUS; break;
        case 0x03: idx = 2; ui_key = UI_KEY_CORE_ENTER; break;
        default: return;
    }

    ui_key_state_t *ks = &s_state.core_keys[idx];
    ui_event_t e;
    memset(&e, 0, sizeof(e));
    e.source = UI_INPUT_CORE_KEY;
    e.key_code = ui_key;

    switch (action) {
        case 0x01: /* Press */
            ks->pressed = true;
            ks->press_time = ui_get_real_ms();
            ks->long_press_sent = false;
            ks->click_pending = false;

            e.type = UI_EVENT_KEY_DOWN;
            queue_push(&e);

            /* Also emit logical key event */
            if (ui_key == UI_KEY_CORE_PLUS)
                emit_logical_key_event(UI_KEY_UP, UI_INPUT_CORE_KEY);
            else if (ui_key == UI_KEY_CORE_MINUS)
                emit_logical_key_event(UI_KEY_DOWN, UI_INPUT_CORE_KEY);
            else if (ui_key == UI_KEY_CORE_ENTER)
                emit_logical_key_event(UI_KEY_OK, UI_INPUT_CORE_KEY);
            break;

        case 0x00: /* Release */
            ks->pressed = false;

            e.type = UI_EVENT_KEY_UP;
            queue_push(&e);

            /* Click detection */
            if (!ks->long_press_sent) {
                uint32_t now = ui_get_real_ms();
                uint32_t duration = now - ks->press_time;
                if (duration <= UI_CLICK_MAX_MS) {
                    if (ks->click_pending &&
                        (now - ks->last_click_time) <= UI_DOUBLE_CLICK_INTERVAL) {
                        ks->click_pending = false;
                        e.type = UI_EVENT_KEY_DOUBLE_CLICK;
                        queue_push(&e);
                    } else {
                        ks->click_pending = true;
                        ks->last_click_time = now;
                    }
                }
            }
            ks->long_press_sent = false;
            break;

        case 0x02: /* Long press (detected by Core firmware) */
            ks->long_press_sent = true;

            e.type = UI_EVENT_KEY_LONG_PRESS;
            queue_push(&e);

            /* Also emit logical long-press for Enter = system wake */
            if (ui_key == UI_KEY_CORE_ENTER) {
                e.type = UI_EVENT_KEY_LONG_PRESS;
                e.key_code = UI_KEY_OK;
                queue_push(&e);
            }
            break;
    }
}

/*=============================================================================
 *  Time-Based Gesture Detection (called every tick)
 *=============================================================================*/

static void check_pointer_hold(ui_pointer_state_t *p, ui_input_source_t source, uint8_t touch_id)
{
    if (!p->active) return;

    uint32_t now = ui_get_real_ms();
    uint32_t elapsed = now - p->press_time;

    /* Don't start long-press if finger has moved too far */
    int16_t dx = p->pos.x - p->start.x;
    int16_t dy = p->pos.y - p->start.y;
    if (i16_abs(dx) > UI_SWIPE_MAX_START_MOVE || i16_abs(dy) > UI_SWIPE_MAX_START_MOVE)
        return;

    if (!p->long_press_sent) {
        if (elapsed >= UI_LONG_PRESS_DELAY_MS) {
            p->long_press_sent = true;
            p->last_long_time = now;

            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_LONG_PRESS;
            e.source = source;
            e.touch_id = touch_id;
            e.pos = p->pos;
            queue_push(&e);
        }
    } else {
        if (now - p->last_long_time >= UI_LONG_PRESS_REPEAT_MS) {
            p->last_long_time = now;

            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_LONG_PRESS_REPEAT;
            e.source = source;
            e.touch_id = touch_id;
            e.pos = p->pos;
            queue_push(&e);
        }
    }
}

static void check_key_hold(ui_key_state_t *ks, ui_input_source_t source)
{
    if (!ks->pressed) return;

    uint32_t now = ui_get_real_ms();
    uint32_t elapsed = now - ks->press_time;

    if (!ks->long_press_sent) {
        if (elapsed >= UI_LONG_PRESS_DELAY_MS) {
            ks->long_press_sent = true;
            ks->last_long_time = now;

            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_KEY_LONG_PRESS;
            e.source = source;
            e.key_code = ks->key_code;
            queue_push(&e);
        }
    } else {
        if (now - ks->last_long_time >= UI_LONG_PRESS_REPEAT_MS) {
            ks->last_long_time = now;

            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_KEY_LONG_REPEAT;
            e.source = source;
            e.key_code = ks->key_code;
            queue_push(&e);
        }
    }
}

static void check_click_timeouts(void)
{
    uint32_t now = ui_get_real_ms();

    /* Touch: click already emitted immediately on UP, just clear stale pending flags */
    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++) {
        ui_pointer_state_t *tp = &s_state.touches[i];
        if (tp->click_pending && (now - tp->last_click_time) > UI_DOUBLE_CLICK_INTERVAL) {
            tp->click_pending = false;
        }
    }

    /* Mouse: click already emitted immediately on UP, just clear stale pending flags */
    ui_pointer_state_t *mp = &s_state.mouse;
    if (mp->click_pending && (now - mp->last_click_time) > UI_DOUBLE_CLICK_INTERVAL) {
        mp->click_pending = false;
    }

    /* Keyboard key click timeouts */
    for (int i = 0; i < UI_MAX_KEYBOARD_KEYS; i++) {
        ui_key_state_t *ks = &s_state.keys[i];
        if (ks->click_pending && (now - ks->last_click_time) > UI_DOUBLE_CLICK_INTERVAL) {
            ks->click_pending = false;
            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_KEY_CLICK;
            e.source = UI_INPUT_KEYBOARD;
            e.key_code = ks->key_code;
            queue_push(&e);
        }
    }

    /* Core key click timeouts */
    for (int i = 0; i < 3; i++) {
        ui_key_state_t *ks = &s_state.core_keys[i];
        if (ks->click_pending && (now - ks->last_click_time) > UI_DOUBLE_CLICK_INTERVAL) {
            ks->click_pending = false;
            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_KEY_CLICK;
            e.source = UI_INPUT_CORE_KEY;
            e.key_code = ks->key_code;
            queue_push(&e);
        }
    }
}

void ui_input_tick(void)
{
    /* Check pointer holds */
    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++)
        check_pointer_hold(&s_state.touches[i], UI_INPUT_TOUCH, (uint8_t)i);
    check_pointer_hold(&s_state.mouse, UI_INPUT_MOUSE, UI_TOUCH_ID_NONE);

    /* Check keyboard key holds */
    for (int i = 0; i < UI_MAX_KEYBOARD_KEYS; i++)
        check_key_hold(&s_state.keys[i], UI_INPUT_KEYBOARD);

    /* Core keys: long-press is detected by Core firmware, but we still check
     * for keys that might not have long-press from Core side */
    for (int i = 0; i < 3; i++)
        check_key_hold(&s_state.core_keys[i], UI_INPUT_CORE_KEY);

    /* Check click timeouts (single click confirmation) */
    check_click_timeouts();
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
    return &s_state;
}

void ui_input_set_focus(ui_widget_t *widget)
{
    s_state.focused_widget = widget;
}

void ui_input_set_capture(ui_widget_t *widget, uint8_t touch_id)
{
    s_state.capture_widget = widget;
    s_state.capture_touch_id = touch_id;
}

ui_widget_t* ui_input_get_capture(void)
{
    return s_state.capture_widget;
}

uint8_t ui_input_get_capture_touch_id(void)
{
    return s_state.capture_touch_id;
}

/*=============================================================================
 *  Mouse Cursor
 *=============================================================================*/

ui_point_t ui_input_get_mouse_pos(void)
{
    return s_state.mouse.pos;
}

bool ui_input_is_mouse_cursor_visible(void)
{
    return s_ext_mouse_connected;
}

void ui_input_set_mouse_connected(bool connected)
{
    if (s_ext_mouse_connected == connected) return;
    s_ext_mouse_connected = connected;

    if (connected) {
        /* Initialize mouse position to (400, 240) */
        s_state.mouse.pos.x = 400;
        s_state.mouse.pos.y = 240;
        s_prev_mouse_pos = s_state.mouse.pos;
    }

    /* Invalidate cursor area so it appears/disappears */
    ui_input_invalidate_cursor();
}

void ui_input_invalidate_cursor(void)
{
    if (!s_ext_mouse_connected) return;

    /* Mouse cursor size: 12x18 pixels */
    #define CURSOR_W  12
    #define CURSOR_H  18

    /* Invalidate old position */
    ui_rect_t r;
    r.x = s_prev_mouse_pos.x;
    r.y = s_prev_mouse_pos.y;
    r.w = CURSOR_W;
    r.h = CURSOR_H;
    ui_page_invalidate(&r);

    /* Invalidate new position */
    r.x = s_state.mouse.pos.x;
    r.y = s_state.mouse.pos.y;
    ui_page_invalidate(&r);
}
