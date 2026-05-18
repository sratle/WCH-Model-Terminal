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
    UI_INPUT_TOUCH,
    UI_INPUT_MOUSE,
    UI_INPUT_KEYBOARD,
} ui_input_source_t;

/* Event types */
typedef enum {
    UI_EVENT_NONE,
    UI_EVENT_PRESS,
    UI_EVENT_RELEASE,
    UI_EVENT_DRAG,
    UI_EVENT_SWIPE_UP,
    UI_EVENT_SWIPE_DOWN,
    UI_EVENT_SWIPE_LEFT,
    UI_EVENT_SWIPE_RIGHT,
    UI_EVENT_KEY_UP,
    UI_EVENT_KEY_DOWN,
    UI_EVENT_KEY_LEFT,
    UI_EVENT_KEY_RIGHT,
    UI_EVENT_KEY_OK,
    UI_EVENT_KEY_BACK,
    UI_EVENT_PRESS_CANCEL,
} ui_event_type_t;

/* Event structure */
typedef struct {
    ui_event_type_t type;
    ui_input_source_t source;
    ui_point_t pos;
    ui_point_t delta;
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
typedef void (*ui_anim_update_cb_t)(int32_t value, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_TYPES_H */
