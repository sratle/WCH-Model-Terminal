/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_widget.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI widget system for E-ink B/W display.
*                      All widgets render in 1bpp (black on white).
********************************************************************************/
#include "miniui_widget.h"
#include "miniui_render.h"
#include "miniui_input.h"
#include "miniui_page.h"
#include "debug.h"
#include <string.h>

/*=============================================================================
 *  Widget Base
 *=============================================================================*/

void ui_widget_init(ui_widget_t *w, const ui_rect_t *rect)
{
    if (!w) return;
    memset(w, 0, sizeof(ui_widget_t));
    w->rect = *rect;
    w->prev_rect = *rect;
    w->flags = UI_WIDGET_FLAG_VISIBLE | UI_WIDGET_FLAG_ENABLED;
    w->bg_color = UI_COLOR_WHITE;
}

void ui_widget_set_visible(ui_widget_t *w, bool visible)
{
    if (!w) return;
    if (visible) w->flags |= UI_WIDGET_FLAG_VISIBLE;
    else w->flags &= ~UI_WIDGET_FLAG_VISIBLE;
}

void ui_widget_set_enabled(ui_widget_t *w, bool enabled)
{
    if (!w) return;
    if (enabled) w->flags |= UI_WIDGET_FLAG_ENABLED;
    else w->flags &= ~UI_WIDGET_FLAG_ENABLED;
}

void ui_widget_invalidate(ui_widget_t *w)
{
    if (!w) return;
    ui_page_invalidate(&w->rect);
    if (w->prev_rect.x != w->rect.x || w->prev_rect.y != w->rect.y ||
        w->prev_rect.w != w->rect.w || w->prev_rect.h != w->rect.h) {
        ui_page_invalidate(&w->prev_rect);
    }
}

void ui_widget_draw(ui_widget_t *w, ui_rect_t *dirty)
{
    if (!w || !(w->flags & UI_WIDGET_FLAG_VISIBLE)) return;
    if (w->draw_cb) w->draw_cb(w, dirty);
}

void ui_widget_event(ui_widget_t *w, ui_event_t *e)
{
    if (!w || !(w->flags & UI_WIDGET_FLAG_ENABLED)) return;
    if (w->event_cb) w->event_cb(w, e);
}

bool ui_widget_hit_test(ui_widget_t *w, int16_t x, int16_t y)
{
    if (!w) return false;
    return (x >= w->rect.x && x < w->rect.x + w->rect.w &&
            y >= w->rect.y && y < w->rect.y + w->rect.h);
}

void ui_widget_move(ui_widget_t *w, int16_t x, int16_t y)
{
    if (!w) return;
    w->prev_rect = w->rect;
    w->rect.x = x;
    w->rect.y = y;
}

void ui_widget_resize(ui_widget_t *w, int16_t width, int16_t height)
{
    if (!w) return;
    w->prev_rect = w->rect;
    w->rect.w = width;
    w->rect.h = height;
}

void ui_widget_set_rect(ui_widget_t *w, const ui_rect_t *rect)
{
    if (!w || !rect) return;
    w->prev_rect = w->rect;
    w->rect = *rect;
}

/*=============================================================================
 *  Label Widget
 *=============================================================================*/

static void label_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_label_t *label = (ui_label_t *)w;
    if (!label->text || !label->font) return;

    ui_render_push_target(dirty);
    ui_draw_text_in_rect(&w->rect, label->text, label->font, label->text_color, label->align);
    ui_render_pop_target();
}

void ui_label_init(ui_label_t *label, const ui_rect_t *rect, const char *text, const ui_font_t *font)
{
    if (!label) return;
    ui_widget_init(&label->base, rect);
    label->base.draw_cb = label_draw_cb;
    label->text = text;
    label->font = font;
    label->text_color = UI_COLOR_BLACK;
    label->align = 0; /* left */
}

void ui_label_set_text(ui_label_t *label, const char *text)
{
    if (!label) return;
    label->text = text;
    ui_widget_invalidate(&label->base);
}

void ui_label_set_color(ui_label_t *label, ui_color_t color)
{
    if (!label) return;
    label->text_color = color;
    ui_widget_invalidate(&label->base);
}

void ui_label_set_align(ui_label_t *label, uint8_t align)
{
    if (!label) return;
    label->align = align;
    ui_widget_invalidate(&label->base);
}

/*=============================================================================
 *  Button Widget
 *=============================================================================*/

static void button_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_button_t *btn = (ui_button_t *)w;
    bool pressed = (w->flags & UI_WIDGET_FLAG_PRESSED) != 0;
    ui_color_t bg = pressed ? btn->bg_color_pressed : btn->bg_color_normal;

    ui_render_push_target(dirty);

    if (btn->radius > 0) {
        ui_draw_round_rect(&w->rect, btn->radius, bg, UI_COLOR_BLACK, 1);
    } else {
        ui_draw_rect(&w->rect, bg, UI_COLOR_BLACK, 1);
    }

    if (btn->text && btn->font) {
        ui_color_t text_c = btn->text_color;
        ui_draw_text_in_rect_bg(&w->rect, btn->text, btn->font, text_c, bg, 1);
    }

    ui_render_pop_target();
}

static void button_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_button_t *btn = (ui_button_t *)w;
    if (e->type == UI_EVENT_TOUCH_DOWN) {
        w->flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
    } else if (e->type == UI_EVENT_TOUCH_UP) {
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
        if (btn->on_click && ui_widget_hit_test(w, e->touch.x, e->touch.y)) {
            btn->on_click(w);
        }
    } else if (e->type == UI_EVENT_TOUCH_CANCEL) {
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
    }
}

void ui_button_init(ui_button_t *btn, const ui_rect_t *rect, const char *text, const ui_font_t *font)
{
    if (!btn) return;
    ui_widget_init(&btn->base, rect);
    btn->base.draw_cb = button_draw_cb;
    btn->base.event_cb = button_event_cb;
    btn->text = text;
    btn->font = font;
    btn->text_color = UI_COLOR_WHITE;  /* Black bg → white text */
    btn->bg_color_normal = UI_COLOR_BLACK;
    btn->bg_color_pressed = UI_COLOR_WHITE;
    btn->text_color = UI_COLOR_BLACK;  /* On white bg, text is black */
    btn->radius = 4;
}

void ui_button_set_colors(ui_button_t *btn, ui_color_t normal, ui_color_t pressed, ui_color_t text)
{
    if (!btn) return;
    btn->bg_color_normal = normal;
    btn->bg_color_pressed = pressed;
    btn->text_color = text;
    ui_widget_invalidate(&btn->base);
}

void ui_button_set_radius(ui_button_t *btn, int16_t radius)
{
    if (!btn) return;
    btn->radius = radius;
}

void ui_button_set_callback(ui_button_t *btn, void (*on_click)(ui_widget_t *w))
{
    if (!btn) return;
    btn->on_click = on_click;
}

/*=============================================================================
 *  Icon Button Widget
 *=============================================================================*/

static void icon_button_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_icon_button_t *btn = (ui_icon_button_t *)w;
    bool pressed = (w->flags & UI_WIDGET_FLAG_PRESSED) != 0;
    ui_color_t bg = pressed ? btn->bg_color_pressed : btn->bg_color_normal;

    ui_render_push_target(dirty);

    if (btn->radius > 0) {
        ui_draw_round_rect(&w->rect, btn->radius, bg, UI_COLOR_BLACK, 1);
    } else {
        ui_draw_rect(&w->rect, bg, UI_COLOR_BLACK, 1);
    }

    /* Draw icon */
    if (btn->icon_bitmap) {
        ui_draw_icon_in_rect(&w->rect, btn->icon_bitmap, btn->icon_w, btn->icon_h, btn->icon_color);
    }

    /* Draw text below icon */
    if (btn->text && btn->font) {
        ui_rect_t text_rect = w->rect;
        if (btn->icon_bitmap) {
            text_rect.y += btn->icon_h + 2;
            text_rect.h -= btn->icon_h + 2;
        }
        ui_draw_text_in_rect_bg(&text_rect, btn->text, btn->font, btn->text_color, bg, 1);
    }

    ui_render_pop_target();
}

static void icon_button_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_icon_button_t *btn = (ui_icon_button_t *)w;
    if (e->type == UI_EVENT_TOUCH_DOWN) {
        w->flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
    } else if (e->type == UI_EVENT_TOUCH_UP) {
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
        if (btn->on_click && ui_widget_hit_test(w, e->touch.x, e->touch.y)) {
            btn->on_click(w);
        }
    } else if (e->type == UI_EVENT_TOUCH_CANCEL) {
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
    }
}

void ui_icon_button_init(ui_icon_button_t *btn, const ui_rect_t *rect,
                         const uint8_t *bitmap, int16_t bw, int16_t bh,
                         const char *text, const ui_font_t *font)
{
    if (!btn) return;
    ui_widget_init(&btn->base, rect);
    btn->base.draw_cb = icon_button_draw_cb;
    btn->base.event_cb = icon_button_event_cb;
    btn->icon_bitmap = bitmap;
    btn->icon_w = bw;
    btn->icon_h = bh;
    btn->text = text;
    btn->font = font;
    btn->icon_color = UI_COLOR_BLACK;
    btn->text_color = UI_COLOR_BLACK;
    btn->bg_color_normal = UI_COLOR_WHITE;
    btn->bg_color_pressed = UI_COLOR_BLACK;
    btn->radius = 4;
}

void ui_icon_button_set_callback(ui_icon_button_t *btn, void (*on_click)(ui_widget_t *w))
{
    if (!btn) return;
    btn->on_click = on_click;
}

/*=============================================================================
 *  Slider Widget
 *=============================================================================*/

static void slider_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_slider_t *slider = (ui_slider_t *)w;
    int16_t track_h = 4;
    int16_t track_y = w->rect.y + (w->rect.h - track_h) / 2;
    int16_t knob_r = 6;
    int16_t range = slider->max - slider->min;
    int16_t fill_w = (range > 0) ? (int16_t)((int32_t)(slider->value - slider->min) * w->rect.w / range) : 0;
    if (fill_w > w->rect.w) fill_w = w->rect.w;

    ui_render_push_target(dirty);

    /* Track background */
    ui_rect_t track = { w->rect.x, track_y, w->rect.w, track_h };
    ui_draw_fill_rect(&track, slider->track_color);

    /* Fill */
    ui_rect_t fill = { w->rect.x, track_y, fill_w, track_h };
    ui_draw_fill_rect(&fill, slider->fill_color);

    /* Knob */
    int16_t knob_x = w->rect.x + fill_w;
    int16_t knob_y = w->rect.y + w->rect.h / 2;
    ui_draw_fill_circle(knob_x, knob_y, knob_r, slider->knob_color);
    ui_draw_circle_border(knob_x, knob_y, knob_r, UI_COLOR_BLACK, 1);

    ui_render_pop_target();
}

static void slider_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_slider_t *slider = (ui_slider_t *)w;
    if (e->type == UI_EVENT_TOUCH_DOWN || e->type == UI_EVENT_TOUCH_MOVE) {
        slider->dragging = true;
        int16_t rel_x = e->touch.x - w->rect.x;
        if (rel_x < 0) rel_x = 0;
        if (rel_x > w->rect.w) rel_x = w->rect.w;
        int16_t range = slider->max - slider->min;
        slider->value = slider->min + (int16_t)((int32_t)rel_x * range / w->rect.w);
        ui_widget_invalidate(w);
        if (slider->on_change) slider->on_change(w, slider->value);
    } else if (e->type == UI_EVENT_TOUCH_UP || e->type == UI_EVENT_TOUCH_CANCEL) {
        slider->dragging = false;
    }
}

void ui_slider_init(ui_slider_t *slider, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value)
{
    if (!slider) return;
    ui_widget_init(&slider->base, rect);
    slider->base.draw_cb = slider_draw_cb;
    slider->base.event_cb = slider_event_cb;
    slider->min = min;
    slider->max = max;
    slider->value = value;
    slider->track_color = UI_COLOR_WHITE;
    slider->fill_color = UI_COLOR_BLACK;
    slider->knob_color = UI_COLOR_WHITE;
    slider->dragging = false;
}

void ui_slider_set_value(ui_slider_t *slider, int16_t value)
{
    if (!slider) return;
    slider->value = value;
    ui_widget_invalidate(&slider->base);
}

void ui_slider_set_callback(ui_slider_t *slider, void (*on_change)(ui_widget_t *w, int16_t value))
{
    if (!slider) return;
    slider->on_change = on_change;
}

/*=============================================================================
 *  Switch Widget
 *=============================================================================*/

static void switch_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_switch_t *sw = (ui_switch_t *)w;
    int16_t track_r = w->rect.h / 2;
    int16_t knob_r = track_r - 2;
    ui_color_t track_c = sw->state ? sw->track_on_color : sw->track_off_color;

    ui_render_push_target(dirty);

    /* Track */
    ui_draw_fill_round_rect(&w->rect, track_r, track_c);
    ui_draw_round_rect_border(&w->rect, track_r, UI_COLOR_BLACK, 1);

    /* Knob */
    int16_t knob_x = sw->state ? (w->rect.x + w->rect.w - track_r) : (w->rect.x + track_r);
    int16_t knob_y = w->rect.y + track_r;
    ui_draw_fill_circle(knob_x, knob_y, knob_r, sw->knob_color);
    ui_draw_circle_border(knob_x, knob_y, knob_r, UI_COLOR_BLACK, 1);

    ui_render_pop_target();
}

static void switch_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_switch_t *sw = (ui_switch_t *)w;
    if (e->type == UI_EVENT_TOUCH_UP) {
        if (ui_widget_hit_test(w, e->touch.x, e->touch.y)) {
            sw->state = !sw->state;
            ui_widget_invalidate(w);
            if (sw->on_toggle) sw->on_toggle(w, sw->state);
        }
    }
}

void ui_switch_init(ui_switch_t *sw, const ui_rect_t *rect, bool state)
{
    if (!sw) return;
    ui_widget_init(&sw->base, rect);
    sw->base.draw_cb = switch_draw_cb;
    sw->base.event_cb = switch_event_cb;
    sw->state = state;
    sw->track_on_color = UI_COLOR_BLACK;
    sw->track_off_color = UI_COLOR_WHITE;
    sw->knob_color = UI_COLOR_WHITE;
}

void ui_switch_set_state(ui_switch_t *sw, bool state)
{
    if (!sw) return;
    sw->state = state;
    ui_widget_invalidate(&sw->base);
}

void ui_switch_set_callback(ui_switch_t *sw, void (*on_toggle)(ui_widget_t *w, bool state))
{
    if (!sw) return;
    sw->on_toggle = on_toggle;
}

/*=============================================================================
 *  Progress Bar Widget
 *=============================================================================*/

static void progress_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_progress_t *prog = (ui_progress_t *)w;
    int16_t range = prog->max - prog->min;
    int16_t fill_w = (range > 0) ? (int16_t)((int32_t)(prog->value - prog->min) * w->rect.w / range) : 0;
    if (fill_w > w->rect.w) fill_w = w->rect.w;

    ui_render_push_target(dirty);

    /* Track */
    ui_draw_fill_rect(&w->rect, prog->track_color);
    ui_draw_rect_border(&w->rect, UI_COLOR_BLACK, 1);

    /* Fill */
    if (fill_w > 0) {
        ui_rect_t fill = { w->rect.x + 1, w->rect.y + 1, fill_w - 2, w->rect.h - 2 };
        if (fill.w > 0 && fill.h > 0) {
            ui_draw_fill_rect(&fill, prog->fill_color);
        }
    }

    ui_render_pop_target();
}

void ui_progress_init(ui_progress_t *prog, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value)
{
    if (!prog) return;
    ui_widget_init(&prog->base, rect);
    prog->base.draw_cb = progress_draw_cb;
    prog->min = min;
    prog->max = max;
    prog->value = value;
    prog->track_color = UI_COLOR_WHITE;
    prog->fill_color = UI_COLOR_BLACK;
    prog->show_text = false;
}

void ui_progress_set_value(ui_progress_t *prog, int16_t value)
{
    if (!prog) return;
    prog->value = value;
    ui_widget_invalidate(&prog->base);
}

/*=============================================================================
 *  List Item Widget
 *=============================================================================*/

static void list_item_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_list_item_t *item = (ui_list_item_t *)w;

    ui_render_push_target(dirty);

    /* Background */
    ui_draw_fill_rect(&w->rect, UI_COLOR_WHITE);

    /* Title */
    if (item->title && item->font) {
        ui_rect_t text_rect = { w->rect.x + 8, w->rect.y, w->rect.w - 16, w->rect.h };
        if (item->control) {
            text_rect.w -= 60;  /* Space for control widget */
        }
        ui_draw_text_in_rect(&text_rect, item->title, item->font, item->text_color, 0);
    }

    /* Control widget */
    if (item->control && (item->control->flags & UI_WIDGET_FLAG_VISIBLE)) {
        ui_widget_draw(item->control, dirty);
    }

    /* Divider */
    if (item->show_divider) {
        ui_rect_t div = { w->rect.x + 8, w->rect.y + w->rect.h - 1, w->rect.w - 16, 1 };
        ui_draw_fill_rect(&div, item->divider_color);
    }

    ui_render_pop_target();
}

static void list_item_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_list_item_t *item = (ui_list_item_t *)w;

    if (e->type == UI_EVENT_TOUCH_DOWN) {
        w->flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
    } else if (e->type == UI_EVENT_TOUCH_UP) {
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);

        /* Forward event to control if active */
        if (item->control && item->control_active &&
            ui_widget_hit_test(item->control, e->touch.x, e->touch.y)) {
            ui_widget_event(item->control, e);
        }
    } else if (e->type == UI_EVENT_TOUCH_MOVE) {
        if (item->control && item->control_active) {
            ui_widget_event(item->control, e);
        }
    } else if (e->type == UI_EVENT_TOUCH_CANCEL) {
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
    }
}

void ui_list_item_init(ui_list_item_t *item, const ui_rect_t *rect, const char *title, const ui_font_t *font)
{
    if (!item) return;
    ui_widget_init(&item->base, rect);
    item->base.draw_cb = list_item_draw_cb;
    item->base.event_cb = list_item_event_cb;
    item->title = title;
    item->font = font;
    item->text_color = UI_COLOR_BLACK;
    item->control = NULL;
    item->show_divider = true;
    item->divider_color = UI_COLOR_BLACK;
    item->control_active = false;
}

void ui_list_item_set_control(ui_list_item_t *item, ui_widget_t *control)
{
    if (!item) return;
    item->control = control;
    item->control_active = (control != NULL);
}
