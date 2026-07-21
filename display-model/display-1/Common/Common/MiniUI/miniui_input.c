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

/* Last rendered cursor position for dirty region tracking.
 * Updated after each render cycle to track where cursor was actually drawn. */
static ui_point_t s_cursor_rendered_pos = { -1, -1 };

/*=============================================================================
 *  Queue Helpers
 *=============================================================================*/

static bool queue_push(const ui_event_t *e)
{
    if (s_queue_count >= UI_INPUT_QUEUE_SIZE) {
        /* Queue full: for critical events (UP / CLICK), drop the oldest
         * MOVE event to make room.  Losing a MOVE is harmless; losing an
         * UP causes stuck captures and unresponsive input. */
        bool is_critical = (e->type == UI_EVENT_UP ||
                            e->type == UI_EVENT_CLICK ||
                            e->type == UI_EVENT_DOUBLE_CLICK ||
                            e->type == UI_EVENT_SWIPE_UP ||
                            e->type == UI_EVENT_SWIPE_DOWN ||
                            e->type == UI_EVENT_SWIPE_LEFT ||
                            e->type == UI_EVENT_SWIPE_RIGHT);
        if (!is_critical) return false;

        /* Scan from head for the oldest MOVE event and remove it */
        int idx = s_queue_head;
        for (int i = 0; i < s_queue_count; i++) {
            if (s_event_queue[idx].type == UI_EVENT_MOVE) {
                /* Shift all events before this one forward by 1 */
                int prev = idx;
                for (int j = i - 1; j >= 0; j--) {
                    int pidx = (s_queue_head + j) % UI_INPUT_QUEUE_SIZE;
                    s_event_queue[prev] = s_event_queue[pidx];
                    prev = pidx;
                }
                s_queue_head = (s_queue_head + 1) % UI_INPUT_QUEUE_SIZE;
                s_queue_count--;
                break;
            }
            idx = (idx + 1) % UI_INPUT_QUEUE_SIZE;
        }
        /* If no MOVE found, drop oldest event (head) */
        if (s_queue_count >= UI_INPUT_QUEUE_SIZE) {
            s_queue_head = (s_queue_head + 1) % UI_INPUT_QUEUE_SIZE;
            s_queue_count--;
        }
    }
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

/* Scroll events coalesce with a pending scroll event at the queue tail:
 * a fast wheel spin would otherwise flood the queue with MOVE events —
 * and MOVE events are the first to be dropped when the queue fills,
 * losing wheel ticks and making scrolling jumpy.  Accumulating preserves
 * every tick as one larger delta. */
static bool queue_push_scroll(ui_event_t *e)
{
    if (s_queue_count > 0) {
        uint8_t last = (s_queue_tail + UI_INPUT_QUEUE_SIZE - 1) % UI_INPUT_QUEUE_SIZE;
        ui_event_t *le = &s_event_queue[last];
        if (le->type == UI_EVENT_MOVE && le->source == UI_INPUT_MOUSE &&
            le->scroll_delta != 0) {
            int16_t sum = le->scroll_delta + e->scroll_delta;
            if (sum > 127) sum = 127;
            if (sum < -128) sum = -128;
            le->scroll_delta = (int8_t)sum;
            le->pos = e->pos;
            return true;
        }
    }
    return queue_push(e);
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
        case 0x4D: return UI_KEY_END;       /* HID End */
        case 0x65: return UI_KEY_MENU;      /* HID Application */
        case 0x2B: return UI_KEY_TAB;       /* HID Tab */
        default:   return UI_KEY_NONE;
    }
}

/* Convert USB HID usage ID to printable ASCII character.
 * Uses standard US QWERTY mapping per USB HID Usage Tables (Page 0x07).
 * HID codes are determined by physical key position, independent of
 * what characters are printed on the keycaps (keyboard layout).
 * Returns 0 for non-printable keys.  Applies shift modifier. */
static uint8_t hid_to_ascii(uint8_t hid_code, uint8_t modifiers)
{
    bool shift = (modifiers & (UI_MOD_LSHIFT | UI_MOD_RSHIFT)) != 0;

    /* Letters: HID 0x04 (A) .. 0x1D (Z) */
    if (hid_code >= 0x04 && hid_code <= 0x1D) {
        uint8_t c = 'a' + (hid_code - 0x04);
        if (shift) c -= 32;  /* uppercase */
        return c;
    }

    /* Numbers: HID 0x1E (1) .. 0x26 (9), 0x27 (0) */
    if (hid_code >= 0x1E && hid_code <= 0x26) {
        if (shift) {
            static const char shifted[] = "!@#$%^&*(";
            return shifted[hid_code - 0x1E];
        }
        return '1' + (hid_code - 0x1E);
    }
    if (hid_code == 0x27) return shift ? ')' : '0';

    /* Control characters */
    if (hid_code == 0x28) return 0x0D;  /* Enter */
    if (hid_code == 0x29) return 0x1B;  /* Escape */
    if (hid_code == 0x2A) return 0x08;  /* Backspace */
    if (hid_code == 0x2B) return 0x09;  /* Tab */
    if (hid_code == 0x2C) return ' ';   /* Space */
    if (hid_code == 0x4C) return 0x7F;  /* Delete (DEL) */

    /* Punctuation: HID 0x2D (Minus) .. 0x32 (Non-US #) */
    if (hid_code >= 0x2D && hid_code <= 0x32) {
        static const char normal[]  = "-=[]\\'";
        static const char shifted_c[] = "_+{}|\"";
        return shift ? shifted_c[hid_code - 0x2D] : normal[hid_code - 0x2D];
    }

    /* Punctuation: HID 0x33 (Semicolon) .. 0x38 (Slash) */
    if (hid_code >= 0x33 && hid_code <= 0x38) {
        static const char normal[]  = ";'`,./";
        static const char shifted_c[] = ":\"~<>?";
        return shift ? shifted_c[hid_code - 0x33] : normal[hid_code - 0x33];
    }

    return 0;  /* Non-printable */
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
        case UI_KEY_TAB:   e.type = UI_EVENT_KEY_TAB; break;
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
 *  Mouse Acceleration Helper
 *=============================================================================*/

/* Nonlinear acceleration: small movements = precise, large movements = fast.
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
        /* Quadratic ramp: 2→4, 3→7 */
        out = (mag * mag + mag) / 2;
    } else {
        /* Aggressive: mag^2 / 2 */
        out = (mag * mag) / 2;
    }
    return sign * out;
}

/*=============================================================================
 *  Touch Input Processing
 *=============================================================================*/

void ui_input_feed_touch(uint8_t touch_id, bool pressed, int16_t x, int16_t y)
{
    if (touch_id >= UI_MAX_TOUCH_POINTS) return;

    ui_pointer_state_t *tp = &s_state.touches[touch_id];
    tp->last_feed_time = ui_get_real_ms();
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
    /* Auto-enable mouse cursor when pointer data is received (from CH9350
     * external mouse, gamepad ROC2, or BLE).  Scroll-only reports (e.g. the
     * touch-ring wheel forwarded by Core) must NOT pop up the cursor —
     * they carry no pointing intent. */
    if (!s_ext_mouse_connected && (dx != 0 || dy != 0 || buttons != 0)) {
        s_ext_mouse_connected = true;
    }
    s_state.mouse_present = true;
    mp->last_feed_time = ui_get_real_ms();

    /* Update position with nonlinear acceleration for better feel */
    if (dx != 0 || dy != 0) {
        mp->pos.x += mouse_accel(dx);
        mp->pos.y += mouse_accel(dy);
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
        e.delta.x = mouse_accel(dx);
        e.delta.y = mouse_accel(dy);
        queue_push(&e);
    } else if (dx != 0 || dy != 0) {
        /* Hover move (no button) */
        e.type = UI_EVENT_MOVE;
        e.pos = mp->pos;
        e.delta.x = mouse_accel(dx);
        e.delta.y = mouse_accel(dy);
        queue_push(&e);
    }

    /* Right button: release emits a CLICK carrying the right-button mask
     * (context menus).  The left-button gesture path above never fires for
     * right clicks, so without this branch right-click was unreachable. */
    bool right_down = (buttons & UI_MOUSE_BTN_RIGHT) != 0;
    bool prev_right_down = (prev_buttons & UI_MOUSE_BTN_RIGHT) != 0;
    if (!right_down && prev_right_down) {
        ui_event_t re;
        memset(&re, 0, sizeof(re));
        re.source = UI_INPUT_MOUSE;
        re.touch_id = UI_TOUCH_ID_NONE;
        re.type = UI_EVENT_CLICK;
        re.pos = mp->pos;
        re.mouse_buttons = UI_MOUSE_BTN_RIGHT;
        queue_push(&re);
    }

    /* Scroll wheel (coalesced with pending scroll events at queue tail) */
    if (scroll != 0) {
        ui_event_t se;
        memset(&se, 0, sizeof(se));
        se.source = UI_INPUT_MOUSE;
        se.type = UI_EVENT_MOVE; /* Scroll is a MOVE variant with scroll_delta */
        se.pos = mp->pos;
        se.scroll_delta = scroll;
        se.mouse_buttons = buttons;
        queue_push_scroll(&se);
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
                e.key_code = ui_key;  /* UI_KEY_NONE for unmapped keys (avoid HID/UI_KEY collision) */
                e.key_modifiers = modifiers;
                e.char_code = hid_to_ascii(released_hid, modifiers);
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
                            e.key_code = ui_key;  /* avoid HID/UI_KEY collision */
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
            e.key_code = ui_key;  /* UI_KEY_NONE for unmapped keys (avoid HID/UI_KEY collision) */
            e.key_modifiers = modifiers;
            e.char_code = hid_to_ascii(new_hid, modifiers);
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
            e.key_code = hid_to_ui_key(ks->key_code);  /* avoid HID/UI_KEY collision */
            e.char_code = hid_to_ascii(ks->key_code, s_state.key_modifiers);
            queue_push(&e);
        }
    } else {
        if (now - ks->last_long_time >= UI_LONG_PRESS_REPEAT_MS) {
            ks->last_long_time = now;

            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.type = UI_EVENT_KEY_LONG_REPEAT;
            e.source = source;
            e.key_code = hid_to_ui_key(ks->key_code);  /* avoid HID/UI_KEY collision */
            e.char_code = hid_to_ascii(ks->key_code, s_state.key_modifiers);
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
            e.key_code = hid_to_ui_key(ks->key_code);  /* avoid HID/UI_KEY collision */
            e.char_code = hid_to_ascii(ks->key_code, s_state.key_modifiers);
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

/*=============================================================================
 *  Stale Pointer Detection & Recovery
 *=============================================================================*/

/* Force-reset a pointer that has been "active" for too long without any new
 * raw input.  This recovers from lost UART packets (touch release missed) or
 * dropped queue events (UP event discarded on queue overflow). */
static void check_stale_pointers(void)
{
    uint32_t now = ui_get_real_ms();

    /* Check each touch point */
    for (int i = 0; i < UI_MAX_TOUCH_POINTS; i++) {
        ui_pointer_state_t *tp = &s_state.touches[i];
        if (tp->active && tp->last_feed_time > 0 &&
            (now - tp->last_feed_time) > UI_POINTER_STALE_TIMEOUT_MS) {

            /* Synthesize UP event so widgets can clean up */
            ui_event_t e;
            memset(&e, 0, sizeof(e));
            e.source = UI_INPUT_TOUCH;
            e.touch_id = (uint8_t)i;
            e.type = UI_EVENT_UP;
            e.pos = tp->pos;
            queue_push(&e);

            tp->active = false;

            /* Release capture if this stale touch holds it */
            if (s_state.capture_widget && s_state.capture_touch_id == (uint8_t)i) {
                s_state.capture_widget = NULL;
                s_state.capture_touch_id = UI_TOUCH_ID_NONE;
            }
        }
    }

    /* Check mouse */
    {
        ui_pointer_state_t *mp = &s_state.mouse;
        if (mp->active && mp->last_feed_time > 0 &&
            (now - mp->last_feed_time) > UI_POINTER_STALE_TIMEOUT_MS) {

            /* If mouse buttons are reported as 0 but active is still true,
             * force-release.  This handles the case where the UP event was
             * silently dropped from the queue. */
            if (!(s_state.mouse_buttons & UI_MOUSE_BTN_LEFT)) {
                ui_event_t e;
                memset(&e, 0, sizeof(e));
                e.source = UI_INPUT_MOUSE;
                e.touch_id = UI_TOUCH_ID_NONE;
                e.type = UI_EVENT_UP;
                e.pos = mp->pos;
                e.mouse_buttons = s_state.mouse_buttons;
                queue_push(&e);

                mp->active = false;

                if (s_state.capture_widget &&
                    s_state.capture_touch_id == UI_TOUCH_ID_NONE) {
                    s_state.capture_widget = NULL;
                }
            }
        }
    }
}

void ui_input_tick(void)
{
    /* Detect and recover from stale pointers (lost UP events) */
    check_stale_pointers();

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
        s_cursor_rendered_pos.x = -1;  /* Force invalidate at new position */
        s_cursor_rendered_pos.y = -1;
    } else {
        /* Mouse disconnected: invalidate old cursor position to erase it */
        if (s_cursor_rendered_pos.x >= 0) {
            ui_rect_t r;
            r.x = s_cursor_rendered_pos.x - 1;
            r.y = s_cursor_rendered_pos.y - 1;
            r.w = 10;
            r.h = 13;
            ui_page_invalidate(&r);
        }
        s_cursor_rendered_pos.x = -1;
        s_cursor_rendered_pos.y = -1;
    }
}

void ui_input_invalidate_cursor(void)
{
    if (!s_ext_mouse_connected) return;

    /* Mouse cursor size: 8x11 pixels + 1px outline = 10x13 dirty region */
    #define CURSOR_W  10
    #define CURSOR_H  13

    ui_rect_t r;
    r.w = CURSOR_W;
    r.h = CURSOR_H;

    /* Invalidate the position where cursor was last rendered (to erase it) */
    if (s_cursor_rendered_pos.x >= 0 &&
        (s_cursor_rendered_pos.x != s_state.mouse.pos.x ||
         s_cursor_rendered_pos.y != s_state.mouse.pos.y)) {
        r.x = s_cursor_rendered_pos.x - 1;
        r.y = s_cursor_rendered_pos.y - 1;
        ui_page_invalidate(&r);
    }

    /* Invalidate the current cursor position (to draw it) */
    r.x = s_state.mouse.pos.x - 1;
    r.y = s_state.mouse.pos.y - 1;
    ui_page_invalidate(&r);
}

/* Called after rendering to update the tracked rendered position */
void ui_input_cursor_rendered(void)
{
    if (s_ext_mouse_connected) {
        s_cursor_rendered_pos = s_state.mouse.pos;
    }
}
