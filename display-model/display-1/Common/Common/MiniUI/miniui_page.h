/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_page.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2025/04/19
* Description        : MiniUI page manager API.
*                      Page stack, navigation, and dirty region tracking.
********************************************************************************/
#ifndef __MINIUI_PAGE_H
#define __MINIUI_PAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "miniui_types.h"

/*=============================================================================
 *  Page Stack Configuration
 *=============================================================================*/

#define UI_PAGE_STACK_DEPTH     16
#define UI_MAX_DIRTY_REGIONS    8

/*=============================================================================
 *  Page Structure
 *=============================================================================*/

struct ui_page {
    const char *name;
    uint32_t id;
    ui_rect_t content_rect;
    ui_widget_t **widgets;
    uint16_t widget_count;
    ui_page_enter_cb_t on_enter;
    ui_page_exit_cb_t on_exit;
    ui_page_draw_cb_t on_draw;
    ui_page_back_cb_t on_back;
    int16_t scroll_y;
    uint8_t flags;
};

/*=============================================================================
 *  Dirty Region Tracking
 *=============================================================================*/

typedef struct {
    ui_rect_t regions[UI_MAX_DIRTY_REGIONS];
    uint8_t count;
} ui_dirty_list_t;

/*=============================================================================
 *  Page Manager API
 *=============================================================================*/

void ui_page_init(void);
void ui_page_register(ui_page_t *page);
void ui_page_switch(ui_page_t *page);
void ui_page_push(ui_page_t *page);
void ui_page_pop(void);
bool ui_page_can_go_back(void);
ui_page_t* ui_page_current(void);
void ui_page_invalidate(const ui_rect_t *rect);
void ui_page_invalidate_all(void);
const ui_dirty_list_t *ui_page_get_dirty_list(void);
void ui_page_clear_dirty(void);
void ui_page_draw(void);

/*=============================================================================
 *  Sidebar Integration
 *=============================================================================*/

typedef void (*ui_sidebar_draw_cb_t)(ui_rect_t *dirty);
typedef bool (*ui_sidebar_event_cb_t)(ui_event_t *e);

void ui_page_set_sidebar_callbacks(ui_sidebar_draw_cb_t draw, ui_sidebar_event_cb_t event);
void ui_page_set_sidebar_width(int16_t width);
ui_sidebar_event_cb_t ui_page_get_sidebar_event_cb(void);

/*=============================================================================
 *  Page Helpers
 *=============================================================================*/

void ui_page_struct_init(ui_page_t *page, const char *name, uint32_t id);
void ui_page_struct_init_fullscreen(ui_page_t *page, const char *name, uint32_t id);
void ui_page_set_widgets(ui_page_t *page, ui_widget_t **widgets, uint16_t count);
void ui_page_set_callbacks(ui_page_t *page, ui_page_enter_cb_t enter, ui_page_exit_cb_t exit,
                           ui_page_draw_cb_t draw, ui_page_back_cb_t back);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_PAGE_H */
