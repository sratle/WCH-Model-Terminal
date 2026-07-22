/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_types.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI core type definitions.
*                      Color, geometry, font, and event types.
********************************************************************************/
#ifndef __MINIUI_TYPES_H
#define __MINIUI_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*=============================================================================
 *  Basic Geometry Types
 *=============================================================================*/

typedef struct {
    int16_t x;
    int16_t y;
} ui_point_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} ui_rect_t;

/*=============================================================================
 *  Color Type (RGB565)
 *=============================================================================*/

typedef uint16_t ui_color_t;

/*=============================================================================
 *  Font Type (Bitmap Font with Anti-aliasing Support)
 *=============================================================================*/

/* Glyph descriptor for a single character */
typedef struct {
    uint16_t unicode;         /* Unicode code point */
    uint8_t width;            /* Glyph width in pixels */
    uint8_t height;           /* Glyph height in pixels */
    int8_t x_offset;          /* X offset from cursor */
    int8_t y_offset;          /* Y offset from cursor (baseline) */
    uint8_t advance;          /* Cursor advance after this glyph */
    const uint8_t *bitmap;    /* Glyph bitmap data (1bpp/2bpp/4bpp) */
} ui_glyph_t;

/* Font descriptor */
typedef struct {
    const ui_glyph_t *glyphs; /* Array of glyph descriptors */
    uint16_t glyph_count;     /* Number of glyphs */
    uint8_t height;           /* Font height (max) */
    int8_t baseline;          /* Baseline offset from top */
    uint8_t bpp;              /* Bits per pixel: 1, 2, or 4 */
} ui_font_t;

/*=============================================================================
 *  Input Event Types
 *=============================================================================*/

/* Input source */
typedef enum {
    UI_INPUT_TOUCH    = 0,  /* Local capacitive touch screen */
    UI_INPUT_MOUSE    = 1,  /* Mouse from Core (CH9350 / BLE HID) */
    UI_INPUT_KEYBOARD = 2,  /* Keyboard from Core (CH9350 / BLE HID / Keyboard module) */
    UI_INPUT_CORE_KEY = 3,  /* Core onboard buttons (+/-/Enter) */
} ui_input_source_t;

/* Event types — unified across all input sources */
typedef enum {
    /* --- Touch / Mouse pointer events --- */
    UI_EVENT_NONE         = 0,
    UI_EVENT_DOWN,              /* Finger/mouse button pressed */
    UI_EVENT_UP,                /* Finger/mouse button released (after short press) */
    UI_EVENT_MOVE,              /* Finger/mouse moved while pressed */
    UI_EVENT_CLICK,             /* Single tap completed (down + up within time & distance) */
    UI_EVENT_DOUBLE_CLICK,      /* Two rapid clicks */
    UI_EVENT_LONG_PRESS,        /* Press & hold exceeded threshold (first trigger) */
    UI_EVENT_LONG_PRESS_REPEAT, /* Continued hold, repeats at interval */
    UI_EVENT_SWIPE_UP,          /* Swipe gesture: up */
    UI_EVENT_SWIPE_DOWN,        /* Swipe gesture: down */
    UI_EVENT_SWIPE_LEFT,        /* Swipe gesture: left */
    UI_EVENT_SWIPE_RIGHT,       /* Swipe gesture: right */

    /* --- Keyboard / Core-key events --- */
    UI_EVENT_KEY_DOWN,          /* Key pressed (physical or logical) */
    UI_EVENT_KEY_UP,            /* Key released */
    UI_EVENT_KEY_CLICK,         /* Key click (down + up within time) */
    UI_EVENT_KEY_DOUBLE_CLICK,  /* Key double click */
    UI_EVENT_KEY_LONG_PRESS,    /* Key long press (first trigger) */
    UI_EVENT_KEY_LONG_REPEAT,   /* Key long press repeat */

    /* --- Convenience logical key events (derived from keyboard/core-key) --- */
    UI_EVENT_KEY_UP_ARROW,      /* Up arrow / + button */
    UI_EVENT_KEY_DOWN_ARROW,    /* Down arrow / - button */
    UI_EVENT_KEY_LEFT_ARROW,    /* Left arrow */
    UI_EVENT_KEY_RIGHT_ARROW,   /* Right arrow */
    UI_EVENT_KEY_OK,            /* OK / Enter */
    UI_EVENT_KEY_BACK,          /* Back / Escape */
    UI_EVENT_KEY_TAB,           /* Tab - for page switching */

    /* --- Legacy compat --- */
    UI_EVENT_PRESS_CANCEL,      /* Press cancelled (e.g. swipe ended capture) */
} ui_event_type_t;

/* Key codes used in ui_event_t.key_code for UI_INPUT_KEYBOARD and UI_INPUT_CORE_KEY */
typedef enum {
    UI_KEY_NONE      = 0x00,
    UI_KEY_UP        = 0x01,
    UI_KEY_DOWN      = 0x02,
    UI_KEY_LEFT      = 0x03,
    UI_KEY_RIGHT     = 0x04,
    UI_KEY_OK        = 0x05,
    UI_KEY_BACK      = 0x06,
    UI_KEY_HOME      = 0x07,
    UI_KEY_MENU      = 0x08,
    UI_KEY_END       = 0x09,
    UI_KEY_TAB       = 0x0A,
    /* Core onboard buttons */
    UI_KEY_CORE_PLUS  = 0x10,
    UI_KEY_CORE_MINUS = 0x11,
    UI_KEY_CORE_ENTER = 0x12,
} ui_key_code_t;

/* USB HID modifier key bitmask (matches protocol) */
typedef enum {
    UI_MOD_LCTRL  = (1 << 0),
    UI_MOD_LSHIFT = (1 << 1),
    UI_MOD_LALT   = (1 << 2),
    UI_MOD_LGUI   = (1 << 3),
    UI_MOD_RCTRL  = (1 << 4),
    UI_MOD_RSHIFT = (1 << 5),
    UI_MOD_RALT   = (1 << 6),
    UI_MOD_RGUI   = (1 << 7),
} ui_key_modifier_t;

/* Mouse button bitmask */
typedef enum {
    UI_MOUSE_BTN_LEFT   = (1 << 0),
    UI_MOUSE_BTN_RIGHT  = (1 << 1),
    UI_MOUSE_BTN_MIDDLE = (1 << 2),
} ui_mouse_button_t;

#define UI_TOUCH_ID_NONE  0xFF
#define UI_MAX_TOUCH_POINTS 5

/* Unified event structure */
typedef struct {
    ui_event_type_t type;
    ui_input_source_t source;

    /* Position — valid for TOUCH and MOUSE events */
    ui_point_t pos;
    /* Delta — movement for MOVE/SWIPE events, scroll for mouse wheel */
    ui_point_t delta;

    /* Touch point ID for multi-touch (0 ~ UI_MAX_TOUCH_POINTS-1),
       UI_TOUCH_ID_NONE for non-touch events */
    uint8_t touch_id;

    /* Key code — valid for KEYBOARD and CORE_KEY events */
    uint8_t key_code;

    /* Modifier bitmask — valid for KEYBOARD events */
    uint8_t key_modifiers;

    /* Mouse button state bitmask — valid for MOUSE events */
    uint8_t mouse_buttons;

    /* Scroll wheel delta — valid for MOUSE events */
    int8_t scroll_delta;

    /* Printable ASCII character code (0 if not printable).
     * Populated by input system for KEYBOARD key-down/click events.
     * Accounts for shift modifier (e.g. 'a' -> 'A', '1' -> '!'). */
    uint8_t char_code;
} ui_event_t;

/*=============================================================================
 *  Widget Base Type (Forward Declaration)
 *=============================================================================*/

struct ui_widget;
typedef struct ui_widget ui_widget_t;

/* Widget flags */
#define UI_WIDGET_FLAG_VISIBLE     (1 << 0)
#define UI_WIDGET_FLAG_ENABLED     (1 << 1)
#define UI_WIDGET_FLAG_DIRTY       (1 << 2)
#define UI_WIDGET_FLAG_PRESSED     (1 << 3)
#define UI_WIDGET_FLAG_FOCUS       (1 << 4)
#define UI_WIDGET_FLAG_OPAQUE      (1 << 5)  /* Widget fully opaque, renderer can skip bg fill */
#define UI_WIDGET_FLAG_FOCUSABLE   (1 << 6)  /* Reachable by TAB keyboard focus traversal */

/*=============================================================================
 *  Page Type (Forward Declaration)
 *=============================================================================*/

struct ui_page;
typedef struct ui_page ui_page_t;

/* Page flags */
#define UI_PAGE_FLAG_FULLSCREEN    (1 << 0)
#define UI_PAGE_FLAG_HAS_BACK      (1 << 1)

/*=============================================================================
 *  Animation Type (Forward Declaration)
 *=============================================================================*/

struct ui_anim;
typedef struct ui_anim ui_anim_t;

/*=============================================================================
 *  Callback Types
 *=============================================================================*/

typedef void (*ui_draw_cb_t)(ui_widget_t *w, ui_rect_t *dirty);
typedef void (*ui_event_cb_t)(ui_widget_t *w, ui_event_t *e);
typedef void (*ui_page_enter_cb_t)(ui_page_t *page);
typedef void (*ui_page_exit_cb_t)(ui_page_t *page);
typedef void (*ui_page_draw_cb_t)(ui_page_t *page, ui_rect_t *dirty);
typedef void (*ui_page_back_cb_t)(ui_page_t *page);
typedef void (*ui_page_update_cb_t)(ui_page_t *page);  /* Per-frame logic update */
typedef bool (*ui_page_event_cb_t)(ui_page_t *page, ui_event_t *e);  /* Page-level event intercept */
typedef void (*ui_anim_update_cb_t)(int32_t value, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_TYPES_H */
