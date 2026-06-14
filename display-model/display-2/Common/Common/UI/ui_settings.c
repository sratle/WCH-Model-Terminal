/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_settings.c
* Author             : E-ink Model Team
* Version            : V3.0.0
* Date               : 2026/06/12
* Description        : Settings page implementation.
*                      Module-based tabs with config get/save via CLI passthrough.
*                      Adapted from Display-1 for E-ink display:
*                      - Removed SSD1963 backlight dependency
*                      - Removed keyboard-specific settings (no keyboard sub-tabs)
*                      - Simplified for E-ink display constraints
********************************************************************************/
#include "ui_settings.h"
#include "ui_main.h"
#include "../settings.h"
#include "../UART/uart_module.h"
#include "../MiniUI/font/font_montserrat_12.h"
#include "../MiniUI/font/font_montserrat_16.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*=============================================================================
 *  Layout Constants
 *=============================================================================*/

#define SETTINGS_LEFT           20
#define SETTINGS_TOP            56
#define SETTINGS_ITEM_H         50
#define SETTINGS_MAX_ITEMS      8
#define SETTINGS_TAB_COUNT      4

#define TAB_BAR_H               36
#define CONTENT_TOP             (SETTINGS_TOP + TAB_BAR_H + 8)
#define CONTENT_LEFT            (SIDEBAR_WIDTH + SETTINGS_LEFT)
#define CONTENT_W               (UI_SCREEN_WIDTH - SIDEBAR_WIDTH - 2 * SETTINGS_LEFT)

/*=============================================================================
 *  Config Item Definition
 *=============================================================================*/

typedef struct {
    const char *module_key;
    const char *key;
    const char *label;
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
 *  Module Config Definitions
 *=============================================================================*/

static const cfg_item_def_t s_core_items[] = {
    { "0000", "volume",         "Volume",           0, 100, 50 },
    { "0000", "volume_step",    "Vol Step",          1, 20,  5 },
    { "0000", "screen_timeout", "Screen Off(min)",   0, 60,  30 },
};

static const cfg_item_def_t s_display_items[] = {
    { "0101", "rotation",       "Rotation",          0, 270, 0 },
};

static const cfg_item_def_t s_submodel_items[] = {
    { "0502", "monitor_interval","Health Interval",  1, 60,  10 },
    { "0505", "rgb_mode",       "RGB Mode",          0, 3,   1 },
    { "0505", "rgb_brightness", "RGB Brightness",    0, 100, 80 },
    { "0505", "rgb_speed",      "RGB Speed",         0, 100, 50 },
    { "0506", "detect_threshold","IR Threshold",     0, 100, 50 },
    { "0507", "content_mode",   "SubDisp Mode",      0, 10,  0 },
};

static const cfg_item_def_t s_system_items[] = {
    { "0201", "discoverable",   "BT Discoverable",   0, 1,   0 },
    { "0301", "report_interval","Pwr Report(s)",     1, 60,  10 },
};

static const cfg_tab_def_t s_tabs[SETTINGS_TAB_COUNT] = {
    { "Core",      3, s_core_items      },
    { "Display",   1, s_display_items   },
    { "Submodels", 6, s_submodel_items  },
    { "System",    2, s_system_items    },
};

/*=============================================================================
 *  Unique Module Keys
 *=============================================================================*/

#define CFG_MODULE_COUNT 8

static const char * const s_cfg_modules[CFG_MODULE_COUNT] = {
    "0000", "0101", "0201", "0301", "0502", "0505", "0506", "0507"
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

static int16_t s_tab_values[SETTINGS_TAB_COUNT][SETTINGS_MAX_ITEMS];
static int8_t s_fetch_module = -1;

#define PENDING_CMD_MAX 8
static char s_pending_cmds[PENDING_CMD_MAX][64];
static uint8_t s_pending_count = 0;

static int16_t s_scroll_y = 0;

#define CONTENT_VIEW_TOP    CONTENT_TOP
#define CONTENT_VIEW_BOTTOM UI_SCREEN_HEIGHT

static const ui_rect_t s_viewport_rect = {
    CONTENT_LEFT, CONTENT_VIEW_TOP,
    CONTENT_W, CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP
};

static const ui_rect_t s_invalidate_rect = {
    CONTENT_LEFT, SETTINGS_TOP + TAB_BAR_H,
    CONTENT_W, UI_SCREEN_HEIGHT - (SETTINGS_TOP + TAB_BAR_H)
};

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

static char s_item_labels[SETTINGS_MAX_ITEMS][32];

static ui_label_t    lbl_title;
static ui_tabview_t  tab_modules;
static ui_button_t   btn_save;

static ui_list_item_t s_items[SETTINGS_MAX_ITEMS];
static ui_slider_t    s_sliders[SETTINGS_MAX_ITEMS];
static ui_switch_t    s_switches[SETTINGS_MAX_ITEMS];

static ui_dialog_t dlg_about;

static ui_widget_t s_scroll_bg;

static ui_widget_t *s_widgets[3 + SETTINGS_MAX_ITEMS + 1 + 1];

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

static int16_t settings_content_height(void)
{
    uint8_t tab_idx = ui_tabview_get_active(&tab_modules);
    if (tab_idx >= SETTINGS_TAB_COUNT) return 0;
    int16_t h = (int16_t)s_tabs[tab_idx].item_count * SETTINGS_ITEM_H;
    if (tab_idx == 3) h += SETTINGS_ITEM_H; /* About item on System tab */
    return h;
}

static int16_t settings_max_scroll(void)
{
    int16_t total = settings_content_height();
    int16_t viewport = CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP;
    int16_t max = total - viewport;
    return max > 0 ? max : 0;
}

static bool settings_clamp_scroll(void)
{
    int16_t max = settings_max_scroll();
    int16_t old = s_scroll_y;
    if (s_scroll_y > max) s_scroll_y = max;
    if (s_scroll_y < 0) s_scroll_y = 0;
    return s_scroll_y != old;
}

static bool settings_page_event(ui_page_t *page, ui_event_t *e)
{
    (void)page;
    if (e->type == UI_EVENT_SWIPE_UP) {
        s_scroll_y += SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return true;
    }
    if (e->type == UI_EVENT_SWIPE_DOWN) {
        s_scroll_y -= SETTINGS_ITEM_H;
        settings_clamp_scroll();
        settings_update_content();
        return true;
    }
    return false;
}

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

    if (found_any) s_module_fetched[s_fetch_module] = true;
    s_fetch_module = -1;

    uint8_t active = ui_tabview_get_active(&tab_modules);
    if (is_tab_fetched(active)) settings_update_content();

    settings_fetch_next();

    if (all_modules_fetched()) flush_pending_cmds();
}

/*=============================================================================
 *  Config Fetch
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

    char cmd[64];
    snprintf(cmd, sizeof(cmd), "config set %s %s %d",
             item->module_key, item->key, value);

    if (!all_modules_fetched() && s_pending_count < PENDING_CMD_MAX) {
        strncpy(s_pending_cmds[s_pending_count], cmd, 63);
        s_pending_cmds[s_pending_count][63] = '\0';
        s_pending_count++;
    } else {
        UART_SendCLI(cmd);
    }

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

    if (!all_modules_fetched() && s_pending_count < PENDING_CMD_MAX) {
        strncpy(s_pending_cmds[s_pending_count], cmd, 63);
        s_pending_cmds[s_pending_count][63] = '\0';
        s_pending_count++;
    } else {
        UART_SendCLI(cmd);
    }

    snprintf(s_item_labels[idx], sizeof(s_item_labels[idx]),
             "%s: %d", item->label, state ? 1 : 0);
    ui_widget_invalidate(&s_items[idx].base);
}

/*=============================================================================
 *  Tab Change Callback
 *=============================================================================*/

static void tab_change_cb(ui_widget_t *w, uint8_t tab)
{
    (void)w;
    (void)tab;
    s_scroll_y = 0;
    settings_update_content();
}

/*=============================================================================
 *  Save / About Callbacks
 *=============================================================================*/

static void save_click(ui_widget_t *w)
{
    (void)w;
    UART_SendCLI("config save");
}

static void about_accept(void)
{
    /* Nothing to do */
}

static void about_click(ui_widget_t *w)
{
    (void)w;
    ui_dialog_show(&dlg_about, "About",
                   "E-ink Display Module\n"
                   "FW: 2.0\n"
                   "HW: E-ink 5.65\"",
                   about_accept, NULL);
}

/*=============================================================================
 *  Update Content
 *=============================================================================*/

static void settings_update_content(void)
{
    uint8_t tab_idx = ui_tabview_get_active(&tab_modules);
    if (tab_idx >= SETTINGS_TAB_COUNT) return;

    const cfg_tab_def_t *tab = &s_tabs[tab_idx];
    int16_t y = CONTENT_VIEW_TOP - s_scroll_y;
    uint16_t widx = 0;

    /* Scroll background */
    ui_rect_t bg_rect = {CONTENT_LEFT, CONTENT_VIEW_TOP, CONTENT_W,
                          CONTENT_VIEW_BOTTOM - CONTENT_VIEW_TOP};
    ui_widget_set_rect(&s_scroll_bg, &bg_rect);
    s_scroll_bg.flags |= UI_WIDGET_FLAG_VISIBLE;
    s_widgets[widx++] = &s_scroll_bg;

    for (uint8_t i = 0; i < SETTINGS_MAX_ITEMS; i++) {
        if (i < tab->item_count) {
            bool visible = (y + SETTINGS_ITEM_H > CONTENT_VIEW_TOP &&
                            y < CONTENT_VIEW_BOTTOM);

            if (visible) {
                int16_t val = s_tab_values[tab_idx][i];
                const cfg_item_def_t *item = &tab->items[i];

                ui_rect_t item_rect = {CONTENT_LEFT, y, CONTENT_W, SETTINGS_ITEM_H};
                ui_widget_set_rect(&s_items[i].base, &item_rect);

                snprintf(s_item_labels[i], sizeof(s_item_labels[i]),
                         "%s: %d", item->label, val);
                s_items[i].title = s_item_labels[i];
                s_items[i].show_divider = (i < tab->item_count - 1);
                s_items[i].base.flags |= UI_WIDGET_FLAG_VISIBLE;

                if (item->max == 1 && item->min == 0) {
                    ui_rect_t sw_rect = {CONTENT_LEFT + CONTENT_W - 60, y + 10, 50, 30};
                    ui_widget_set_rect(&s_switches[i].base, &sw_rect);
                    ui_switch_set_state(&s_switches[i], val != 0);
                    s_switches[i].base.flags |= UI_WIDGET_FLAG_VISIBLE;
                    s_switches[i].base.user_data = (void*)(uintptr_t)i;
                    ui_list_item_set_control(&s_items[i], (ui_widget_t *)&s_switches[i]);
                    s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                } else {
                    ui_rect_t sl_rect = {CONTENT_LEFT + CONTENT_W - 160, y + 15, 150, 20};
                    ui_widget_set_rect(&s_sliders[i].base, &sl_rect);
                    s_sliders[i].min = item->min;
                    s_sliders[i].max = item->max;
                    s_sliders[i].value = val;
                    s_sliders[i].base.user_data = (void*)(uintptr_t)i;
                    s_sliders[i].base.flags |= UI_WIDGET_FLAG_VISIBLE;
                    ui_list_item_set_control(&s_items[i], (ui_widget_t *)&s_sliders[i]);
                    s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                }

                s_widgets[widx++] = (ui_widget_t *)&s_items[i];
            } else {
                s_items[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
                s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
            }

            y += SETTINGS_ITEM_H;
        } else {
            s_items[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
            s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
            s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
        }
    }

    /* About item on System tab */
    if (tab_idx == 3) {
        bool about_visible = (y + SETTINGS_ITEM_H > CONTENT_VIEW_TOP &&
                              y < CONTENT_VIEW_BOTTOM);
        if (about_visible) {
            ui_rect_t about_rect = {CONTENT_LEFT, y, CONTENT_W, SETTINGS_ITEM_H};
            ui_widget_set_rect(&item_about.base, &about_rect);
            item_about.base.flags |= UI_WIDGET_FLAG_VISIBLE;
            s_widgets[widx++] = (ui_widget_t *)&item_about;
        } else {
            item_about.base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
        }
    } else {
        item_about.base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
    }

    s_widgets[widx++] = (ui_widget_t *)&lbl_title;
    s_widgets[widx++] = (ui_widget_t *)&tab_modules;
    s_widgets[widx++] = (ui_widget_t *)&btn_save;

    ui_page_set_widgets(&page_settings, s_widgets, widx);
    settings_invalidate_content();
}

/*=============================================================================
 *  Settings Page Enter / Exit
 *=============================================================================*/

static void ui_settings_enter(ui_page_t *page)
{
    (void)page;

    uart_cli_cb_t cb = {
        .on_cli_complete = settings_on_cli_complete,
    };
    UART_SetCLICallbacks(&cb);

    memset(s_module_fetched, 0, sizeof(s_module_fetched));
    s_fetch_module = -1;
    s_pending_count = 0;
    s_scroll_y = 0;

    for (uint8_t t = 0; t < SETTINGS_TAB_COUNT; t++) {
        for (uint8_t i = 0; i < s_tabs[t].item_count && i < SETTINGS_MAX_ITEMS; i++) {
            s_tab_values[t][i] = s_tabs[t].items[i].default_val;
        }
    }

    settings_fetch_next();
    settings_update_content();
}

static void ui_settings_exit(ui_page_t *page)
{
    (void)page;
    if (s_pending_count > 0) flush_pending_cmds();
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

    ui_rect_t title_rect = {cx, 16, 200, 28};
    ui_label_init(&lbl_title, &title_rect, "Settings", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    ui_rect_t tv_rect = {cx, SETTINGS_TOP, content_w, TAB_BAR_H};
    ui_tabview_init(&tab_modules, &tv_rect, SETTINGS_TAB_COUNT);
    for (uint8_t i = 0; i < SETTINGS_TAB_COUNT; i++) {
        ui_tabview_set_label(&tab_modules, i, s_tabs[i].tab_label);
    }
    ui_tabview_set_active(&tab_modules, 0);
    ui_tabview_set_callback(&tab_modules, tab_change_cb);

    ui_rect_t save_rect = {cx + content_w - 90, 14, 80, 32};
    ui_button_init(&btn_save, &save_rect, "Save", &font_montserrat_12);
    ui_button_set_colors(&btn_save, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    ui_button_set_callback(&btn_save, save_click);

    for (uint8_t i = 0; i < SETTINGS_MAX_ITEMS; i++) {
        ui_rect_t r = {0, 0, 0, 0};
        ui_list_item_init(&s_items[i], &r, "", &font_montserrat_12);
        s_items[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
        s_items[i].base.user_data = (void*)(uintptr_t)i;

        ui_slider_init(&s_sliders[i], &r, 0, 100, 0);
        ui_slider_set_callback(&s_sliders[i], slider_change);
        s_sliders[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;

        ui_switch_init(&s_switches[i], &r, false);
        ui_switch_set_callback(&s_switches[i], switch_toggle);
        s_switches[i].base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
    }

    ui_rect_t about_rect = {0, 0, 0, 0};
    ui_list_item_init(&item_about, &about_rect, "About", &font_montserrat_12);
    item_about.show_divider = false;
    item_about.base.flags &= ~UI_WIDGET_FLAG_VISIBLE;
    ui_rect_t btn_about_rect = {0, 0, 80, 34};
    ui_button_init(&btn_about_info, &btn_about_rect, "Info", &font_montserrat_12);
    ui_button_set_callback(&btn_about_info, about_click);
    ui_button_set_colors(&btn_about_info, UI_COLOR_PRIMARY, UI_COLOR_SECONDARY, UI_COLOR_WHITE);
    ui_list_item_set_control(&item_about, (ui_widget_t *)&btn_about_info);

    ui_dialog_init(&dlg_about);

    ui_widget_init(&s_scroll_bg, &(ui_rect_t){0, 0, 0, 0});
    s_scroll_bg.event_cb = scroll_bg_event_cb;
    s_scroll_bg.flags &= ~UI_WIDGET_FLAG_VISIBLE;

    s_widgets[0] = &s_scroll_bg;
    ui_page_set_widgets(&page_settings, s_widgets, 1);
    ui_page_set_callbacks(&page_settings, ui_settings_enter, ui_settings_exit, NULL, NULL);
    ui_page_set_event_cb(&page_settings, settings_page_event);
}
