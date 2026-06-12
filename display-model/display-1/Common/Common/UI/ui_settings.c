/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_settings.c
* Author             : LCD Model Team
* Version            : V3.0.0
* Date               : 2026/06/03
* Description        : Settings page implementation.
*                      Module-based tabs with config get/save via CLI passthrough.
********************************************************************************/
#include "ui_settings.h"
#include "ui_main.h"
#include "../SSD1963/ssd1963.h"
#include "../settings.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdlib.h>

/*=============================================================================
 *  Layout Constants
 *=============================================================================*/

#define SETTINGS_LEFT           20
#define SETTINGS_TOP            56
#define SETTINGS_ITEM_H         50
#define SETTINGS_MAX_ITEMS      8
#define SETTINGS_TAB_COUNT      5

#define TAB_BAR_H               36
#define CONTENT_TOP             (SETTINGS_TOP + TAB_BAR_H + 8)
#define CONTENT_LEFT            (SIDEBAR_WIDTH + SETTINGS_LEFT)
#define CONTENT_W               (UI_SCREEN_WIDTH - SIDEBAR_WIDTH - 2 * SETTINGS_LEFT)

/*=============================================================================
 *  Config Item Definition
 *=============================================================================*/

typedef struct {
    const char *module_key;     /* e.g. "0000" */
    const char *key;            /* e.g. "volume" */
    const char *label;          /* e.g. "Volume" */
    int16_t min;
    int16_t max;
    int16_t default_val;
} cfg_item_def_t;

typedef struct {
    const char *tab_label;
    uint8_t item_count;
    const cfg_item_def_t *items;
} cfg_tab_def_t;

/*=============================================================================
 *  Module Config Definitions (mirrors config_defaults[] in config.c)
 *=============================================================================*/

static const cfg_item_def_t s_core_items[] = {
    { "0000", "volume",         "Volume",           0, 100, 50 },
    { "0000", "volume_step",    "Vol Step",          1, 20,  5 },
    { "0000", "screen_timeout", "Screen Off(min)",   0, 60,  30 },
};

static const cfg_item_def_t s_display_items[] = {
    { "0101", "brightness",     "Brightness",        0, 100, 80 },
    { "0101", "rotation",       "Rotation",          0, 270, 0 },
};

static const cfg_item_def_t s_keyboard_items[] = {
    { "0401", "backlight",      "Main Backlight",    0, 100, 50 },
    { "0402", "backlight",      "Game Backlight",    0, 100, 50 },
    { "0403", "backlight",      "Music Backlight",   0, 100, 50 },
};

static const cfg_item_def_t s_submodel_items[] = {
    { "0502", "monitor_interval","Health Interval",  1, 60,  10 },
    { "0505", "rgb_mode",       "RGB Mode",          0, 3,   1 },
    { "0505", "rgb_brightness", "RGB Brightness",    0, 100, 80 },
    { "0505", "rgb_speed",      "RGB Speed",         0, 100, 50 },
    { "0504", "sensitivity",    "Touch Sensitivity", 0, 100, 50 },
    { "0506", "detect_threshold","IR Threshold",     0, 100, 50 },
    { "0507", "brightness",     "SubDisp Bright",    0, 100, 80 },
    { "0507", "content_mode",   "SubDisp Mode",      0, 10,  0 },
};

static const cfg_item_def_t s_system_items[] = {
    { "0201", "discoverable",   "BT Discoverable",   0, 1,   0 },
    { "0301", "report_interval","Pwr Report(s)",     1, 60,  10 },
};

static const cfg_tab_def_t s_tabs[SETTINGS_TAB_COUNT] = {
    { "Core",      3, s_core_items      },
    { "Display",   2, s_display_items   },
    { "Keyboard",  3, s_keyboard_items  },
    { "Submodels", 8, s_submodel_items  },
    { "System",    2, s_system_items    },
};

/*=============================================================================
 *  Unique Module Keys (for sequential config get fetching)
 *=============================================================================*/

#define CFG_MODULE_COUNT 12

static const char * const s_cfg_modules[CFG_MODULE_COUNT] = {
    "0000", "0101", "0201", "0301", "0401", "0402",
    "0403", "0502", "0504", "0505", "0506", "0507"
};

static bool s_module_fetched[CFG_MODULE_COUNT];

static int find_module_index(const char *mk)
{
    for (int i = 0; i < CFG_MODULE_COUNT; i++) {
        if (strcmp(s_cfg_modules[i], mk) == 0) return i;
    }
    return -1;
}

static bool is_tab_fetched(uint8_t tab_idx)
{
    if (tab_idx >= SETTINGS_TAB_COUNT) return false;
    for (uint8_t i = 0; i < s_tabs[tab_idx].item_count; i++) {
        int mi = find_module_index(s_tabs[tab_idx].items[i].module_key);
        if (mi >= 0 && !s_module_fetched[mi]) return false;
    }
    return true;
}

/*=============================================================================
 *  Runtime State
 *=============================================================================*/

/* Per-tab, per-item values */
static int16_t s_tab_values[SETTINGS_TAB_COUNT][SETTINGS_MAX_ITEMS];

/* Which module we are currently fetching (-1 = idle) */
static int8_t s_fetch_module = -1;

/* CLI response handled via on_cli_complete callback */

/* Pending config set commands (queued during fetch to avoid interference) */
#define PENDING_CMD_MAX 8
static char s_pending_cmds[PENDING_CMD_MAX][64];
static uint8_t s_pending_count = 0;

/* Content scroll */
static int16_t s_scroll_y = 0;

/* Viewport for content area */
#define CONTENT_VIEW_TOP    CONTENT_TOP
#define CONTENT_VIEW_BOTTOM UI_SCREEN_HEIGHT

/* Shared viewport rect for render push_target clipping.
 * Controls (slider/switch) are clipped to this rect to prevent
 * drawing into the tab bar area. */
static const ui_rect_t s_viewport_rect = {
    CONTENT_LEFT, CONTENT_VIEW_TOP,
    CONTENT_W, CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP
};

/* Invalidation rect covers from tab bar bottom to screen bottom.
 * This includes the gap below the tab bar and slider knob overflow
 * (knob extends ~6px beyond widget rect). */
static const ui_rect_t s_invalidate_rect = {
    CONTENT_LEFT, SETTINGS_TOP + TAB_BAR_H,
    CONTENT_W, UI_SCREEN_HEIGHT - (SETTINGS_TOP + TAB_BAR_H)
};

/* Invalidate content area including the gap below tab bar */
static void settings_invalidate_content(void)
{
    ui_page_invalidate(&s_invalidate_rect);
}

static void flush_pending_cmds(void)
{
    for (uint8_t i = 0; i < s_pending_count; i++) {
        UART_SendCLI(s_pending_cmds[i]);
    }
    s_pending_count = 0;
}

static bool all_modules_fetched(void)
{
    for (uint8_t m = 0; m < CFG_MODULE_COUNT; m++) {
        if (!s_module_fetched[m]) return false;
    }
    return true;
}

/*=============================================================================
 *  Widgets
 *=============================================================================*/

/* Formatted labels for each item slot (e.g. "Volume: 50") */
static char s_item_labels[SETTINGS_MAX_ITEMS][32];

static ui_label_t    lbl_title;
static ui_tabview_t  tab_modules;
static ui_button_t   btn_save;

/* Per-slot widgets (reconfigured when tab changes) */
static ui_list_item_t s_items[SETTINGS_MAX_ITEMS];
static ui_slider_t    s_sliders[SETTINGS_MAX_ITEMS];
static ui_switch_t    s_switches[SETTINGS_MAX_ITEMS];

/* About dialog */
static ui_dialog_t dlg_about;

/* Scroll background — transparent widget covering content viewport to capture
 * swipe events on empty space (below the last list item) */
static ui_widget_t s_scroll_bg;

/* Widget pointer array for the page */
static ui_widget_t *s_widgets[3 + SETTINGS_MAX_ITEMS + 1 + 1]; /* title+tab+save + items + about + scroll_bg */

/* About button (only shown on System tab) */
static ui_list_item_t item_about;
static ui_button_t    btn_about_info;

/*=============================================================================
 *  Forward Declarations
 *=============================================================================*/

static void settings_update_content(void);
static void settings_fetch_next(void);

/*=============================================================================
 *  Scroll Helpers
 *=============================================================================*/

/* Total content height for the active tab */
static int16_t settings_content_height(void)
{
    uint8_t tab_idx = ui_tabview_get_active(&tab_modules);
    if (tab_idx >= SETTINGS_TAB_COUNT) return 0;

    int16_t h = (int16_t)s_tabs[tab_idx].item_count * SETTINGS_ITEM_H;
    /* About item on System tab */
    if (tab_idx == 4) h += SETTINGS_ITEM_H;
    return h;
}

/* Maximum scroll offset */
static int16_t settings_max_scroll(void)
{
    int16_t total = settings_content_height();
    int16_t viewport = CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP;
    int16_t max = total - viewport;
    return max > 0 ? max : 0;
}

/* Clamp scroll_y and return true if it changed */
static bool settings_clamp_scroll(void)
{
    int16_t max = settings_max_scroll();
    int16_t old = s_scroll_y;
    if (s_scroll_y > max) s_scroll_y = max;
    if (s_scroll_y < 0) s_scroll_y = 0;
    return s_scroll_y != old;
}

/* Page-level event handler for key scrolling */
static bool settings_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;

    /* Key arrows */
    if (e->type == UI_EVENT_KEY_DOWN_ARROW) {
        s_scroll_y += SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return true;
    }
    if (e->type == UI_EVENT_KEY_UP_ARROW) {
        s_scroll_y -= SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return true;
    }

    return false;
}

/* Per-item event wrapper: intercepts swipe events for scrolling,
 * passes other events to the original list_item handler */
static ui_event_cb_t s_orig_item_event_cb[SETTINGS_MAX_ITEMS];

/* Per-item original draw callback */
static ui_draw_cb_t s_orig_item_draw_cb[SETTINGS_MAX_ITEMS];

/* Helper: intersect two rects, return true if they overlap */
static bool settings_rect_intersect(const ui_rect_t *a, const ui_rect_t *b, ui_rect_t *out)
{
    int16_t x1 = a->x > b->x ? a->x : b->x;
    int16_t y1 = a->y > b->y ? a->y : b->y;
    int16_t x2 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int16_t y2 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    if (x2 <= x1 || y2 <= y1) return false;
    out->x = x1; out->y = y1; out->w = x2 - x1; out->h = y2 - y1;
    return true;
}

/* Custom draw callback that clips list items to the content viewport */
static void settings_item_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    /* Clip the widget rect to the viewport before drawing */
    ui_rect_t clipped;
    ui_rect_t viewport = { CONTENT_LEFT, CONTENT_VIEW_TOP,
                           CONTENT_W, CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP };

    if (!settings_rect_intersect(&w->rect, &viewport, &clipped)) return;

    /* Draw the clipped background */
    ui_draw_fill_rect(&clipped, w->bg_color);

    /* Draw title text (clipped to viewport) */
    ui_list_item_t *item = (ui_list_item_t *)w;
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
        /* Clip text rect to viewport */
        ui_rect_t text_clip;
        if (settings_rect_intersect(&text_rect, &viewport, &text_clip)) {
            ui_draw_text_in_rect(&text_clip, item->title, item->font, item->text_color, 0);
        }
    }

    /* Draw control (slider/switch) if visible — push narrowed render target
     * so control drawing is clipped to the content viewport, preventing
     * slider knobs etc. from overflowing into the tab bar area. */
    if (item->control && (item->control->flags & UI_WIDGET_FLAG_VISIBLE)) {
        ui_render_push_target(&s_viewport_rect);
        ui_widget_draw(item->control, dirty);
        ui_render_pop_target();
    }

    /* Draw divider */
    if (item->show_divider) {
        ui_rect_t div = { w->rect.x + 10, w->rect.y + w->rect.h - 1, w->rect.w - 20, 1 };
        ui_rect_t div_clip;
        if (settings_rect_intersect(&div, &viewport, &div_clip)) {
            ui_draw_fill_rect(&div_clip, item->divider_color);
        }
    }
}

static void settings_item_event_cb(ui_widget_t *w, ui_event_t *e)
{
    /* Swipe: scroll content */
    if (e->type == UI_EVENT_SWIPE_UP) {
        s_scroll_y += SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_scroll_y -= SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return;
    }

    /* Mouse wheel */
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        s_scroll_y -= e->scroll_delta * SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return;
    }

    /* All other events: forward to the original list_item handler */
    uint8_t idx = (uint8_t)(uintptr_t)w->user_data;
    if (idx < SETTINGS_MAX_ITEMS && s_orig_item_event_cb[idx]) {
        s_orig_item_event_cb[idx](w, e);
    }
}

/* Background scroll area event handler — captures swipes on empty space */
static void scroll_bg_event_cb(ui_widget_t *w, ui_event_t *e)
{
    (void)w;

    if (e->type == UI_EVENT_SWIPE_UP) {
        s_scroll_y += SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_scroll_y -= SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return;
    }
    if (e->type == UI_EVENT_MOVE && e->scroll_delta != 0) {
        s_scroll_y -= e->scroll_delta * SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return;
    }
}

/* Custom draw callback for About list item — clips all drawing to the
 * content viewport to prevent overflow into the tab bar area. */
static void about_item_draw_cb(ui_widget_t *w, ui_rect_t *dirty)
{
    ui_render_push_target(&s_viewport_rect);

    /* Draw background */
    ui_draw_fill_rect(&w->rect, w->bg_color);

    /* Draw title text */
    ui_list_item_t *item = (ui_list_item_t *)w;
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

    /* Draw control (Info button) with viewport clipping */
    if (item->control && (item->control->flags & UI_WIDGET_FLAG_VISIBLE)) {
        ui_widget_draw(item->control, dirty);
    }

    ui_render_pop_target();
}

/*=============================================================================
 *  CLI Response Parsing
 *=============================================================================*/

static void settings_on_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    (void)tag;
    if (s_fetch_module < 0 || s_fetch_module >= CFG_MODULE_COUNT) {
        s_fetch_module = -1;
        return;
    }

    const char *cur_mk = s_cfg_modules[s_fetch_module];
    bool found_any = false;

    /* Parse lines like "volume:50\r\n" */
    /* Work on a mutable copy since we modify delimiters */
    char parse_buf[512];
    uint16_t copy_len = (len < sizeof(parse_buf) - 1) ? len : (uint16_t)(sizeof(parse_buf) - 1);
    memcpy(parse_buf, buf, copy_len);
    parse_buf[copy_len] = '\0';

    char *line = parse_buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        char *cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            const char *key = line;
            int val = atoi(colon + 1);

            for (uint8_t t = 0; t < SETTINGS_TAB_COUNT; t++) {
                for (uint8_t i = 0; i < s_tabs[t].item_count; i++) {
                    if (strcmp(s_tabs[t].items[i].module_key, cur_mk) == 0 &&
                        strcmp(s_tabs[t].items[i].key, key) == 0) {
                        s_tab_values[t][i] = (int16_t)val;
                        found_any = true;
                    }
                }
            }
        }

        if (nl) line = nl + 1;
        else break;
    }

    if (found_any) {
        s_module_fetched[s_fetch_module] = true;
    }
    s_fetch_module = -1;

    uint8_t active = ui_tabview_get_active(&tab_modules);
    if (is_tab_fetched(active)) {
        settings_update_content();
    }

    settings_fetch_next();

    if (all_modules_fetched()) {
        flush_pending_cmds();
    }
}

/*=============================================================================
 *  Config Fetch (sequential, one module at a time)
 *=============================================================================*/

static void settings_fetch_next(void)
{
    for (uint8_t m = 0; m < CFG_MODULE_COUNT; m++) {
        if (!s_module_fetched[m]) {
            char cmd[32];
            snprintf(cmd, sizeof(cmd), "config get %s", s_cfg_modules[m]);
            s_fetch_module = (int8_t)m;
            UART_SendCLI(cmd);
            return;
        }
    }
    /* All modules fetched */
}

/*=============================================================================
 *  Slider / Switch Change Callbacks
 *=============================================================================*/

static void slider_change(ui_widget_t *w, int16_t value)
{
    uint8_t idx = (uint8_t)(uintptr_t)w->user_data;
    uint8_t tab_idx = ui_tabview_get_active(&tab_modules);
    if (tab_idx >= SETTINGS_TAB_COUNT || idx >= s_tabs[tab_idx].item_count) return;

    const cfg_item_def_t *item = &s_tabs[tab_idx].items[idx];
    s_tab_values[tab_idx][idx] = value;

    /* Build config set command */
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "config set %s %s %d",
             item->module_key, item->key, value);

    /* Queue if still fetching, send directly otherwise */
    if (!all_modules_fetched() && s_pending_count < PENDING_CMD_MAX) {
        strncpy(s_pending_cmds[s_pending_count], cmd, 63);
        s_pending_cmds[s_pending_count][63] = '\0';
        s_pending_count++;
    } else {
        UART_SendCLI(cmd);
    }

    /* Apply locally for display brightness */
    if (strcmp(item->module_key, "0101") == 0 && strcmp(item->key, "brightness") == 0) {
        g_settings.backlight = (uint8_t)((uint16_t)value * 255 / 100);
        SSD1963_SetBacklight(g_settings.backlight);
    }

    /* Update label with new value */
    snprintf(s_item_labels[idx], sizeof(s_item_labels[idx]),
             "%s: %d", item->label, value);
    ui_widget_invalidate(&s_items[idx].base);
}

static void switch_toggle(ui_widget_t *w, bool state)
{
    uint8_t idx = (uint8_t)(uintptr_t)w->user_data;
    uint8_t tab_idx = ui_tabview_get_active(&tab_modules);
    if (tab_idx >= SETTINGS_TAB_COUNT || idx >= s_tabs[tab_idx].item_count) return;

    const cfg_item_def_t *item = &s_tabs[tab_idx].items[idx];
    s_tab_values[tab_idx][idx] = state ? 1 : 0;

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "config set %s %s %d",
             item->module_key, item->key, state ? 1 : 0);

    /* Queue if still fetching, send directly otherwise */
    if (!all_modules_fetched() && s_pending_count < PENDING_CMD_MAX) {
        strncpy(s_pending_cmds[s_pending_count], cmd, 63);
        s_pending_cmds[s_pending_count][63] = '\0';
        s_pending_count++;
    } else {
        UART_SendCLI(cmd);
    }

    /* Update label with new value */
    snprintf(s_item_labels[idx], sizeof(s_item_labels[idx]),
             "%s: %d", item->label, state ? 1 : 0);
    ui_widget_invalidate(&s_items[idx].base);
}

/*=============================================================================
 *  Save Button
 *=============================================================================*/

static void save_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("config save");
}

/*=============================================================================
 *  About Dialog
 *=============================================================================*/

static void about_click(ui_widget_t *w)
{
    (void)w;
    ui_dialog_show(&dlg_about, "About",
                   "WCH Model Terminal\n2026@ELAB\nShen Xingyi\nZhang ZhiCheng\nHuang Yuchun",
                   NULL, NULL);
}

/*=============================================================================
 *  Tab Change Handler
 *=============================================================================*/

static void tab_change_cb(ui_widget_t *w, uint8_t tab)
{
    (void)w;
    (void)tab;
    s_scroll_y = 0;
    settings_update_content();
}

/*=============================================================================
 *  Update Content Area (reconfigure slot widgets for active tab)
 *=============================================================================*/

static void settings_update_content(void)
{
    uint8_t tab_idx = ui_tabview_get_active(&tab_modules);
    if (tab_idx >= SETTINGS_TAB_COUNT) return;

    const cfg_tab_def_t *tab = &s_tabs[tab_idx];
    bool fetched = is_tab_fetched(tab_idx);
    int16_t y = CONTENT_TOP - s_scroll_y;

    /* Background scroll area (lowest Z-order in content, so list items are hit first) */
    ui_rect_t bg_rect = { CONTENT_LEFT, CONTENT_VIEW_TOP,
                          CONTENT_W, CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP };
    ui_widget_set_rect(&s_scroll_bg, &bg_rect);
    s_scroll_bg.flags |= UI_WIDGET_FLAG_VISIBLE;
    s_widgets[0] = &s_scroll_bg;

    uint16_t widx = 1;

    for (uint8_t i = 0; i < SETTINGS_MAX_ITEMS; i++) {
        if (i < tab->item_count) {
            const cfg_item_def_t *item = &tab->items[i];
            int16_t val = fetched ? s_tab_values[tab_idx][i] : item->default_val;

            /* Check if item is within viewport */
            bool in_view = (y + SETTINGS_ITEM_H > CONTENT_VIEW_TOP &&
                            y < CONTENT_VIEW_BOTTOM);

            if (in_view) {
                /* List item */
                ui_rect_t item_rect = { CONTENT_LEFT, y, CONTENT_W, SETTINGS_ITEM_H };
                ui_widget_set_rect(&s_items[i].base, &item_rect);
                snprintf(s_item_labels[i], sizeof(s_item_labels[i]),
                         "%s: %d", item->label, val);
                s_items[i].title = s_item_labels[i];
                s_items[i].show_divider = (i < tab->item_count - 1);
                s_items[i].base.flags |= UI_WIDGET_FLAG_VISIBLE;

                /* Use switch for boolean items (min=0, max=1), slider for others */
                if (item->max == 1 && item->min == 0) {
                    /* Switch */
                    ui_rect_t sw_rect = { CONTENT_LEFT + CONTENT_W - 60, y + 10, 50, 30 };
                    ui_widget_set_rect(&s_switches[i].base, &sw_rect);
                    ui_switch_set_state(&s_switches[i], val != 0);
                    s_switches[i].base.flags |= UI_WIDGET_FLAG_VISIBLE;
                    s_switches[i].base.user_data = (void*)(uintptr_t)i;
                    ui_list_item_set_control(&s_items[i], (ui_widget_t *)&s_switches[i]);

                    /* Hide slider */
                    s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                } else {
                    /* Slider */
                    ui_rect_t sl_rect = { CONTENT_LEFT + CONTENT_W - 160, y + 15, 150, 20 };
                    ui_slider_init(&s_sliders[i], &sl_rect, item->min, item->max, val);
                    s_sliders[i].base.user_data = (void*)(uintptr_t)i;
                    s_sliders[i].base.flags |= UI_WIDGET_FLAG_VISIBLE;
                    ui_slider_set_callback(&s_sliders[i], slider_change);
                    ui_list_item_set_control(&s_items[i], (ui_widget_t *)&s_sliders[i]);

                    /* Hide switch */
                    s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                }

                s_widgets[widx++] = (ui_widget_t *)&s_items[i];
            } else {
                /* Outside viewport — hide */
                s_items[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
            }

            y += SETTINGS_ITEM_H;
        } else {
            /* Hide unused slots */
            s_items[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
            s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
            s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
        }
    }

    /* Show About item only on System tab */
    if (tab_idx == 4) {
        bool about_visible = (y + SETTINGS_ITEM_H > CONTENT_VIEW_TOP &&
                              y < CONTENT_VIEW_BOTTOM);
        if (about_visible) {
            ui_rect_t about_rect = { CONTENT_LEFT, y, CONTENT_W, SETTINGS_ITEM_H };
            ui_widget_set_rect(&item_about.base, &about_rect);
            item_about.base.flags |= UI_WIDGET_FLAG_VISIBLE;
            s_widgets[widx++] = (ui_widget_t *)&item_about;
        } else {
            item_about.base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
        }
    } else {
        item_about.base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
    }

    /* Title, tabview, save button — drawn on top of list items so they
     * are never obscured by scrolling content */
    s_widgets[widx++] = (ui_widget_t *)&lbl_title;
    s_widgets[widx++] = (ui_widget_t *)&tab_modules;
    s_widgets[widx++] = (ui_widget_t *)&btn_save;

    ui_page_set_widgets(&page_settings, s_widgets, widx);
    settings_invalidate_content();
}

/*=============================================================================
 *  Settings Page Enter / Exit
 *=============================================================================*/

void ui_settings_enter(ui_page_t *page)
{
    (void)page;

    /* Register CLI response callback */
    uart_cli_cb_t cb = {
        .on_cli_complete = settings_on_cli_complete,
    };
    UART_SetCLICallbacks(&cb);

    /* Reset fetch state */
    memset(s_module_fetched, 0, sizeof(s_module_fetched));
    s_fetch_module = -1;
    s_pending_count = 0;
    s_scroll_y = 0;

    /* Initialize values with defaults */
    for (uint8_t t = 0; t < SETTINGS_TAB_COUNT; t++) {
        for (uint8_t i = 0; i < s_tabs[t].item_count && i < SETTINGS_MAX_ITEMS; i++) {
            s_tab_values[t][i] = s_tabs[t].items[i].default_val;
        }
    }

    /* Start fetching config for all modules */
    settings_fetch_next();

    /* Show content for the first tab */
    settings_update_content();
}

static void ui_settings_exit(ui_page_t *page)
{
    (void)page;

    /* Flush any pending config set commands before leaving */
    if (s_pending_count > 0) {
        flush_pending_cmds();
    }

    /* Clear our callback */
    UART_ClearCLICallbacks();
    s_fetch_module = -1;
}

/*=============================================================================
 *  Settings Page Initialization
 *=============================================================================*/

void ui_settings_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + SETTINGS_LEFT;
    int16_t content_w = UI_SCREEN_WIDTH - SIDEBAR_WIDTH - 2 * SETTINGS_LEFT;

    /* Title */
    ui_rect_t title_rect = { cx, 16, 200, 28 };
    ui_label_init(&lbl_title, &title_rect, "Settings", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    /* Tab view */
    ui_rect_t tv_rect = { cx, SETTINGS_TOP, content_w, TAB_BAR_H };
    ui_tabview_init(&tab_modules, &tv_rect, SETTINGS_TAB_COUNT);
    for (uint8_t i = 0; i < SETTINGS_TAB_COUNT; i++) {
        ui_tabview_set_label(&tab_modules, i, s_tabs[i].tab_label);
    }
    ui_tabview_set_active(&tab_modules, 0);
    ui_tabview_set_callback(&tab_modules, tab_change_cb);

    /* Save button */
    ui_rect_t save_rect = { cx + content_w - 90, 14, 80, 32 };
    ui_button_init(&btn_save, &save_rect, "Save", &font_montserrat_12);
    ui_button_set_colors(&btn_save, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    ui_button_set_callback(&btn_save, save_click);

    /* Initialize slot widgets */
    for (uint8_t i = 0; i < SETTINGS_MAX_ITEMS; i++) {
        ui_rect_t r = { 0, 0, 0, 0 };
        ui_list_item_init(&s_items[i], &r, "", &font_montserrat_12);
        s_items[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;

        /* Save original event_cb and install our wrapper for swipe scrolling */
        s_orig_item_event_cb[i] = s_items[i].base.event_cb;
        s_items[i].base.event_cb = settings_item_event_cb;

        /* Save original draw_cb and install our wrapper for viewport clipping */
        s_orig_item_draw_cb[i] = s_items[i].base.draw_cb;
        s_items[i].base.draw_cb = settings_item_draw_cb;

        s_items[i].base.user_data = (void*)(uintptr_t)i;

        ui_slider_init(&s_sliders[i], &r, 0, 100, 0);
        ui_slider_set_callback(&s_sliders[i], slider_change);
        s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;

        ui_switch_init(&s_switches[i], &r, false);
        ui_switch_set_callback(&s_switches[i], switch_toggle);
        s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
    }

    /* About item */
    ui_rect_t about_rect = { 0, 0, 0, 0 };
    ui_list_item_init(&item_about, &about_rect, "About", &font_montserrat_12);
    item_about.show_divider = false;
    item_about.base.draw_cb = about_item_draw_cb;
    item_about.base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
    ui_rect_t btn_about_rect = { 0, 0, 80, 34 };
    ui_button_init(&btn_about_info, &btn_about_rect, "Info", &font_montserrat_12);
    ui_button_set_callback(&btn_about_info, about_click);
    ui_button_set_colors(&btn_about_info, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    ui_list_item_set_control(&item_about, (ui_widget_t *)&btn_about_info);

    /* Dialog */
    ui_dialog_init(&dlg_about);

    /* Background scroll area */
    ui_widget_init(&s_scroll_bg, &(ui_rect_t){0, 0, 0, 0});
    s_scroll_bg.event_cb = scroll_bg_event_cb;
    s_scroll_bg.flags &= ~UI_WIDGET_FLAG_VISIBLE;

    /* Initial widget array (will be rebuilt in settings_update_content) */
    s_widgets[0] = &s_scroll_bg;

    ui_page_set_widgets(&page_settings, s_widgets, 1);
    ui_page_set_callbacks(&page_settings, ui_settings_enter, ui_settings_exit, NULL, NULL);
    ui_page_set_event_cb(&page_settings, settings_page_event);
}
