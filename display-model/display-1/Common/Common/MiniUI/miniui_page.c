/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_page.c
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : MiniUI page manager implementation.
*                      Page stack, navigation, and dirty region tracking.
********************************************************************************/
#include "miniui_page.h"
#include "miniui_widget.h"
#include "miniui_input.h"
#include "miniui_render.h"
#include "debug.h"
#include <string.h>

static int16_t s_sidebar_width = 200;

/*=============================================================================
 *  Internal State
 *=============================================================================*/

static ui_page_t *s_registered_pages[32];
static uint8_t s_registered_count = 0;

static ui_page_t *s_page_stack[UI_PAGE_STACK_DEPTH];
static int8_t s_stack_top = -1;

static ui_dirty_list_t s_dirty_list;

static ui_sidebar_draw_cb_t s_sidebar_draw = NULL;
static ui_sidebar_event_cb_t s_sidebar_event = NULL;

/*=============================================================================
 *  Helper Functions
 *=============================================================================*/

/* Clear PRESSED flag on all widgets of a page to avoid stuck visual state */
static void page_reset_widget_states(ui_page_t *page)
{
    if (!page) return;
    for (uint16_t i = 0; i < page->widget_count; i++) {
        if (page->widgets[i]) {
            page->widgets[i]->flags &= ~UI_WIDGET_FLAG_PRESSED;
        }
    }
}

static bool rects_overlap(const ui_rect_t *a, const ui_rect_t *b)
{
    return (a->x < b->x + b->w && a->x + a->w > b->x &&
            a->y < b->y + b->h && a->y + a->h > b->y);
}

static void rect_merge(const ui_rect_t *a, const ui_rect_t *b, ui_rect_t *out)
{
    int16_t x1 = (a->x < b->x) ? a->x : b->x;
    int16_t y1 = (a->y < b->y) ? a->y : b->y;
    int16_t x2 = (a->x + a->w > b->x + b->w) ? a->x + a->w : b->x + b->w;
    int16_t y2 = (a->y + a->h > b->y + b->h) ? a->y + a->h : b->y + b->h;

    out->x = x1;
    out->y = y1;
    out->w = x2 - x1;
    out->h = y2 - y1;
}

static void rect_clamp_screen(ui_rect_t *r)
{
    if (r->x < 0) r->x = 0;
    if (r->y < 0) r->y = 0;
    if (r->x + r->w > UI_SCREEN_WIDTH) r->w = UI_SCREEN_WIDTH - r->x;
    if (r->y + r->h > UI_SCREEN_HEIGHT) r->h = UI_SCREEN_HEIGHT - r->y;
}

/*=============================================================================
 *  Page Manager Implementation
 *=============================================================================*/

void ui_page_init(void)
{
    printf("[ui_page_init] start\r\n");
    s_registered_count = 0;
    s_stack_top = -1;
    s_dirty_list.count = 0;
    s_sidebar_draw = NULL;
    s_sidebar_event = NULL;
    memset(s_registered_pages, 0, sizeof(s_registered_pages));
    memset(s_page_stack, 0, sizeof(s_page_stack));
    printf("[ui_page_init] done\r\n");
}

void ui_page_register(ui_page_t *page)
{
    if (!page || s_registered_count >= 32) return;
    s_registered_pages[s_registered_count++] = page;
}

void ui_page_switch(ui_page_t *page)
{
    if (!page) return;

    if (s_stack_top >= 0 && s_page_stack[s_stack_top]) {
        ui_page_t *current = s_page_stack[s_stack_top];
        page_reset_widget_states(current);
        if (current->on_exit) current->on_exit(current);
    }

    ui_input_set_capture(NULL, UI_TOUCH_ID_NONE);
    s_stack_top = -1;
    s_dirty_list.count = 0;

    s_page_stack[++s_stack_top] = page;

    if (page->on_enter) page->on_enter(page);

    ui_page_invalidate_all();
}

void ui_page_push(ui_page_t *page)
{
    if (!page || s_stack_top >= UI_PAGE_STACK_DEPTH - 1) return;

    if (s_stack_top >= 0 && s_page_stack[s_stack_top]) {
        ui_page_t *current = s_page_stack[s_stack_top];
        page_reset_widget_states(current);
        if (current->on_exit) current->on_exit(current);
    }

    ui_input_set_capture(NULL, UI_TOUCH_ID_NONE);
    s_page_stack[++s_stack_top] = page;

    if (page->on_enter) page->on_enter(page);

    ui_page_invalidate_all();
}

void ui_page_pop(void)
{
    if (s_stack_top <= 0) return;

    ui_page_t *current = s_page_stack[s_stack_top];
    if (current) {
        page_reset_widget_states(current);
        if (current->on_exit) current->on_exit(current);
    }

    ui_input_set_capture(NULL, UI_TOUCH_ID_NONE);
    s_page_stack[s_stack_top--] = NULL;

    ui_page_t *prev = s_page_stack[s_stack_top];
    page_reset_widget_states(prev);
    if (prev && prev->on_enter) prev->on_enter(prev);

    /* Full invalidate to ensure previous page redraws everything,
     * covering any content drawn by fullscreen/game pages */
    ui_page_invalidate_all();
}

bool ui_page_can_go_back(void)
{
    return s_stack_top > 0;
}

ui_page_t* ui_page_current(void)
{
    if (s_stack_top < 0) return NULL;
    return s_page_stack[s_stack_top];
}

/*=============================================================================
 *  Sidebar Integration
 *=============================================================================*/

void ui_page_set_sidebar_callbacks(ui_sidebar_draw_cb_t draw, ui_sidebar_event_cb_t event)
{
    s_sidebar_draw = draw;
    s_sidebar_event = event;
}

void ui_page_set_sidebar_width(int16_t width)
{
    if (width >= 0) s_sidebar_width = width;
}

ui_sidebar_event_cb_t ui_page_get_sidebar_event_cb(void)
{
    return s_sidebar_event;
}

/*=============================================================================
 *  Dirty Region Tracking
 *=============================================================================*/

void ui_page_invalidate(const ui_rect_t *rect)
{
    if (!rect) return;

    ui_rect_t r = *rect;
    rect_clamp_screen(&r);

    if (r.w <= 0 || r.h <= 0) return;

    int16_t merge_target = -1;

    for (uint16_t i = 0; i < s_dirty_list.count; i++) {
        if (rects_overlap(&r, &s_dirty_list.regions[i])) {
            if (merge_target < 0) {
                rect_merge(&r, &s_dirty_list.regions[i], &s_dirty_list.regions[i]);
                rect_clamp_screen(&s_dirty_list.regions[i]);
                merge_target = i;
            } else {
                rect_merge(&s_dirty_list.regions[merge_target], &s_dirty_list.regions[i], &s_dirty_list.regions[merge_target]);
                rect_clamp_screen(&s_dirty_list.regions[merge_target]);
                s_dirty_list.regions[i] = s_dirty_list.regions[s_dirty_list.count - 1];
                s_dirty_list.count--;
                i--;
            }
        }
    }

    if (merge_target >= 0) return;

    if (s_dirty_list.count < UI_MAX_DIRTY_REGIONS) {
        s_dirty_list.regions[s_dirty_list.count++] = r;
    } else {
        s_dirty_list.regions[0].x = 0;
        s_dirty_list.regions[0].y = 0;
        s_dirty_list.regions[0].w = UI_SCREEN_WIDTH;
        s_dirty_list.regions[0].h = UI_SCREEN_HEIGHT;
        s_dirty_list.count = 1;
    }
}

void ui_page_invalidate_all(void)
{
    s_dirty_list.regions[0].x = 0;
    s_dirty_list.regions[0].y = 0;
    s_dirty_list.regions[0].w = UI_SCREEN_WIDTH;
    s_dirty_list.regions[0].h = UI_SCREEN_HEIGHT;
    s_dirty_list.count = 1;
}

const ui_dirty_list_t *ui_page_get_dirty_list(void)
{
    return &s_dirty_list;
}

void ui_page_clear_dirty(void)
{
    s_dirty_list.count = 0;
}

/*=============================================================================
 *  Page Drawing
 *=============================================================================*/

void ui_page_draw(void)
{
    if (s_dirty_list.count == 0) return;

    ui_page_t *page = ui_page_current();
    if (!page) return;

    bool is_fullscreen = (page->flags & UI_PAGE_FLAG_FULLSCREEN) != 0;

    if (s_sidebar_draw && !is_fullscreen) {
        ui_rect_t sidebar_rect = {0, 0, s_sidebar_width, UI_SCREEN_HEIGHT};
        bool sidebar_dirty = false;
        for (uint16_t i = 0; i < s_dirty_list.count; i++) {
            if (rects_overlap(&s_dirty_list.regions[i], &sidebar_rect)) {
                sidebar_dirty = true;
                break;
            }
        }
        if (sidebar_dirty) {
            ui_rect_t full_sidebar = {0, 0, s_sidebar_width, UI_SCREEN_HEIGHT};
            ui_render_set_clip(&full_sidebar);
            s_sidebar_draw(&full_sidebar);
        }
    }

    for (uint16_t i = 0; i < s_dirty_list.count; i++) {
        ui_rect_t *dirty = &s_dirty_list.regions[i];

        ui_render_set_clip(dirty);

        if (page->on_draw) {
            page->on_draw(page, dirty);
        }

        for (uint16_t j = 0; j < page->widget_count; j++) {
            if (page->widgets[j]) {
                if (rects_overlap(&page->widgets[j]->rect, dirty)) {
                    ui_widget_draw(page->widgets[j], dirty);
                }
            }
        }
    }

    s_dirty_list.count = 0;
    ui_render_reset_clip();
}

/*=============================================================================
 *  Page Helpers
 *=============================================================================*/

void ui_page_struct_init(ui_page_t *page, const char *name, uint32_t id)
{
    if (!page) return;
    memset(page, 0, sizeof(ui_page_t));
    page->name = name;
    page->id = id;
    page->content_rect.x = s_sidebar_width;
    page->content_rect.y = 0;
    page->content_rect.w = UI_SCREEN_WIDTH - s_sidebar_width;
    page->content_rect.h = UI_SCREEN_HEIGHT;
}

void ui_page_struct_init_fullscreen(ui_page_t *page, const char *name, uint32_t id)
{
    if (!page) return;
    memset(page, 0, sizeof(ui_page_t));
    page->name = name;
    page->id = id;
    page->content_rect.x = 0;
    page->content_rect.y = 0;
    page->content_rect.w = UI_SCREEN_WIDTH;
    page->content_rect.h = UI_SCREEN_HEIGHT;
    page->flags = UI_PAGE_FLAG_FULLSCREEN;
}

void ui_page_set_widgets(ui_page_t *page, ui_widget_t **widgets, uint16_t count)
{
    if (!page) return;
    page->widgets = widgets;
    page->widget_count = count;
}

void ui_page_set_callbacks(ui_page_t *page, ui_page_enter_cb_t enter, ui_page_exit_cb_t exit,
                           ui_page_draw_cb_t draw, ui_page_back_cb_t back)
{
    if (!page) return;
    page->on_enter = enter;
    page->on_exit = exit;
    page->on_draw = draw;
    page->on_back = back;
}
