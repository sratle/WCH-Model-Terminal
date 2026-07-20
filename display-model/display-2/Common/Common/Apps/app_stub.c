/********************************** (C) COPYRIGHT *******************************
* File Name          : app_stub.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : Stub pages for apps not yet ported to E-ink.
*                      Title bar + "coming soon" note + back button,
*                      so every icon in the Apps grid opens a page.
********************************************************************************/
#include "app_stub.h"
#include "../UI/ui_app_common.h"
#include "../MiniUI/font/font_montserrat_12.h"

typedef struct {
    ui_app_page_t page;
    ui_label_t    lbl_note;
    ui_widget_t  *widgets[3];
} app_stub_page_t;

typedef enum {
    STUB_USB = 0,
    STUB_POWER,
    STUB_BT,
    STUB_NFC,
    STUB_FINGER,
    STUB_HEALTH,
    STUB_SUBDISP,
    STUB_RGB,
    STUB_IRRANGE,
    STUB_EMUSIC,
    STUB_TERMINAL,
    STUB_COUNT
} stub_idx_t;

static const char *s_stub_names[STUB_COUNT] = {
    "USB", "Power", "BT", "NFC", "Finger", "Health",
    "SubDisp", "RGB", "IRRange", "EMusic", "Terminal"
};

static app_stub_page_t s_stubs[STUB_COUNT];

static void stub_init_one(stub_idx_t idx, uint32_t id)
{
    app_stub_page_t *s = &s_stubs[idx];

    ui_app_page_init(&s->page, s_stub_names[idx], id);

    ui_rect_t r = {0, 220, UI_SCREEN_WIDTH, 30};
    ui_label_init(&s->lbl_note, &r, "Not available on E-ink yet", &font_montserrat_12);
    ui_label_set_color(&s->lbl_note, UI_COLOR_BLACK);
    ui_label_set_align(&s->lbl_note, 1);

    s->widgets[0] = (ui_widget_t *)&s->page.btn_back;
    s->widgets[1] = (ui_widget_t *)&s->page.lbl_title;
    s->widgets[2] = (ui_widget_t *)&s->lbl_note;
    ui_page_set_widgets(&s->page.page, s->widgets, 3);

    ui_page_register(&s->page.page);
}

void app_stub_init_all(void)
{
    stub_init_one(STUB_USB,      0x104);
    stub_init_one(STUB_POWER,    0x105);
    stub_init_one(STUB_BT,       0x106);
    stub_init_one(STUB_NFC,      0x107);
    stub_init_one(STUB_FINGER,   0x108);
    stub_init_one(STUB_HEALTH,   0x109);
    stub_init_one(STUB_SUBDISP,  0x10A);
    stub_init_one(STUB_RGB,      0x10B);
    stub_init_one(STUB_IRRANGE,  0x10C);
    stub_init_one(STUB_EMUSIC,   0x10E);
    stub_init_one(STUB_TERMINAL, 0x110);
}

ui_page_t *app_usb_get_page(void)         { return &s_stubs[STUB_USB].page.page; }
ui_page_t *app_power_get_page(void)       { return &s_stubs[STUB_POWER].page.page; }
ui_page_t *app_bt_get_page(void)          { return &s_stubs[STUB_BT].page.page; }
ui_page_t *app_nfc_get_page(void)         { return &s_stubs[STUB_NFC].page.page; }
ui_page_t *app_fingerprint_get_page(void) { return &s_stubs[STUB_FINGER].page.page; }
ui_page_t *app_health_get_page(void)      { return &s_stubs[STUB_HEALTH].page.page; }
ui_page_t *app_subdisplay_get_page(void)  { return &s_stubs[STUB_SUBDISP].page.page; }
ui_page_t *app_rgb_get_page(void)         { return &s_stubs[STUB_RGB].page.page; }
ui_page_t *app_irrange_get_page(void)     { return &s_stubs[STUB_IRRANGE].page.page; }
ui_page_t *app_emusic_get_page(void)      { return &s_stubs[STUB_EMUSIC].page.page; }
ui_page_t *app_terminal_get_page(void)    { return &s_stubs[STUB_TERMINAL].page.page; }
