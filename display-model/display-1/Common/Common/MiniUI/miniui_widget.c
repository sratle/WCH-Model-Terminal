/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_widget.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : MiniUI widget system implementation.
********************************************************************************/
#include "miniui_widget.h"
#include "miniui_page.h"
#include <string.h>

/*=============================================================================
 *  Widget Base Implementation
 *=============================================================================*/

void ui_widget_init(ui_widget_t *w, const ui_rect_t *rect)
{
    if (!w) return;
    memset(w, 0, sizeof(ui_widget_t));
    if (rect) {
        w->rect = *rect;
        w->prev_rect = *rect;  /* prev_rect tracks the initial position */
    }
    w->flags = UI_WIDGET_FLAG_VISIBLE | UI_WIDGET_FLAG_ENABLED;
    w->bg_color = UI_COLOR_TRANSPARENT;
}

void ui_widget_set_visible(ui_widget_t *w, bool visible)
{
    if (!w) return;
    if (visible) w->flags |= UI_WIDGET_FLAG_VISIBLE;
    else         w->flags &= ~UI_WIDGET_FLAG_VISIBLE;
    ui_widget_invalidate(w);
}

void ui_widget_set_enabled(ui_widget_t *w, bool enabled)
{
    if (!w) return;
    if (enabled) w->flags |= UI_WIDGET_FLAG_ENABLED;
    else         w->flags &= ~UI_WIDGET_FLAG_ENABLED;
}

void ui_widget_invalidate(ui_widget_t *w)
{
    if (!w) return;
    /* Mark old position as dirty (if valid and different from current) */
    if (w->prev_rect.w > 0 && w->prev_rect.h > 0 &&
        (w->prev_rect.x != w->rect.x || w->prev_rect.y != w->rect.y ||
         w->prev_rect.w != w->rect.w || w->prev_rect.h != w->rect.h)) {
        ui_page_invalidate(&w->prev_rect);
    }
    /* Mark new position as dirty */
    ui_page_invalidate(&w->rect);
    /* Save current rect as prev_rect for next frame */
    w->prev_rect = w->rect;
    w->flags |= UI_WIDGET_FLAG_DIRTY;
}

void ui_widget_draw(ui_widget_t *w, ui_rect_t *dirty)
{
    if (!w || !(w->flags & UI_WIDGET_FLAG_VISIBLE)) return;
    if (w->draw_cb) w->draw_cb(w, dirty);
    w->flags &= ~UI_WIDGET_FLAG_DIRTY;
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
    if (w->rect.x == x && w->rect.y == y) return;
    /* Mark old position dirty before changing */
    ui_page_invalidate(&w->rect);
    w->rect.x = x;
    w->rect.y = y;
    /* Invalidate marks new position and saves prev_rect */
    ui_widget_invalidate(w);
}

void ui_widget_resize(ui_widget_t *w, int16_t width, int16_t height)
{
    if (!w) return;
    if (w->rect.w == width && w->rect.h == height) return;
    ui_page_invalidate(&w->rect);
    w->rect.w = width;
    w->rect.h = height;
    ui_widget_invalidate(w);
}

void ui_widget_set_rect(ui_widget_t *w, const ui_rect_t *rect)
{
    if (!w || !rect) return;
    w->rect = *rect;
    w->prev_rect = *rect;
}

/*=============================================================================
 *  Label Widget Implementation
 *=============================================================================*/

static void label_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_label_t *label = (ui_label_t *)w;
    if (!label->text || !label->font) return;

    if (w->bg_color != UI_COLOR_TRANSPARENT) {
        ui_draw_fill_rect(&w->rect, w->bg_color);
    }
    ui_draw_text_in_rect(&w->rect, label->text, label->font, label->text_color, label->align);
}

void ui_label_init(ui_label_t *label, const ui_rect_t *rect, const char *text, const ui_font_t *font)
{
    if (!label) return;
    ui_widget_init(&label->base, rect);
    label->base.draw_cb = label_draw_cb;
    label->text = text ? text : "";
    label->font = font;
    label->text_color = UI_COLOR_TEXT_PRIMARY;
    label->align = 0;
}

void ui_label_set_text(ui_label_t *label, const char *text)
{
    if (!label) return;
    label->text = text ? text : "";
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
 *  Button Widget Implementation
 *=============================================================================*/

static void button_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_button_t *btn = (ui_button_t *)w;
    bool pressed = (w->flags & UI_WIDGET_FLAG_PRESSED) != 0;
    ui_color_t bg = pressed ? btn->bg_color_pressed : btn->bg_color_normal;

    if (btn->radius > 0) {
        ui_draw_fill_round_rect(&w->rect, btn->radius, bg);
    } else {
        ui_draw_fill_rect(&w->rect, bg);
    }

    if (btn->text && btn->font) {
        ui_draw_text_in_rect(&w->rect, btn->text, btn->font, btn->text_color, 1);
    }
}

static void button_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_button_t *btn = (ui_button_t *)w;

    switch (e->type) {
    case UI_EVENT_DOWN:
        w->flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
        break;
    case UI_EVENT_MOVE:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            if (!ui_widget_hit_test(w, e->pos.x, e->pos.y)) {
                w->flags &= ~UI_WIDGET_FLAG_PRESSED;
                ui_widget_invalidate(w);
            }
        } else {
            if (ui_widget_hit_test(w, e->pos.x, e->pos.y)) {
                w->flags |= UI_WIDGET_FLAG_PRESSED;
                ui_widget_invalidate(w);
            }
        }
        break;
    case UI_EVENT_UP:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        break;
    case UI_EVENT_CLICK:
    case UI_EVENT_DOUBLE_CLICK:
    case UI_EVENT_LONG_PRESS:
        /* Clear pressed state before firing callback to avoid stuck visual
         * if the callback triggers a page navigation */
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        if (btn->on_click) btn->on_click(w);
        break;
    case UI_EVENT_KEY_OK:
        if (btn->on_click) btn->on_click(w);
        break;
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
    case UI_EVENT_PRESS_CANCEL:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        break;
    default:
        break;
    }
}

void ui_button_init(ui_button_t *btn, const ui_rect_t *rect, const char *text, const ui_font_t *font)
{
    if (!btn) return;
    ui_widget_init(&btn->base, rect);
    btn->base.draw_cb = button_draw_cb;
    btn->base.event_cb = button_event_cb;
    btn->text = text ? text : "";
    btn->font = font;
    btn->text_color = UI_COLOR_WHITE;
    btn->bg_color_normal = UI_COLOR_PRIMARY;
    btn->bg_color_pressed = UI_COLOR_SECONDARY;
    btn->radius = 8;
    btn->on_click = NULL;
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
    ui_widget_invalidate(&btn->base);
}

void ui_button_set_callback(ui_button_t *btn, void (*on_click)(ui_widget_t *w))
{
    if (!btn) return;
    btn->on_click = on_click;
}

/*=============================================================================
 *  Icon Button Widget Implementation
 *=============================================================================*/

static void icon_button_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_icon_button_t *btn = (ui_icon_button_t *)w;
    bool pressed = (w->flags & UI_WIDGET_FLAG_PRESSED) != 0;
    ui_color_t bg = pressed ? btn->bg_color_pressed : btn->bg_color_normal;

    if (btn->radius > 0) {
        ui_draw_fill_round_rect(&w->rect, btn->radius, bg);
    } else {
        ui_draw_fill_rect(&w->rect, bg);
    }

    if (btn->icon_bitmap && btn->text && btn->font) {
        int16_t gap = 4;
        int16_t total_h = btn->icon_h + gap + btn->font->height;
        int16_t icon_y = w->rect.y + (w->rect.h - total_h) / 2;
        int16_t icon_x = w->rect.x + (w->rect.w - btn->icon_w) / 2;
        ui_draw_icon(icon_x, icon_y, btn->icon_bitmap, btn->icon_w, btn->icon_h, btn->icon_color);
        ui_rect_t text_rect = {
            w->rect.x,
            icon_y + btn->icon_h + gap,
            w->rect.w,
            btn->font->height + 4
        };
        ui_draw_text_in_rect(&text_rect, btn->text, btn->font, btn->text_color, 1);
    } else if (btn->icon_bitmap) {
        int16_t icon_y = w->rect.y + (w->rect.h - btn->icon_h) / 2;
        int16_t icon_x = w->rect.x + (w->rect.w - btn->icon_w) / 2;
        ui_draw_icon(icon_x, icon_y, btn->icon_bitmap, btn->icon_w, btn->icon_h, btn->icon_color);
    } else if (btn->text && btn->font) {
        ui_rect_t text_rect = {
            w->rect.x,
            w->rect.y,
            w->rect.w,
            w->rect.h
        };
        ui_draw_text_in_rect(&text_rect, btn->text, btn->font, btn->text_color, 1);
    }
}

static void icon_button_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_icon_button_t *btn = (ui_icon_button_t *)w;

    switch (e->type) {
    case UI_EVENT_DOWN:
        w->flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
        break;
    case UI_EVENT_MOVE:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            if (!ui_widget_hit_test(w, e->pos.x, e->pos.y)) {
                w->flags &= ~UI_WIDGET_FLAG_PRESSED;
                ui_widget_invalidate(w);
            }
        } else {
            if (ui_widget_hit_test(w, e->pos.x, e->pos.y)) {
                w->flags |= UI_WIDGET_FLAG_PRESSED;
                ui_widget_invalidate(w);
            }
        }
        break;
    case UI_EVENT_UP:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        break;
    case UI_EVENT_CLICK:
    case UI_EVENT_DOUBLE_CLICK:
    case UI_EVENT_LONG_PRESS:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        if (btn->on_click) btn->on_click(w);
        break;
    case UI_EVENT_KEY_OK:
        if (btn->on_click) btn->on_click(w);
        break;
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
    case UI_EVENT_PRESS_CANCEL:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        break;
    default:
        break;
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
    btn->text = text ? text : "";
    btn->font = font;
    btn->icon_color = UI_COLOR_PRIMARY;
    btn->text_color = UI_COLOR_TEXT_PRIMARY;
    btn->bg_color_normal = UI_COLOR_BG_CARD;
    btn->bg_color_pressed = UI_COLOR_SECONDARY;
    btn->radius = 12;
    btn->on_click = NULL;
}

void ui_icon_button_set_callback(ui_icon_button_t *btn, void (*on_click)(ui_widget_t *w))
{
    if (!btn) return;
    btn->on_click = on_click;
}

/*=============================================================================
 *  Slider Widget Implementation
 *=============================================================================*/

static void slider_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_slider_t *slider = (ui_slider_t *)w;

    int16_t track_y = w->rect.y + w->rect.h / 2 - 2;
    int16_t track_h = 4;

    ui_rect_t track = {w->rect.x + 10, track_y, w->rect.w - 20, track_h};
    ui_draw_fill_round_rect(&track, 2, slider->track_color);

    int16_t usable_w = w->rect.w - 20;
    int16_t fill_w = (int32_t)(slider->value - slider->min) * usable_w / (slider->max - slider->min);
    if (fill_w > 0) {
        ui_rect_t fill = {w->rect.x + 10, track_y, fill_w, track_h};
        ui_draw_fill_round_rect(&fill, 2, slider->fill_color);
    }

    int16_t knob_x = w->rect.x + 10 + fill_w;
    int16_t knob_r = 6;
    ui_draw_fill_circle(knob_x, track_y + track_h / 2, knob_r, slider->knob_color);
}

static void slider_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_slider_t *slider = (ui_slider_t *)w;

    switch (e->type) {
    case UI_EVENT_DOWN:
        slider->dragging = true;
        /* fall through */
    case UI_EVENT_MOVE:
        if (slider->dragging) {
            int16_t usable_w = w->rect.w - 20;
            int16_t rel_x = e->pos.x - w->rect.x - 10;
            if (rel_x < 0) rel_x = 0;
            if (rel_x > usable_w) rel_x = usable_w;
            int16_t new_val = slider->min + (int32_t)rel_x * (slider->max - slider->min) / usable_w;
            if (new_val != slider->value) {
                slider->value = new_val;
                ui_widget_invalidate(w);
                if (slider->on_change) slider->on_change(w, slider->value);
            }
        }
        break;
    case UI_EVENT_UP:
        slider->dragging = false;
        break;
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
    case UI_EVENT_PRESS_CANCEL:
        slider->dragging = false;
        break;
    default:
        break;
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
    slider->track_color = UI_COLOR_LIGHT_GRAY;
    slider->fill_color = UI_COLOR_PRIMARY;
    slider->knob_color = UI_COLOR_ACCENT;
    slider->dragging = false;
    slider->on_change = NULL;
}

void ui_slider_set_value(ui_slider_t *slider, int16_t value)
{
    if (!slider) return;
    if (value < slider->min) value = slider->min;
    if (value > slider->max) value = slider->max;
    slider->value = value;
    ui_widget_invalidate(&slider->base);
}

void ui_slider_set_callback(ui_slider_t *slider, void (*on_change)(ui_widget_t *w, int16_t value))
{
    if (!slider) return;
    slider->on_change = on_change;
}

/*=============================================================================
 *  Switch Widget Implementation
 *=============================================================================*/

static void switch_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_switch_t *sw = (ui_switch_t *)w;

    int16_t track_h = w->rect.h / 2;
    int16_t track_y = w->rect.y + (w->rect.h - track_h) / 2;
    int16_t track_r = track_h / 2;

    ui_rect_t track = {w->rect.x, track_y, w->rect.w, track_h};
    bool pressed = (w->flags & UI_WIDGET_FLAG_PRESSED) != 0;
    ui_color_t track_color;
    if (pressed) {
        track_color = sw->state ? sw->track_off_color : sw->track_on_color;
    } else {
        track_color = sw->state ? sw->track_on_color : sw->track_off_color;
    }
    ui_draw_fill_round_rect(&track, track_r, track_color);

    int16_t knob_r = track_h / 2 - 2;
    int16_t knob_x = sw->state ? (w->rect.x + w->rect.w - track_h / 2) : (w->rect.x + track_h / 2);
    int16_t knob_y = w->rect.y + w->rect.h / 2;
    ui_draw_fill_circle(knob_x, knob_y, knob_r, sw->knob_color);
}

static void switch_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_switch_t *sw = (ui_switch_t *)w;

    switch (e->type) {
    case UI_EVENT_DOWN:
        w->flags |= UI_WIDGET_FLAG_PRESSED;
        ui_widget_invalidate(w);
        break;
    case UI_EVENT_UP:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        break;
    case UI_EVENT_CLICK:
    case UI_EVENT_DOUBLE_CLICK:
        sw->state = !sw->state;
        ui_widget_invalidate(w);
        if (sw->on_toggle) sw->on_toggle(w, sw->state);
        break;
    case UI_EVENT_KEY_OK:
        sw->state = !sw->state;
        ui_widget_invalidate(w);
        if (sw->on_toggle) sw->on_toggle(w, sw->state);
        break;
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
    case UI_EVENT_PRESS_CANCEL:
        if (w->flags & UI_WIDGET_FLAG_PRESSED) {
            w->flags &= ~UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
        }
        break;
    default:
        break;
    }
}

void ui_switch_init(ui_switch_t *sw, const ui_rect_t *rect, bool state)
{
    if (!sw) return;
    ui_widget_init(&sw->base, rect);
    sw->base.draw_cb = switch_draw_cb;
    sw->base.event_cb = switch_event_cb;
    sw->state = state;
    sw->track_on_color = UI_COLOR_PRIMARY;
    sw->track_off_color = UI_COLOR_LIGHT_GRAY;
    sw->knob_color = UI_COLOR_WHITE;
    sw->on_toggle = NULL;
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
 *  Progress Bar Widget Implementation
 *=============================================================================*/

static void progress_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_progress_t *prog = (ui_progress_t *)w;

    int16_t track_r = w->rect.h / 2;
    ui_draw_fill_round_rect(&w->rect, track_r, prog->track_color);

    int16_t fill_w = (int32_t)(prog->value - prog->min) * w->rect.w / (prog->max - prog->min);
    if (fill_w > 0) {
        if (fill_w > w->rect.w) fill_w = w->rect.w;
        int16_t fill_r = track_r;
        if (fill_w < fill_r * 2) {
            fill_r = fill_w / 2;
        }
        ui_rect_t fill = {w->rect.x, w->rect.y, fill_w, w->rect.h};
        ui_draw_fill_round_rect(&fill, fill_r, prog->fill_color);
    }
}

void ui_progress_init(ui_progress_t *prog, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value)
{
    if (!prog) return;
    ui_widget_init(&prog->base, rect);
    prog->base.draw_cb = progress_draw_cb;
    prog->min = min;
    prog->max = max;
    prog->value = value;
    prog->track_color = UI_COLOR_LIGHT_GRAY;
    prog->fill_color = UI_COLOR_PRIMARY;
    prog->show_text = false;
}

void ui_progress_set_value(ui_progress_t *prog, int16_t value)
{
    if (!prog) return;
    if (value < prog->min) value = prog->min;
    if (value > prog->max) value = prog->max;
    prog->value = value;
    ui_widget_invalidate(&prog->base);
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

    switch (e->type) {
    case UI_EVENT_DOWN: {
        card->active_child = -1;
        for (uint16_t i = 0; i < card->child_count; i++) {
            if (card->children[i] && ui_widget_hit_test(card->children[i], e->pos.x, e->pos.y)) {
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
    card->border_color = UI_COLOR_LIGHT_GRAY;
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
 *  List Item Widget Implementation
 *=============================================================================*/

static void list_item_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_list_item_t *item = (ui_list_item_t *)w;

    ui_draw_fill_rect(&w->rect, w->bg_color);

    if (item->title && item->font) {
        ui_rect_t text_rect;
        if (item->control) {
            text_rect.x = w->rect.x + 10;
            text_rect.y = w->rect.y;
            text_rect.w = w->rect.w - item->control->rect.w - 20;
            text_rect.h = w->rect.h;
        } else {
            text_rect.x = w->rect.x + 10;
            text_rect.y = w->rect.y;
            text_rect.w = w->rect.w - 20;
            text_rect.h = w->rect.h;
        }
        ui_draw_text_in_rect(&text_rect, item->title, item->font, item->text_color, 0);
    }

    if (item->control) {
        ui_widget_draw(item->control, dirty);
    }

    if (item->show_divider) {
        ui_rect_t div = { w->rect.x + 10, w->rect.y + w->rect.h - 1, w->rect.w - 20, 1 };
        ui_draw_fill_rect(&div, item->divider_color);
    }
}

static void list_item_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_list_item_t *item = (ui_list_item_t *)w;
    if (!item->control) return;

    switch (e->type) {
    case UI_EVENT_DOWN:
        if (ui_widget_hit_test(item->control, e->pos.x, e->pos.y)) {
            item->control_active = true;
            ui_widget_event(item->control, e);
        }
        break;
    case UI_EVENT_MOVE:
        if (item->control_active) {
            ui_widget_event(item->control, e);
        }
        break;
    case UI_EVENT_UP:
        if (item->control_active) {
            ui_widget_event(item->control, e);
        }
        break;
    case UI_EVENT_CLICK:
    case UI_EVENT_DOUBLE_CLICK:
    case UI_EVENT_LONG_PRESS:
        if (item->control_active) {
            ui_widget_event(item->control, e);
        }
        item->control_active = false;
        break;
    case UI_EVENT_SWIPE_UP:
    case UI_EVENT_SWIPE_DOWN:
    case UI_EVENT_SWIPE_LEFT:
    case UI_EVENT_SWIPE_RIGHT:
    case UI_EVENT_PRESS_CANCEL:
        if (item->control_active) {
            item->control_active = false;
            ui_widget_event(item->control, e);
        }
        break;
    default:
        ui_widget_event(item->control, e);
        break;
    }
}

void ui_list_item_init(ui_list_item_t *item, const ui_rect_t *rect, const char *title, const ui_font_t *font)
{
    if (!item) return;
    ui_widget_init(&item->base, rect);
    item->base.draw_cb = list_item_draw_cb;
    item->base.event_cb = list_item_event_cb;
    item->base.bg_color = UI_COLOR_BG_CARD;
    item->title = title ? title : "";
    item->font = font;
    item->text_color = UI_COLOR_TEXT_PRIMARY;
    item->control = NULL;
    item->show_divider = true;
    item->divider_color = UI_COLOR_LIGHT_GRAY;
    item->control_active = false;
}

void ui_list_item_set_control(ui_list_item_t *item, ui_widget_t *control)
{
    if (!item) return;
    item->control = control;
    ui_widget_invalidate(&item->base);
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
        if (e->pos.y >= w->rect.y && e->pos.y < w->rect.y + TAB_BAR_HEIGHT) {
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
        if (e->pos.y >= w->rect.y && e->pos.y < w->rect.y + TAB_BAR_HEIGHT) {
            int16_t tab_w = w->rect.w / tv->tab_count;
            int16_t idx = (e->pos.x - w->rect.x) / tab_w;
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

    /* Fullscreen transparent background catcher (lowest z-order) */
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
