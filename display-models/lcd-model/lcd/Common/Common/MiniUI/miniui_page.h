/********************************** (C) COPYRIGHT *******************************
* File Name          : miniui_page.h
* Author             : LCD Model Team
* Version            : V1.0.0
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
#include "miniui_widget.h"

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

/* Initialize page manager */
void ui_page_init(void);

/* Register a page */
void ui_page_register(ui_page_t *page);

/* Switch to a page (clears stack, pushes new page as root) */
void ui_page_switch(ui_page_t *page);

/* Push a page onto stack */
void ui_page_push(ui_page_t *page);

/* Pop current page from stack (go back) */
void ui_page_pop(void);

/* Check if we can go back */
bool ui_page_can_go_back(void);

/* Get current page */
ui_page_t* ui_page_current(void);

/* Mark a region as dirty */
void ui_page_invalidate(const ui_rect_t *rect);

/* Mark entire page as dirty */
void ui_page_invalidate_all(void);

/* Draw current page (process dirty regions) */
void ui_page_draw(void);

/*=============================================================================
 *  Page Helpers
 *=============================================================================*/

/* Initialize a page structure */
void ui_page_struct_init(ui_page_t *page, const char *name, uint32_t id);

/* Set page widgets */
void ui_page_set_widgets(ui_page_t *page, ui_widget_t **widgets, uint16_t count);

/* Set page callbacks */
void ui_page_set_callbacks(ui_page_t *page, ui_page_enter_cb_t enter, ui_page_exit_cb_t exit,
                           ui_page_draw_cb_t draw, ui_page_back_cb_t back);

#ifdef __cplusplus
}
#endif

#endif /* __MINIUI_PAGE_H */
