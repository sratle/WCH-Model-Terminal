/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_page.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : MiniUI page manager for E-ink display.
*                      Adapted from Display-1: compositing writes to 1bpp
*                      frame buffer, then flushes dirty regions via
*                      Epaper_PartialRefresh for fast partial updates.
********************************************************************************/
#include "miniui_page.h"
#include "miniui_widget.h"
#include "miniui_input.h"
#include "miniui_render.h"
#include "../Eink/epaper.h"
#include "../Touch/touch_matrix.h"
#include "debug.h"
#include <string.h>

static int16_t s_sidebar_width = 0;  /* No sidebar on E-ink by default */

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

/* Align rect x and w to multiples of 8 (required by JD79686AB partial refresh) */
static void rect_align8(ui_rect_t *r)
{
    /* Round x down to multiple of 8 */
    int16_t x_aligned = r->x & ~7;
    /* Round (x + w) up to multiple of 8 */
    int16_t x2 = r->x + r->w;
    int16_t x2_aligned = (x2 + 7) & ~7;
    if (x2_aligned > UI_SCREEN_WIDTH) x2_aligned = UI_SCREEN_WIDTH;

    r->x = x_aligned;
    r->w = x2_aligned - x_aligned;
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

int16_t ui_page_get_sidebar_width(void)
{
    return s_sidebar_width;
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

    /* Fast-path: try merging with the last region first */
    if (s_dirty_list.count > 0) {
        ui_rect_t *last = &s_dirty_list.regions[s_dirty_list.count - 1];
        if (rects_overlap(&r, last)) {
            rect_merge(&r, last, last);
            rect_clamp_screen(last);
            return;
        }
    }

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
        printf("[page] dirty list full (%d), falling back to full screen\r\n", UI_MAX_DIRTY_REGIONS);
        s_dirty_list.regions[0].x = 0;
        s_dirty_list.regions[0].y = 0;
        s_dirty_list.regions[0].w = UI_SCREEN_WIDTH;
        s_dirty_list.regions[0].h = UI_SCREEN_HEIGHT;
        s_dirty_list.count = 1;
    }
}

void ui_page_invalidate_all(void)
{
    printf("[page] invalidate_all (full screen)\r\n");
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
 *  E-ink Partial Refresh Helper
 *
 *  Send a dirty rectangular region to the e-paper driver for partial refresh.
 *  Reads directly from the frame buffers — no intermediate extraction buffer
 *  needed, so there is no size limit on the region (no fallback to full
 *  refresh). The driver handles data inversion and the PON/PDRF/POF sequence.
 *=============================================================================*/

static void flush_dirty_region_to_epd(const ui_rect_t *dirty)
{
    ui_rect_t r = *dirty;
    rect_clamp_screen(&r);
    rect_align8(&r);  /* X and W must be multiples of 8 */

    if (r.w <= 0 || r.h <= 0) return;

    uint8_t *frame_new = ui_render_get_frame_buf();
    uint8_t *frame_old = ui_render_get_old_buf();
    uint16_t row_bytes  = r.w / 8;
    int16_t  start_byte = r.x / 8;

    /* Quick check: skip refresh if old and new data are identical in this
     * region (avoids unnecessary e-ink updates). */
    bool has_diff = false;
    for (int16_t row = 0; row < r.h; row++) {
        int16_t y = r.y + row;
        if (y < 0 || y >= UI_SCREEN_HEIGHT) continue;
        uint8_t *sn = &frame_new[(uint32_t)y * UI_SCREEN_ROW_BYTES + start_byte];
        uint8_t *so = &frame_old[(uint32_t)y * UI_SCREEN_ROW_BYTES + start_byte];
        if (memcmp(so, sn, row_bytes) != 0) {
            has_diff = true;
            break;
        }
    }
    if (!has_diff) return;

    /* Send partial refresh directly from frame buffers.
     * The driver reads the rectangular region using the frame stride
     * (EPD_ROW_BYTES) and inverts data to match the panel pixel format. */
    printf("[page] partial refresh: (%d,%d,%d,%d) %u bytes\r\n",
           r.x, r.y, r.w, r.h, (uint32_t)row_bytes * r.h);
    Epaper_PartialRefresh(r.x, r.y, r.w, r.h, frame_old, frame_new);

    /* Update old buffer to match new for the refreshed region, so the next
     * partial refresh has the correct "old" reference data. */
    for (int16_t row = 0; row < r.h; row++) {
        int16_t y = r.y + row;
        if (y < 0 || y >= UI_SCREEN_HEIGHT) continue;
        uint8_t *dst_old = &frame_old[(uint32_t)y * UI_SCREEN_ROW_BYTES + start_byte];
        uint8_t *src_new = &frame_new[(uint32_t)y * UI_SCREEN_ROW_BYTES + start_byte];
        memcpy(dst_old, src_new, row_bytes);
    }
}

/*=============================================================================
 *  Page Drawing — Batched Compositing Renderer for E-ink
 *
 *  For each dirty rect, process rows in batches of UI_COMPOSE_BATCH:
 *    1. Set compositing target to batch rectangle
 *    2. Fill background for entire batch
 *    3. Draw page on_draw callback
 *    4. Draw all visible widgets intersecting this batch
 *    5. Flush batch to frame buffer
 *
 *  After all dirty rects are composited, flush changed regions to e-paper.
 *=============================================================================*/

static inline bool widget_intersects_batch(const ui_widget_t *w,
                                            int16_t y_start, int16_t y_end)
{
    return (y_end > w->rect.y && y_start < w->rect.y + w->rect.h);
}

void ui_page_draw(void)
{
    if (s_dirty_list.count == 0) return;

    ui_page_t *page = ui_page_current();
    if (!page) return;

    bool is_fullscreen = (page->flags & UI_PAGE_FLAG_FULLSCREEN) != 0;

    for (uint16_t i = 0; i < s_dirty_list.count; i++) {
        ui_rect_t *dirty = &s_dirty_list.regions[i];

        int16_t y_end = dirty->y + dirty->h;
        for (int16_t y = dirty->y; y < y_end; y += UI_COMPOSE_BATCH) {
            int16_t batch_h = y_end - y;
            if (batch_h > UI_COMPOSE_BATCH) batch_h = UI_COMPOSE_BATCH;

            ui_rect_t batch_target = { dirty->x, y, dirty->w, batch_h };
            ui_render_begin_rect(&batch_target);

            /* 1. Fill background (white for E-ink) */
            ui_render_fill_line_buf(dirty->x, dirty->w, UI_COLOR_WHITE);

            /* 2. Sidebar (if non-fullscreen and batch intersects sidebar area) */
            if (s_sidebar_draw && !is_fullscreen && dirty->x < s_sidebar_width) {
                s_sidebar_draw(&batch_target);
            }

            /* 3. Page on_draw callback */
            if (page->on_draw) {
                page->on_draw(page, &batch_target);
            }

            /* 4. Draw all visible widgets intersecting this batch */
            int16_t batch_end = y + batch_h;
            for (uint16_t j = 0; j < page->widget_count; j++) {
                ui_widget_t *w = page->widgets[j];
                if (w && (w->flags & UI_WIDGET_FLAG_VISIBLE)) {
                    if (widget_intersects_batch(w, y, batch_end)) {
                        ui_widget_draw(w, &batch_target);
                    }
                }
            }

            /* 5. Draw touch cursor overlay (topmost, if visible and intersects) */
            if (TouchMatrix_IsCursorVisible()) {
                int16_t cx = TouchMatrix_GetCursorX();
                int16_t cy = TouchMatrix_GetCursorY();
                /* Cursor is 10x13 (8x11 bitmap + 1px outline); check intersection */
                if (cx + 9 >= batch_target.x && cx - 1 < batch_target.x + batch_target.w &&
                    cy + 12 >= y && cy - 1 < batch_end) {
                    ui_draw_touch_cursor(cx, cy);
                }
            }

            /* 6. Flush batch to frame buffer */
            ui_render_flush_rows(y, batch_h, dirty->x, dirty->w);
        }
    }

    /* 6. Flush dirty regions to e-paper display via partial refresh */
    for (uint16_t i = 0; i < s_dirty_list.count; i++) {
        flush_dirty_region_to_epd(&s_dirty_list.regions[i]);
    }

    s_dirty_list.count = 0;
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

void ui_page_set_update_cb(ui_page_t *page, ui_page_update_cb_t update)
{
    if (!page) return;
    page->on_update = update;
}

void ui_page_set_event_cb(ui_page_t *page, ui_page_event_cb_t event_cb)
{
    if (!page) return;
    page->on_page_event = event_cb;
}
