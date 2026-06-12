/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_widget.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI widget system API for E-ink display.
*                      Adapted from Display-1 for 1bpp B/W rendering.
********************************************************************************/
#ifndef __MINIUI_WIDGET_H
#define __MINIUI_WIDGET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui.h"

/*=============================================================================
 *  Widget Base Structure
 *=============================================================================*/

struct ui_widget {
    ui_rect_t rect;
    ui_rect_t prev_rect;
    uint16_t flags;
    ui_color_t bg_color;
    ui_draw_cb_t draw_cb;
    ui_event_cb_t event_cb;
    void *user_data;
};

/*=============================================================================
 *  Widget Management
 *=============================================================================*/

void ui_widget_init(ui_widget_t *w, const ui_rect_t *rect);
void ui_widget_set_visible(ui_widget_t *w, bool visible);
void ui_widget_set_enabled(ui_widget_t *w, bool enabled);
void ui_widget_invalidate(ui_widget_t *w);
void ui_widget_draw(ui_widget_t *w, ui_rect_t *dirty);
void ui_widget_event(ui_widget_t *w, ui_event_t *e);
bool ui_widget_hit_test(ui_widget_t *w, int16_t x, int16_t y);
void ui_widget_move(ui_widget_t *w, int16_t x, int16_t y);
void ui_widget_resize(ui_widget_t *w, int16_t width, int16_t height);
void ui_widget_set_rect(ui_widget_t *w, const ui_rect_t *rect);

/*=============================================================================
 *  Label Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    const char *text;
    const ui_font_t *font;
    ui_color_t text_color;
    uint8_t align;
} ui_label_t;

void ui_label_init(ui_label_t *label, const ui_rect_t *rect, const char *text, const ui_font_t *font);
void ui_label_set_text(ui_label_t *label, const char *text);
void ui_label_set_color(ui_label_t *label, ui_color_t color);
void ui_label_set_align(ui_label_t *label, uint8_t align);

/*=============================================================================
 *  Button Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    const char *text;
    const ui_font_t *font;
    ui_color_t text_color;
    ui_color_t bg_color_normal;
    ui_color_t bg_color_pressed;
    int16_t radius;
    void (*on_click)(ui_widget_t *w);
} ui_button_t;

void ui_button_init(ui_button_t *btn, const ui_rect_t *rect, const char *text, const ui_font_t *font);
void ui_button_set_colors(ui_button_t *btn, ui_color_t normal, ui_color_t pressed, ui_color_t text);
void ui_button_set_radius(ui_button_t *btn, int16_t radius);
void ui_button_set_callback(ui_button_t *btn, void (*on_click)(ui_widget_t *w));

/*=============================================================================
 *  Icon Button Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    const uint8_t *icon_bitmap;
    int16_t icon_w;
    int16_t icon_h;
    const char *text;
    const ui_font_t *font;
    ui_color_t icon_color;
    ui_color_t text_color;
    ui_color_t bg_color_normal;
    ui_color_t bg_color_pressed;
    int16_t radius;
    void (*on_click)(ui_widget_t *w);
} ui_icon_button_t;

void ui_icon_button_init(ui_icon_button_t *btn, const ui_rect_t *rect,
                         const uint8_t *bitmap, int16_t bw, int16_t bh,
                         const char *text, const ui_font_t *font);
void ui_icon_button_set_callback(ui_icon_button_t *btn, void (*on_click)(ui_widget_t *w));

/*=============================================================================
 *  Slider Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    int16_t min;
    int16_t max;
    int16_t value;
    ui_color_t track_color;
    ui_color_t fill_color;
    ui_color_t knob_color;
    bool dragging;
    void (*on_change)(ui_widget_t *w, int16_t value);
} ui_slider_t;

void ui_slider_init(ui_slider_t *slider, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value);
void ui_slider_set_value(ui_slider_t *slider, int16_t value);
void ui_slider_set_callback(ui_slider_t *slider, void (*on_change)(ui_widget_t *w, int16_t value));

/*=============================================================================
 *  Switch Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    bool state;
    ui_color_t track_on_color;
    ui_color_t track_off_color;
    ui_color_t knob_color;
    void (*on_toggle)(ui_widget_t *w, bool state);
} ui_switch_t;

void ui_switch_init(ui_switch_t *sw, const ui_rect_t *rect, bool state);
void ui_switch_set_state(ui_switch_t *sw, bool state);
void ui_switch_set_callback(ui_switch_t *sw, void (*on_toggle)(ui_widget_t *w, bool state));

/*=============================================================================
 *  Progress Bar Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    int16_t min;
    int16_t max;
    int16_t value;
    ui_color_t track_color;
    ui_color_t fill_color;
    bool show_text;
} ui_progress_t;

void ui_progress_init(ui_progress_t *prog, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value);
void ui_progress_set_value(ui_progress_t *prog, int16_t value);

/*=============================================================================
 *  List Item Widget
 *=============================================================================*/

typedef struct {
    ui_widget_t base;
    const char *title;
    const ui_font_t *font;
    ui_color_t text_color;
    ui_widget_t *control;
    bool show_divider;
    ui_color_t divider_color;
    bool control_active;
} ui_list_item_t;

void ui_list_item_init(ui_list_item_t *item, const ui_rect_t *rect, const char *title, const ui_font_t *font);
void ui_list_item_set_control(ui_list_item_t *item, ui_widget_t *control);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_WIDGET_H */
