/********************************** (C) COPYRIGHT *******************************
* File Name          : ui_models.c
* Author             : LCD Model Team
* Version            : V4.0.0
* Date               : 2026/05/25
* Description        : Models page implementation.
*                      Tab view with module status cards.
*                      V4.0: Dynamic module status from Core lsdev command.
********************************************************************************/
#include "ui_models.h"
#include "ui_main.h"
#include "../UART/uart_module.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*=============================================================================
 *  Module Status Data (dynamic, updated from lsdev response)
*=============================================================================*/

#define LSDEV_SLOT_COUNT  6

typedef struct {
    const char *name;       /* Fixed display name */
    const uint8_t *icon;
    int16_t icon_w;
    int16_t icon_h;
    bool online;
    uint8_t type;           /* Module type from lsdev */
    uint8_t subtype;        /* Module subtype from lsdev */
    char info[24];          /* Status string: "ONLINE"/"OFFLINE" + type info */
} module_info_t;

/* Submodel subtype name mapping */
static const char *subtype_name(uint8_t type, uint8_t subtype)
{
    if (type == MODULE_TYPE_SUBMODEL) {
        switch (subtype) {
        case SUBMODEL_FINGERPRINT: return "Finger";
        case SUBMODEL_HEALTH:      return "Health";
        case SUBMODEL_NFC:         return "NFC";
        case SUBMODEL_TOUCH_RING:  return "Touch";
        case SUBMODEL_RGB:         return "RGB";
        case SUBMODEL_INFRARED:    return "IR";
        case SUBMODEL_SUBDISPLAY:  return "SubDisp";
        default:                   return "Unknown";
        }
    }
    return NULL;
}

/* Core modules: Display, Keyboard, Power */
static module_info_t s_core_modules[3] = {
    {"Display",  icon_image_16_bitmap,     ICON_IMAGE_16_WIDTH,     ICON_IMAGE_16_HEIGHT,     false, 0, 0, "OFFLINE"},
    {"Keyboard", icon_keyboard_16_bitmap,  ICON_KEYBOARD_16_WIDTH,  ICON_KEYBOARD_16_HEIGHT,  false, 0, 0, "OFFLINE"},
    {"Power",    icon_power_16_bitmap,     ICON_POWER_16_WIDTH,     ICON_POWER_16_HEIGHT,     false, 0, 0, "OFFLINE"},
};

/* Submodel modules: Slot1, Slot2, Slot3 */
static module_info_t s_sub_modules[3] = {
    {"Slot 1",   icon_bars_16_bitmap,      ICON_BARS_16_WIDTH,      ICON_BARS_16_HEIGHT,      false, 0, 0, "OFFLINE"},
    {"Slot 2",   icon_bars_16_bitmap,      ICON_BARS_16_WIDTH,      ICON_BARS_16_HEIGHT,      false, 0, 0, "OFFLINE"},
    {"Slot 3",   icon_bars_16_bitmap,      ICON_BARS_16_WIDTH,      ICON_BARS_16_HEIGHT,      false, 0, 0, "OFFLINE"},
};

/* lsdev slot name to module index mapping:
 * lsdev outputs: Display(0), Keyboard(1), Power(2), Submodel1(3), Submodel2(4), Submodel3(5)
 * Core modules: 0-2, Sub modules: 3-5 */

/*=============================================================================
 *  Models Page Widgets
*=============================================================================*/

static ui_label_t lbl_title;
static ui_tabview_t tabview;

static ui_card_t card_core[3];
static ui_card_t card_sub[3];

static ui_label_t lbl_core_name[3];
static ui_label_t lbl_sub_name[3];

static ui_status_dot_t dot_core[3];
static ui_status_dot_t dot_sub[3];

static ui_label_t lbl_core_info[3];
static ui_label_t lbl_sub_info[3];

static ui_widget_t *s_models_widgets[6];

static bool s_models_inited = false;
static bool s_lsdev_pending = false;

/*=============================================================================
 *  Forward Declarations
*=============================================================================*/

static void update_models_widgets(void);
static void models_update_cards(void);

/*=============================================================================
 *  lsdev Response Parsing
*=============================================================================*/

/* Parse lsdev output and update module status.
 * lsdev output format (from Core CLI_Cmd_Lsdev):
 *   Module Status:
 *     Display     ONLINE   type=0x02  subtype=0x01  miss=0
 *     Keyboard    OFFLINE  type=0x00  subtype=0x00  miss=5
 *     Power       ONLINE   type=0x03  subtype=0x01  miss=0
 *     Submodel1   ONLINE   type=0x05  subtype=0x01  miss=0
 *     Submodel2   OFFLINE  type=0x00  subtype=0x00  miss=5
 *     Submodel3   OFFLINE  type=0x00  subtype=0x00  miss=5
 */
static void parse_lsdev_response(const char *output, uint16_t len)
{
    /* Make a null-terminated copy for parsing */
    char buf[512];
    uint16_t copy = len;
    if (copy > sizeof(buf) - 1) copy = sizeof(buf) - 1;
    memcpy(buf, output, copy);
    buf[copy] = '\0';

    /* Parse each line */
    char *line = strtok(buf, "\r\n");
    while (line) {
        /* Skip header line "Module Status:" */
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "Module Status:", 14) == 0) {
            line = strtok(NULL, "\r\n");
            continue;
        }

        /* Parse module line: "  DisplayName  STATUS  type=0xNN  subtype=0xNN  miss=N" */
        char name[16] = {0};
        char status[16] = {0};
        uint8_t type_val = 0, subtype_val = 0;

        /* Extract name (first non-space token) */
        int n = 0;
        while (*p == ' ' || *p == '\t') p++;
        while (*p && *p != ' ' && *p != '\t' && n < 15) {
            name[n++] = *p++;
        }
        name[n] = '\0';

        /* Extract status (next token: ONLINE/OFFLINE/UNKNOWN) */
        while (*p == ' ' || *p == '\t') p++;
        n = 0;
        while (*p && *p != ' ' && *p != '\t' && n < 15) {
            status[n++] = *p++;
        }
        status[n] = '\0';

        /* Extract type=0xNN */
        const char *type_str = strstr(p, "type=0x");
        if (type_str) {
            type_val = (uint8_t)strtol(type_str + 7, NULL, 16);
        }

        /* Extract subtype=0xNN */
        const char *subtype_str = strstr(p, "subtype=0x");
        if (subtype_str) {
            subtype_val = (uint8_t)strtol(subtype_str + 10, NULL, 16);
        }

        /* Map to module slot */
        bool is_online = (strcmp(status, "ONLINE") == 0);

        if (strcmp(name, "Display") == 0) {
            s_core_modules[0].online = is_online;
            s_core_modules[0].type = type_val;
            s_core_modules[0].subtype = subtype_val;
            snprintf(s_core_modules[0].info, sizeof(s_core_modules[0].info),
                     "%s", is_online ? "ONLINE" : "OFFLINE");
        } else if (strcmp(name, "Keyboard") == 0) {
            s_core_modules[1].online = is_online;
            s_core_modules[1].type = type_val;
            s_core_modules[1].subtype = subtype_val;
            snprintf(s_core_modules[1].info, sizeof(s_core_modules[1].info),
                     "%s", is_online ? "ONLINE" : "OFFLINE");
        } else if (strcmp(name, "Power") == 0) {
            s_core_modules[2].online = is_online;
            s_core_modules[2].type = type_val;
            s_core_modules[2].subtype = subtype_val;
            snprintf(s_core_modules[2].info, sizeof(s_core_modules[2].info),
                     "%s", is_online ? "ONLINE" : "OFFLINE");
        } else if (strcmp(name, "Submodel1") == 0) {
            s_sub_modules[0].online = is_online;
            s_sub_modules[0].type = type_val;
            s_sub_modules[0].subtype = subtype_val;
            if (is_online) {
                const char *sub = subtype_name(type_val, subtype_val);
                if (sub)
                    snprintf(s_sub_modules[0].info, sizeof(s_sub_modules[0].info), "%s", sub);
                else
                    snprintf(s_sub_modules[0].info, sizeof(s_sub_modules[0].info), "ONLINE");
            } else {
                snprintf(s_sub_modules[0].info, sizeof(s_sub_modules[0].info), "OFFLINE");
            }
        } else if (strcmp(name, "Submodel2") == 0) {
            s_sub_modules[1].online = is_online;
            s_sub_modules[1].type = type_val;
            s_sub_modules[1].subtype = subtype_val;
            if (is_online) {
                const char *sub = subtype_name(type_val, subtype_val);
                if (sub)
                    snprintf(s_sub_modules[1].info, sizeof(s_sub_modules[1].info), "%s", sub);
                else
                    snprintf(s_sub_modules[1].info, sizeof(s_sub_modules[1].info), "ONLINE");
            } else {
                snprintf(s_sub_modules[1].info, sizeof(s_sub_modules[1].info), "OFFLINE");
            }
        } else if (strcmp(name, "Submodel3") == 0) {
            s_sub_modules[2].online = is_online;
            s_sub_modules[2].type = type_val;
            s_sub_modules[2].subtype = subtype_val;
            if (is_online) {
                const char *sub = subtype_name(type_val, subtype_val);
                if (sub)
                    snprintf(s_sub_modules[2].info, sizeof(s_sub_modules[2].info), "%s", sub);
                else
                    snprintf(s_sub_modules[2].info, sizeof(s_sub_modules[2].info), "ONLINE");
            } else {
                snprintf(s_sub_modules[2].info, sizeof(s_sub_modules[2].info), "OFFLINE");
            }
        }

        line = strtok(NULL, "\r\n");
    }
}

/*=============================================================================
 *  CLI Response Callback
*=============================================================================*/

/* Static buffers for dynamic label text (must persist since labels hold pointers) */
static char s_core_info_text[3][24];
static char s_sub_info_text[3][24];

/* Accumulation buffer for multi-frame CLI responses */
/* lsdev response uses global CLI buffer via on_cli_complete */

static void on_lsdev_cli_complete(const char *buf, uint16_t len, const char *tag)
{
    (void)tag;

    parse_lsdev_response(buf, len);

    for (int i = 0; i < 3; i++) {
        strncpy(s_core_info_text[i], s_core_modules[i].info, sizeof(s_core_info_text[i]) - 1);
        s_core_info_text[i][sizeof(s_core_info_text[i]) - 1] = '\0';
        strncpy(s_sub_info_text[i], s_sub_modules[i].info, sizeof(s_sub_info_text[i]) - 1);
        s_sub_info_text[i][sizeof(s_sub_info_text[i]) - 1] = '\0';
    }

    s_lsdev_pending = false;
    models_update_cards();
    ui_page_invalidate_all();
}

static uart_cli_cb_t s_models_cb = {
    .on_cli_complete = on_lsdev_cli_complete,
};

/*=============================================================================
 *  Widget Management
*=============================================================================*/

static void update_models_widgets(void)
{
    uint8_t tab = ui_tabview_get_active(&tabview);
    s_models_widgets[0] = (ui_widget_t *)&lbl_title;
    s_models_widgets[1] = (ui_widget_t *)&tabview;

    ui_card_t *cards = NULL;
    int count = 0;
    switch (tab) {
    case 0: cards = card_core; count = 3; break;
    case 1: cards = card_sub;  count = 3; break;
    default: break;
    }

    for (int i = 0; i < count; i++) {
        s_models_widgets[2 + i] = (ui_widget_t *)&cards[i];
    }
    ui_page_set_widgets(&page_models, s_models_widgets, 2 + count);
}

/*=============================================================================
 *  Tab Change Handler
*=============================================================================*/

static void models_tab_change(ui_widget_t *w, uint8_t tab)
{
    (void)w;
    (void)tab;
    update_models_widgets();
    ui_page_invalidate_all();
}

/*=============================================================================
 *  Internal: Initialize card widgets for a tab
*=============================================================================*/

static void init_tab_cards(module_info_t *mods, ui_card_t *cards,
                           ui_label_t *names, ui_status_dot_t *dots,
                           ui_label_t *infos, int count,
                           int16_t cx, int16_t cy_start)
{
    int16_t card_w = 160;
    int16_t card_h = 70;
    int16_t card_gap = 12;
    int16_t col = 0;
    int16_t cy = cy_start;

    for (int i = 0; i < count; i++) {
        ui_rect_t card_rect = {cx + col * (card_w + card_gap), cy, card_w, card_h};
        ui_card_init(&cards[i], &card_rect);
        cards[i].base.bg_color = UI_COLOR_BG_CARD;
        cards[i].radius = 8;

        ui_rect_t name_r = {card_rect.x + 10, card_rect.y + 8, 100, 18};
        ui_label_init(&names[i], &name_r, mods[i].name, &font_montserrat_12);
        names[i].base.bg_color = UI_COLOR_TRANSPARENT;
        ui_label_set_color(&names[i], UI_COLOR_TEXT_PRIMARY);

        ui_rect_t dot_r = {card_rect.x + card_w - 22, card_rect.y + 10, 12, 12};
        ui_status_dot_init(&dots[i], &dot_r, mods[i].online);

        ui_rect_t info_r = {card_rect.x + 10, card_rect.y + 34, 140, 16};
        ui_label_init(&infos[i], &info_r, mods[i].info, &font_montserrat_12);
        infos[i].base.bg_color = UI_COLOR_TRANSPARENT;
        ui_label_set_color(&infos[i], UI_COLOR_TEXT_SECONDARY);

        ui_card_add_child(&cards[i], (ui_widget_t *)&names[i]);
        ui_card_add_child(&cards[i], (ui_widget_t *)&dots[i]);
        ui_card_add_child(&cards[i], (ui_widget_t *)&infos[i]);

        col++;
        if (col >= 3) { col = 0; cy += card_h + card_gap; }
    }
}

/* Update card widgets with current module status data */
static void models_update_cards(void)
{
    for (int i = 0; i < 3; i++) {
        ui_status_dot_set_state(&dot_core[i], s_core_modules[i].online);
        ui_label_set_text(&lbl_core_info[i], s_core_info_text[i]);

        ui_status_dot_set_state(&dot_sub[i], s_sub_modules[i].online);
        ui_label_set_text(&lbl_sub_info[i], s_sub_info_text[i]);
    }
}

/*=============================================================================
 *  Models Page Initialization
*=============================================================================*/

void ui_models_init(void)
{
    int16_t cx = SIDEBAR_WIDTH + 20;

    ui_rect_t title_rect = {cx, 20, 300, 30};
    ui_label_init(&lbl_title, &title_rect, "Models", &font_montserrat_16);
    ui_label_set_color(&lbl_title, UI_COLOR_TEXT_PRIMARY);

    ui_rect_t tv_rect = {SIDEBAR_WIDTH, 50, UI_SCREEN_WIDTH - SIDEBAR_WIDTH, UI_SCREEN_HEIGHT - 50};
    ui_tabview_init(&tabview, &tv_rect, 2);
    ui_tabview_set_label(&tabview, 0, "Core");
    ui_tabview_set_label(&tabview, 1, "Sub");
    tabview.on_tab_change = models_tab_change;

    s_models_widgets[0] = (ui_widget_t *)&lbl_title;
    s_models_widgets[1] = (ui_widget_t *)&tabview;
    ui_page_set_widgets(&page_models, s_models_widgets, 2);
    ui_page_set_callbacks(&page_models, ui_models_enter, NULL, NULL, NULL);

    s_models_inited = false;
    s_lsdev_pending = false;
}

void ui_models_enter(ui_page_t *page)
{
    (void)page;

    if (!s_models_inited) {
        int16_t cx = SIDEBAR_WIDTH + 20;
        int16_t cy = tabview.content_rect.y + 10;

        /* Initialize persistent info text buffers */
        for (int i = 0; i < 3; i++) {
            strncpy(s_core_info_text[i], s_core_modules[i].info, sizeof(s_core_info_text[i]) - 1);
            s_core_info_text[i][sizeof(s_core_info_text[i]) - 1] = '\0';
            strncpy(s_sub_info_text[i], s_sub_modules[i].info, sizeof(s_sub_info_text[i]) - 1);
            s_sub_info_text[i][sizeof(s_sub_info_text[i]) - 1] = '\0';
        }

        init_tab_cards(s_core_modules, card_core, lbl_core_name, dot_core, lbl_core_info, 3, cx, cy);
        init_tab_cards(s_sub_modules, card_sub, lbl_sub_name, dot_sub, lbl_sub_info, 3, cx, cy);

        /* Set info labels to point to persistent buffers */
        for (int i = 0; i < 3; i++) {
            ui_label_set_text(&lbl_core_info[i], s_core_info_text[i]);
            ui_label_set_text(&lbl_sub_info[i], s_sub_info_text[i]);
        }

        update_models_widgets();
        s_models_inited = true;
    }

    /* Send lsdev command to query current module status */
    if (!s_lsdev_pending) {
        s_lsdev_pending = true;
        UART_SetCLICallbacks(&s_models_cb);
        UART_SendCLI("lsdev");
    }
}
