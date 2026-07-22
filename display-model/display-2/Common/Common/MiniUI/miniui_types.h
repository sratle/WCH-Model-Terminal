/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_types.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI core type definitions for E-ink display.
*                      Adapted from Display-1 for 1bpp B/W e-paper.
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
 *  Color Type (1bpp for E-ink: 0 = white, 1 = black)
 *=============================================================================*/

typedef uint8_t ui_color_t;

/*=============================================================================
 *  Font Type (Bitmap Font, 1bpp only for E-ink)
 *=============================================================================*/

/* Glyph descriptor for a single character */
typedef struct {
    uint16_t unicode;         /* Unicode code point */
    uint8_t width;            /* Glyph width in pixels */
    uint8_t height;           /* Glyph height in pixels */
    int8_t x_offset;          /* X offset from cursor */
    int8_t y_offset;          /* Y offset from cursor (baseline) */
    uint8_t advance;          /* Cursor advance after this glyph */
    const uint8_t *bitmap;    /* Glyph bitmap data (1bpp) */
} ui_glyph_t;

/* Font descriptor */
typedef struct {
    const ui_glyph_t *glyphs; /* Array of glyph descriptors */
    uint16_t glyph_count;     /* Number of glyphs */
    uint8_t height;           /* Font height (max) */
    int8_t baseline;          /* Baseline offset from top */
    uint8_t bpp;              /* Bits per pixel: always 1 for E-ink */
} ui_font_t;

/*=============================================================================
 *  Input Event Types
 *=============================================================================*/

/* Input source */
typedef enum {
    UI_INPUT_TOUCH    = 0,
    UI_INPUT_MOUSE    = 1,
    UI_INPUT_KEYBOARD = 2,
    UI_INPUT_CORE_KEY = 3,
} ui_input_source_t;

/* Event types — unified across all input sources */
typedef enum {
    UI_EVENT_NONE         = 0,

    /* Touch / pointer events */
    UI_EVENT_TOUCH_DOWN   = 1,
    UI_EVENT_TOUCH_UP     = 2,
    UI_EVENT_TOUCH_MOVE   = 3,
    UI_EVENT_TOUCH_CANCEL = 4,

    /* Generic pointer events */
    UI_EVENT_DOWN,
    UI_EVENT_UP,
    UI_EVENT_MOVE,
    UI_EVENT_CLICK,
    UI_EVENT_DOUBLE_CLICK,
    UI_EVENT_LONG_PRESS,
    UI_EVENT_LONG_PRESS_REPEAT,
    UI_EVENT_SWIPE_UP,
    UI_EVENT_SWIPE_DOWN,
    UI_EVENT_SWIPE_LEFT,
    UI_EVENT_SWIPE_RIGHT,

    /* Key events */
    UI_EVENT_KEY_DOWN,
    UI_EVENT_KEY_UP,
    UI_EVENT_KEY_CLICK,
    UI_EVENT_KEY_DOUBLE_CLICK,
    UI_EVENT_KEY_LONG_PRESS,
    UI_EVENT_KEY_LONG_REPEAT,

    UI_EVENT_KEY_UP_ARROW,
    UI_EVENT_KEY_DOWN_ARROW,
    UI_EVENT_KEY_LEFT_ARROW,
    UI_EVENT_KEY_RIGHT_ARROW,
    UI_EVENT_KEY_OK,
    UI_EVENT_KEY_BACK,
    UI_EVENT_KEY_TAB,

    UI_EVENT_PRESS_CANCEL,
} ui_event_type_t;

/* Key codes */
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
    UI_KEY_CORE_PLUS  = 0x10,
    UI_KEY_CORE_MINUS = 0x11,
    UI_KEY_CORE_ENTER = 0x12,
} ui_key_code_t;

/* USB HID modifier key bitmask */
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
#define UI_MAX_KEYBOARD_KEYS 6   /* Max simultaneous non-modifier keys (USB HID) */

/* Touch event data sub-structure */
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t id;
} ui_touch_data_t;

/* Unified event structure */
typedef struct {
    ui_event_type_t type;
    ui_input_source_t source;
    union {
        ui_touch_data_t touch;
        struct {
            ui_point_t pos;
            ui_point_t delta;
        } mouse;
        struct {
            uint8_t code;
            uint8_t modifiers;
        } key;
    };
    uint8_t char_code;
    int8_t scroll_delta;
    /* Mouse button state bitmask (UI_MOUSE_BTN_*) — valid for
     * UI_INPUT_MOUSE sourced events (e.g. right-click context menu) */
    uint8_t mouse_buttons;
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
#define UI_WIDGET_FLAG_OPAQUE      (1 << 5)
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
typedef void (*ui_page_update_cb_t)(ui_page_t *page, uint32_t dt_ms);
typedef bool (*ui_page_event_cb_t)(ui_page_t *page, ui_event_t *e);
typedef void (*ui_anim_update_cb_t)(int32_t value, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_TYPES_H */
