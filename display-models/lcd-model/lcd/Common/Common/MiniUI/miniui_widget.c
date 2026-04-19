/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_widget.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : MiniUI widget system implementation.
********************************************************************************/
#include "miniui_widget.h"
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
    }
    w->flags = UI_WIDGET_FLAG_VISIBLE | UI_WIDGET_FLAG_ENABLED;
    w->bg_color = UI_COLOR_TRANSPARENT;
}

void ui_widget_set_visible(ui_widget_t *w, bool visible)
{
    if (!w) return;
    if (visible) {
        w->flags |= UI_WIDGET_FLAG_VISIBLE;
    } else {
        w->flags &= ~UI_WIDGET_FLAG_VISIBLE;
    }
    ui_widget_invalidate(w);
}

void ui_widget_set_enabled(ui_widget_t *w, bool enabled)
{
    if (!w) return;
    if (enabled) {
        w->flags |= UI_WIDGET_FLAG_ENABLED;
    } else {
        w->flags &= ~UI_WIDGET_FLAG_ENABLED;
    }
}

void ui_widget_invalidate(ui_widget_t *w)
{
    if (!w) return;
    w->flags |= UI_WIDGET_FLAG_DIRTY;
}

void ui_widget_draw(ui_widget_t *w, ui_rect_t *dirty)
{
    if (!w || !(w->flags & UI_WIDGET_FLAG_VISIBLE)) return;
    
    if (w->draw_cb) {
        w->draw_cb(w, dirty);
    }
    w->flags &= ~UI_WIDGET_FLAG_DIRTY;
}

void ui_widget_event(ui_widget_t *w, ui_event_t *e)
{
    if (!w || !(w->flags & UI_WIDGET_FLAG_ENABLED)) return;
    
    if (w->event_cb) {
        w->event_cb(w, e);
    }
}

bool ui_widget_hit_test(ui_widget_t *w, int16_t x, int16_t y)
{
    if (!w) return false;
    return (x >= w->rect.x && x < w->rect.x + w->rect.w &&
            y >= w->rect.y && y < w->rect.y + w->rect.h);
}

/*=============================================================================
 *  Label Widget Implementation
 *=============================================================================*/

static void label_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_label_t *label = (ui_label_t *)w;
    if (!label->text || !label->font) return;
    
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

/*=============================================================================
 *  Button Widget Implementation
 *=============================================================================*/

static void button_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_button_t *btn = (ui_button_t *)w;
    bool pressed = (w->flags & UI_WIDGET_FLAG_PRESSED);
    ui_color_t bg = pressed ? btn->bg_color_pressed : btn->bg_color_normal;
    
    /* Draw button background */
    if (btn->radius > 0) {
        ui_draw_fill_round_rect(&w->rect, btn->radius, bg);
    } else {
        ui_draw_fill_rect(&w->rect, bg);
    }
    
    /* Draw text */
    if (btn->text && btn->font) {
        ui_draw_text_in_rect(&w->rect, btn->text, btn->font, btn->text_color, 1);
    }
}

static void button_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_button_t *btn = (ui_button_t *)w;
    
    switch (e->type) {
        case UI_EVENT_PRESS:
            w->flags |= UI_WIDGET_FLAG_PRESSED;
            ui_widget_invalidate(w);
            break;
            
        case UI_EVENT_RELEASE:
            if (w->flags & UI_WIDGET_FLAG_PRESSED) {
                w->flags &= ~UI_WIDGET_FLAG_PRESSED;
                ui_widget_invalidate(w);
                if (btn->on_click) {
                    btn->on_click(w);
                }
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
 *  Slider Widget Implementation
 *=============================================================================*/

static void slider_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_slider_t *slider = (ui_slider_t *)w;
    
    int16_t track_y = w->rect.y + w->rect.h / 2 - 2;
    int16_t track_h = 4;
    
    /* Draw track background */
    ui_rect_t track = {w->rect.x, track_y, w->rect.w, track_h};
    ui_draw_fill_rect(&track, slider->track_color);
    
    /* Draw filled portion */
    int16_t fill_w = (int32_t)(slider->value - slider->min) * w->rect.w / (slider->max - slider->min);
    if (fill_w > 0) {
        ui_rect_t fill = {w->rect.x, track_y, fill_w, track_h};
        ui_draw_fill_rect(&fill, slider->fill_color);
    }
    
    /* Draw knob */
    int16_t knob_x = w->rect.x + fill_w;
    int16_t knob_r = w->rect.h / 2 - 2;
    ui_draw_fill_circle(knob_x, track_y + track_h / 2, knob_r, slider->knob_color);
}

void ui_slider_init(ui_slider_t *slider, const ui_rect_t *rect, int16_t min, int16_t max, int16_t value)
{
    if (!slider) return;
    ui_widget_init(&slider->base, rect);
    slider->base.draw_cb = slider_draw_cb;
    slider->min = min;
    slider->max = max;
    slider->value = value;
    slider->track_color = UI_COLOR_LIGHT_GRAY;
    slider->fill_color = UI_COLOR_PRIMARY;
    slider->knob_color = UI_COLOR_WHITE;
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

/*=============================================================================
 *  Switch Widget Implementation
 *=============================================================================*/

static void switch_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_switch_t *sw = (ui_switch_t *)w;
    
    int16_t track_h = w->rect.h / 2;
    int16_t track_y = w->rect.y + (w->rect.h - track_h) / 2;
    int16_t track_r = track_h / 2;
    
    /* Draw track */
    ui_rect_t track = {w->rect.x, track_y, w->rect.w, track_h};
    ui_color_t track_color = sw->state ? sw->track_on_color : sw->track_off_color;
    ui_draw_fill_round_rect(&track, track_r, track_color);
    
    /* Draw knob */
    int16_t knob_r = track_h / 2 - 2;
    int16_t knob_x = sw->state ? (w->rect.x + w->rect.w - track_h / 2) : (w->rect.x + track_h / 2);
    int16_t knob_y = w->rect.y + w->rect.h / 2;
    ui_draw_fill_circle(knob_x, knob_y, knob_r, sw->knob_color);
}

static void switch_event_cb(ui_widget_t *w, ui_event_t *e)
{
    ui_switch_t *sw = (ui_switch_t *)w;
    
    if (e->type == UI_EVENT_PRESS) {
        sw->state = !sw->state;
        ui_widget_invalidate(w);
        if (sw->on_toggle) {
            sw->on_toggle(w, sw->state);
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

/*=============================================================================
 *  Progress Bar Widget Implementation
 *=============================================================================*/

static void progress_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_progress_t *prog = (ui_progress_t *)w;
    
    /* Draw track */
    ui_draw_fill_rect(&w->rect, prog->track_color);
    
    /* Draw fill */
    int16_t fill_w = (int32_t)(prog->value - prog->min) * w->rect.w / (prog->max - prog->min);
    if (fill_w > 0) {
        ui_rect_t fill = {w->rect.x, w->rect.y, fill_w, w->rect.h};
        ui_draw_fill_rect(&fill, prog->fill_color);
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
    
    /* Draw card background */
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
    
    /* Draw children */
    for (uint16_t i = 0; i < card->child_count; i++) {
        if (card->children[i]) {
            ui_widget_draw(card->children[i], dirty);
        }
    }
}

void ui_card_init(ui_card_t *card, const ui_rect_t *rect)
{
    if (!card) return;
    ui_widget_init(&card->base, rect);
    card->base.draw_cb = card_draw_cb;
    card->base.bg_color = UI_COLOR_BG_CARD;
    card->radius = 8;
    card->border_color = UI_COLOR_LIGHT_GRAY;
    card->border_width = 0;
    card->children = NULL;
    card->child_count = 0;
}

void ui_card_add_child(ui_card_t *card, ui_widget_t *child)
{
    if (!card || !child) return;
    /* Simple implementation: just draw children, no layout management */
    /* For full implementation, would need dynamic array allocation */
}

/*=============================================================================
 *  List Item Widget Implementation
 *=============================================================================*/

static void list_item_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_list_item_t *item = (ui_list_item_t *)w;
    
    /* Draw background */
    ui_draw_fill_rect(&w->rect, w->bg_color);
    
    /* Draw title text */
    if (item->title && item->font) {
        ui_rect_t text_rect = {
            w->rect.x + 10,
            w->rect.y,
            w->rect.w / 2,
            w->rect.h
        };
        ui_draw_text_in_rect(&text_rect, item->title, item->font, item->text_color, 0);
    }
    
    /* Draw control widget */
    if (item->control) {
        ui_widget_draw(item->control, dirty);
    }
    
    /* Draw divider */
    if (item->show_divider) {
        ui_rect_t div = {
            w->rect.x + 10,
            w->rect.y + w->rect.h - 1,
            w->rect.w - 20,
            1
        };
        ui_draw_fill_rect(&div, item->divider_color);
    }
}

void ui_list_item_init(ui_list_item_t *item, const ui_rect_t *rect, const char *title, const ui_font_t *font)
{
    if (!item) return;
    ui_widget_init(&item->base, rect);
    item->base.draw_cb = list_item_draw_cb;
    item->title = title ? title : "";
    item->font = font;
    item->text_color = UI_COLOR_TEXT_PRIMARY;
    item->control = NULL;
    item->show_divider = true;
    item->divider_color = UI_COLOR_LIGHT_GRAY;
}

void ui_list_item_set_control(ui_list_item_t *item, ui_widget_t *control)
{
    if (!item) return;
    item->control = control;
    ui_widget_invalidate(&item->base);
}
