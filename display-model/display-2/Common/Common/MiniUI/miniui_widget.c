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
#include "font/font_montserrat_12.h"
#include "font/font_montserrat_16.h"
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
    } else if (e->type == UI_EVENT_KEY_OK) {
        /* Keyboard activation of the TAB-focused button (PRESSED = focus). */
        if ((w->flags & UI_WIDGET_FLAG_PRESSED) && btn->on_click) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
            btn->on_click(w);
        }
    }
}

void ui_button_init(ui_button_t *btn, const ui_rect_t *rect, const char *text, const ui_font_t *font)
{
    if (!btn) return;
    ui_widget_init(&btn->base, rect);
    btn->base.draw_cb = button_draw_cb;
    btn->base.event_cb = button_event_cb;
    btn->base.flags |= UI_WIDGET_FLAG_FOCUSABLE;  /* reachable by TAB */
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

    /* Split button into icon area (top) and text area (bottom) so they
     * never overlap. Previously the icon was centered in the full button
     * rect, which collided with the text drawn below it. */
    int16_t text_h    = (btn->text && btn->font) ? (btn->font->height + 6) : 0;
    int16_t icon_area_h = w->rect.h - text_h;

    /* Draw icon centered in the upper area */
    if (btn->icon_bitmap && icon_area_h > 0) {
        ui_rect_t icon_rect = { w->rect.x, w->rect.y, w->rect.w, icon_area_h };
        ui_draw_icon_in_rect(&icon_rect, btn->icon_bitmap,
                             btn->icon_w, btn->icon_h, btn->icon_color);
    }

    /* Draw text centered in the lower area */
    if (btn->text && btn->font && text_h > 0) {
        ui_rect_t text_rect = { w->rect.x, w->rect.y + icon_area_h,
                                w->rect.w, text_h };
        ui_draw_text_in_rect_bg(&text_rect, btn->text, btn->font,
                                btn->text_color, bg, 1);
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
    } else if (e->type == UI_EVENT_KEY_OK) {
        /* Keyboard activation of the TAB-focused icon button (PRESSED = focus). */
        if ((w->flags & UI_WIDGET_FLAG_PRESSED) && btn->on_click) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
            btn->on_click(w);
        }
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
    btn->base.flags |= UI_WIDGET_FLAG_FOCUSABLE;  /* reachable by TAB */
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

    /* Keyboard focus outline (set by TAB traversal via PRESSED). */
    if (w->flags & UI_WIDGET_FLAG_PRESSED) {
        ui_draw_rect_border(&w->rect, UI_COLOR_BLACK, 2);
    }

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
    } else if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        /* Mouse wheel over the slider nudges its value (scroll up = increase).
         * The step scales with the range so wide sliders still move at a
         * usable pace, while narrow ones (e.g. font size) move by 1 per notch. */
        int16_t range = slider->max - slider->min;
        int16_t step = range / 20;
        if (step < 1) step = 1;
        int32_t nv = (int32_t)slider->value + (int32_t)e->scroll_delta * step;
        if (nv < slider->min) nv = slider->min;
        if (nv > slider->max) nv = slider->max;
        if ((int16_t)nv != slider->value) {
            slider->value = (int16_t)nv;
            ui_widget_invalidate(w);
            if (slider->on_change) slider->on_change(w, slider->value);
        }
    } else if ((e->type == UI_EVENT_KEY_LEFT_ARROW || e->type == UI_EVENT_KEY_RIGHT_ARROW) &&
               (w->flags & UI_WIDGET_FLAG_PRESSED)) {
        /* Arrow keys adjust the value while the slider is TAB-focused. */
        int16_t range = slider->max - slider->min;
        int16_t step = range / 20;
        if (step < 1) step = 1;
        int32_t av = (int32_t)slider->value +
                     ((e->type == UI_EVENT_KEY_RIGHT_ARROW) ? step : -step);
        if (av < slider->min) av = slider->min;
        if (av > slider->max) av = slider->max;
        if ((int16_t)av != slider->value) {
            slider->value = (int16_t)av;
            ui_widget_invalidate(w);
            if (slider->on_change) slider->on_change(w, slider->value);
        }
    }
}

void ui_slider_init(ui_slider_t *slider, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value)
{
    if (!slider) return;
    ui_widget_init(&slider->base, rect);
    slider->base.draw_cb = slider_draw_cb;
    slider->base.event_cb = slider_event_cb;
    slider->base.flags |= UI_WIDGET_FLAG_FOCUSABLE;  /* reachable by TAB */
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

    /* Keyboard focus outline (set by TAB traversal via PRESSED). */
    if (w->flags & UI_WIDGET_FLAG_PRESSED) {
        ui_draw_rect_border(&w->rect, UI_COLOR_BLACK, 2);
    }

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
    } else if (e->type == UI_EVENT_KEY_OK && (w->flags & UI_WIDGET_FLAG_PRESSED)) {
        /* Keyboard toggle of the TAB-focused switch. */
        sw->state = !sw->state;
        ui_widget_invalidate(w);
        if (sw->on_toggle) sw->on_toggle(w, sw->state);
    }
}

void ui_switch_init(ui_switch_t *sw, const ui_rect_t *rect, bool state)
{
    if (!sw) return;
    ui_widget_init(&sw->base, rect);
    sw->base.draw_cb = switch_draw_cb;
    sw->base.event_cb = switch_event_cb;
    sw->base.flags |= UI_WIDGET_FLAG_FOCUSABLE;  /* reachable by TAB */
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
    ui_widget_invalidate(&item->base);
}

/*=============================================================================
 *  Card Widget Implementation
 *=============================================================================*/

static void card_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_card_t *card = (ui_card_t *)w;

    if (card->radius > 0) {
        ui_draw_fill_round_rect(&w->rect, card->radius, w->bg_color);
        if (card->border_width > 0) {
            ui_draw_round_rect_border(&w->rect, card->radius, card->border_color, card->border_width);
        }
    } else {
        ui_draw_fill_rect(&w->rect, w->bg_color);
        if (card->border_width > 0) {
            ui_draw_rect_border(&w->rect, card->border_color, card->border_width);
        }
    }

    for (uint16_t i = 0; i < card->child_count; i++) {
        if (card->children[i]) {
            ui_widget_draw(card->children[i], dirty);
        }
    }
}

static void card_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_card_t *card = (ui_card_t *)w;

    /* Map TOUCH_* events to pointer events for unified handling */
    ui_event_type_t et = e->type;
    if (et == UI_EVENT_TOUCH_DOWN) et = UI_EVENT_DOWN;
    else if (et == UI_EVENT_TOUCH_UP) et = UI_EVENT_UP;
    else if (et == UI_EVENT_TOUCH_MOVE) et = UI_EVENT_MOVE;
    else if (et == UI_EVENT_TOUCH_CANCEL) et = UI_EVENT_PRESS_CANCEL;

    switch (et) {
    case UI_EVENT_DOWN: {
        card->active_child = -1;
        for (uint16_t i = 0; i < card->child_count; i++) {
            if (card->children[i] && ui_widget_hit_test(card->children[i], e->touch.x, e->touch.y)) {
                card->active_child = (int16_t)i;
                ui_widget_event(card->children[i], e);
                break;
            }
        }
        break;
    }
    case UI_EVENT_MOVE:
        if (card->active_child >= 0 && card->active_child < (int16_t)card->child_count) {
            ui_widget_event(card->children[card->active_child], e);
        }
        break;
    case UI_EVENT_UP:
        if (card->active_child >= 0 && card->active_child < (int16_t)card->child_count) {
            ui_widget_event(card->children[card->active_child], e);
        }
        break;
    case UI_EVENT_CLICK:
    case UI_EVENT_DOUBLE_CLICK:
    case UI_EVENT_LONG_PRESS:
        if (card->active_child >= 0 && card->active_child < (int16_t)card->child_count) {
            ui_widget_event(card->children[card->active_child], e);
        }
        card->active_child = -1;
        break;
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
    case UI_EVENT_PRESS_CANCEL:
        if (card->active_child >= 0 && card->active_child < (int16_t)card->child_count) {
            ui_widget_event(card->children[card->active_child], e);
            card->active_child = -1;
        }
        break;
    default:
        for (uint16_t i = 0; i < card->child_count; i++) {
            if (card->children[i]) {
                ui_widget_event(card->children[i], e);
            }
        }
        break;
    }
}

void ui_card_init(ui_card_t *card, const ui_rect_t *rect)
{
    if (!card) return;
    ui_widget_init(&card->base, rect);
    card->base.draw_cb = card_draw_cb;
    card->base.event_cb = card_event_cb;
    card->base.bg_color = UI_COLOR_BG_CARD;
    card->radius = 8;
    card->border_color = UI_COLOR_BLACK;
    card->border_width = 0;
    card->child_count = 0;
    card->active_child = -1;
}

void ui_card_add_child(ui_card_t *card, ui_widget_t *child)
{
    if (!card || !child) return;
    if (card->child_count < UI_CARD_MAX_CHILDREN) {
        card->children[card->child_count++] = child;
    }
}

/*=============================================================================
 *  TabView Widget Implementation
 *=============================================================================*/

#define TAB_BAR_HEIGHT 36

static void tabview_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_tabview_t *tv = (ui_tabview_t *)w;

    ui_rect_t bar = { w->rect.x, w->rect.y, w->rect.w, TAB_BAR_HEIGHT };
    ui_draw_fill_rect(&bar, tv->tab_bg_color);

    int16_t tab_w = w->rect.w / tv->tab_count;

    for (uint8_t i = 0; i < tv->tab_count; i++) {
        ui_rect_t tab_rect = { w->rect.x + i * tab_w, w->rect.y, tab_w, TAB_BAR_HEIGHT };
        if (i == tv->active_tab) {
            ui_draw_fill_rect(&tab_rect, tv->tab_active_color);
            ui_rect_t ind = { tab_rect.x, tab_rect.y + TAB_BAR_HEIGHT - 3, tab_rect.w, 3 };
            ui_draw_fill_rect(&ind, tv->indicator_color);
        }
        if (tv->tab_labels[i]) {
            ui_color_t tc = (i == tv->active_tab) ? tv->tab_active_text_color : tv->tab_text_color;
            ui_draw_text_in_rect(&tab_rect, tv->tab_labels[i], &font_montserrat_12, tc, 1);
        }
    }
}

static void tabview_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_tabview_t *tv = (ui_tabview_t *)w;

    switch (e->type) {
    case UI_EVENT_DOWN:
        if (e->touch.y >= w->rect.y && e->touch.y < w->rect.y + TAB_BAR_HEIGHT) {
            w->flags |= UI_WIDGET_FLAG_PRESSED;
        }
        break;
    case UI_EVENT_UP:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        }
        break;
    case UI_EVENT_CLICK:
    case UI_EVENT_DOUBLE_CLICK:
        if (e->touch.y >= w->rect.y && e->touch.y < w->rect.y + TAB_BAR_HEIGHT) {
            int16_t tab_w = w->rect.w / tv->tab_count;
            int16_t idx = (e->touch.x - w->rect.x) / tab_w;
            if (idx >= 0 && idx < tv->tab_count && idx != tv->active_tab) {
                tv->active_tab = (uint8_t)idx;
                ui_widget_invalidate(w);
                if (tv->on_tab_change) tv->on_tab_change(w, tv->active_tab);
            }
        }
        break;
    case UI_EVENT_PRESS_CANCEL:
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
        w->flags &= ~UI_WIDGET_FLAG_PRESSED;
        break;
    default:
        break;
    }
}

void ui_tabview_init(ui_tabview_t *tv, const ui_rect_t *rect, uint8_t tab_count)
{
    if (!tv) return;
    ui_widget_init(&tv->base, rect);
    tv->base.draw_cb = tabview_draw_cb;
    tv->base.event_cb = tabview_event_cb;
    tv->tab_count = (tab_count > UI_TABVIEW_MAX_TABS) ? UI_TABVIEW_MAX_TABS : tab_count;
    tv->active_tab = 0;
    tv->tab_bg_color = UI_COLOR_BG_MAIN;
    tv->tab_active_color = UI_COLOR_BG_CARD;
    tv->tab_text_color = UI_COLOR_TEXT_SECONDARY;
    tv->tab_active_text_color = UI_COLOR_TEXT_PRIMARY;
    tv->indicator_color = UI_COLOR_PRIMARY;
    tv->content_rect.x = rect->x;
    tv->content_rect.y = rect->y + TAB_BAR_HEIGHT;
    tv->content_rect.w = rect->w;
    tv->content_rect.h = rect->h - TAB_BAR_HEIGHT;
    tv->on_tab_change = NULL;
    memset(tv->tab_labels, 0, sizeof(tv->tab_labels));
}

void ui_tabview_set_label(ui_tabview_t *tv, uint8_t idx, const char *label)
{
    if (!tv || idx >= tv->tab_count) return;
    tv->tab_labels[idx] = label;
}

void ui_tabview_set_active(ui_tabview_t *tv, uint8_t idx)
{
    if (!tv || idx >= tv->tab_count) return;
    tv->active_tab = idx;
    ui_widget_invalidate(&tv->base);
}

uint8_t ui_tabview_get_active(ui_tabview_t *tv)
{
    if (!tv) return 0;
    return tv->active_tab;
}

void ui_tabview_set_callback(ui_tabview_t *tv, void (*on_tab_change)(ui_widget_t *, uint8_t))
{
    if (!tv) return;
    tv->on_tab_change = on_tab_change;
}

/*=============================================================================
 *  Status Dot Widget Implementation
 *=============================================================================*/

static void status_dot_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_status_dot_t *dot = (ui_status_dot_t *)w;
    int16_t r = (w->rect.w < w->rect.h) ? w->rect.w / 2 : w->rect.h / 2;
    if (r < 2) r = 2;
    int16_t cx = w->rect.x + w->rect.w / 2;
    int16_t cy = w->rect.y + w->rect.h / 2;
    ui_color_t color = dot->online ? dot->online_color : dot->offline_color;
    ui_draw_fill_circle(cx, cy, r, color);
}

void ui_status_dot_init(ui_status_dot_t *dot, const ui_rect_t *rect, bool online)
{
    if (!dot) return;
    ui_widget_init(&dot->base, rect);
    dot->base.draw_cb = status_dot_draw_cb;
    dot->online = online;
    dot->online_color = UI_COLOR_STATUS_ON;
    dot->offline_color = UI_COLOR_STATUS_OFF;
}

void ui_status_dot_set_state(ui_status_dot_t *dot, bool online)
{
    if (!dot) return;
    dot->online = online;
    ui_widget_invalidate(&dot->base);
}

/*=============================================================================
 *  Dialog Widget Implementation (Modal Popup)
 *=============================================================================*/

#define DLG_CARD_W      400
#define DLG_CARD_H      240
#define DLG_CARD_X      ((UI_SCREEN_WIDTH - DLG_CARD_W) / 2)
#define DLG_CARD_Y      ((UI_SCREEN_HEIGHT - DLG_CARD_H) / 2)

#define DLG_BTN_W       140
#define DLG_BTN_H       36
#define DLG_BTN_Y       (DLG_CARD_Y + DLG_CARD_H - 54)
#define DLG_ACCEPT_X    (DLG_CARD_X + DLG_CARD_W - 24 - DLG_BTN_W)
#define DLG_CANCEL_X    (DLG_CARD_X + 24)

static void dialog_bg_event_cb(ui_widget_t *w, ui_event_t *e)
{
    (void)w;
    if (e->type == UI_EVENT_DOWN) {
        ui_page_pop();
    }
}

static void dialog_draw_cb(ui_page_t *page, ui_rect_t *dirty)
{
    (void)dirty;
    ui_dialog_t *dlg = (ui_dialog_t *)page;

    /* Dimmed fullscreen background */
    ui_rect_t bg_rect = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_draw_fill_rect(&bg_rect, UI_COLOR_DARK_GRAY);

    /* White rounded card */
    ui_rect_t card = {DLG_CARD_X, DLG_CARD_Y, DLG_CARD_W, DLG_CARD_H};
    ui_draw_fill_round_rect(&card, 16, UI_COLOR_BG_CARD);

    /* Message text with '\n' line break support */
    if (dlg->message) {
        int16_t x = DLG_CARD_X + 24;
        int16_t y = DLG_CARD_Y + 70;
        int16_t max_y = DLG_BTN_Y - 10;
        const char *p = dlg->message;

        while (*p && y < max_y) {
            const char *line_start = p;
            while (*p && *p != '\n') p++;

            uint16_t len = p - line_start;
            if (len > 0) {
                char buf[64];
                if (len > 63) len = 63;
                memcpy(buf, line_start, len);
                buf[len] = '\0';
                ui_draw_text(x, y, buf, &font_montserrat_12, UI_COLOR_TEXT_PRIMARY);
            }
            y += 20;
            if (*p == '\n') p++;
        }
    }
}

static void dialog_accept_click(ui_widget_t *w)
{
    ui_dialog_t *dlg = (ui_dialog_t *)w->user_data;
    if (dlg && dlg->on_accept) dlg->on_accept();
    ui_page_pop();
}

static void dialog_cancel_click(ui_widget_t *w)
{
    ui_dialog_t *dlg = (ui_dialog_t *)w->user_data;
    if (dlg && dlg->on_cancel) dlg->on_cancel();
    ui_page_pop();
}

void ui_dialog_init(ui_dialog_t *dlg)
{
    if (!dlg) return;
    memset(dlg, 0, sizeof(ui_dialog_t));

    ui_page_struct_init_fullscreen(&dlg->page, "Dialog", 0xFF);

    /* Fullscreen transparent background catcher */
    ui_rect_t full = {0, 0, UI_SCREEN_WIDTH, UI_SCREEN_HEIGHT};
    ui_widget_init(&dlg->bg, &full);
    dlg->bg.event_cb = dialog_bg_event_cb;
    dlg->bg.bg_color = UI_COLOR_TRANSPARENT;

    /* Title label */
    ui_rect_t title_rect = {DLG_CARD_X + 24, DLG_CARD_Y + 24, DLG_CARD_W - 48, 28};
    ui_label_init(&dlg->lbl_title, &title_rect, "", &font_montserrat_16);
    ui_label_set_color(&dlg->lbl_title, UI_COLOR_TEXT_PRIMARY);
    ui_label_set_align(&dlg->lbl_title, 1);

    /* Accept button (right) */
    ui_rect_t accept_rect = {DLG_ACCEPT_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H};
    ui_button_init(&dlg->btn_accept, &accept_rect, "Accept", &font_montserrat_12);
    dlg->btn_accept.base.user_data = dlg;
    ui_button_set_callback(&dlg->btn_accept, dialog_accept_click);
    ui_button_set_colors(&dlg->btn_accept, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);

    /* Cancel button (left) */
    ui_rect_t cancel_rect = {DLG_CANCEL_X, DLG_BTN_Y, DLG_BTN_W, DLG_BTN_H};
    ui_button_init(&dlg->btn_cancel, &cancel_rect, "Cancel", &font_montserrat_12);
    dlg->btn_cancel.base.user_data = dlg;
    ui_button_set_callback(&dlg->btn_cancel, dialog_cancel_click);
    ui_button_set_colors(&dlg->btn_cancel, UI_COLOR_LIGHT_GRAY, UI_COLOR_GRAY, UI_COLOR_TEXT_PRIMARY);

    dlg->widgets[0] = (ui_widget_t *)&dlg->bg;
    dlg->widgets[1] = (ui_widget_t *)&dlg->lbl_title;
    dlg->widgets[2] = (ui_widget_t *)&dlg->btn_cancel;
    dlg->widgets[3] = (ui_widget_t *)&dlg->btn_accept;

    ui_page_set_widgets(&dlg->page, dlg->widgets, 4);
    ui_page_set_callbacks(&dlg->page, NULL, NULL, dialog_draw_cb, NULL);
}

void ui_dialog_show(ui_dialog_t *dlg, const char *title, const char *message,
                    void (*on_accept)(void), void (*on_cancel)(void))
{
    if (!dlg) return;

    dlg->title = title;
    dlg->message = message;
    dlg->on_accept = on_accept;
    dlg->on_cancel = on_cancel;

    if (title) {
        ui_label_set_text(&dlg->lbl_title, title);
    }

    ui_page_push(&dlg->page);
}

void ui_dialog_close(ui_dialog_t *dlg)
{
    (void)dlg;
    ui_page_pop();
}
