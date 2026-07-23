/********************************** (C) COPYRIGHT *******************************
* File Name          : app_usb.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2026/07/22
* Description        : USB app — shows external HID (USB host via CH9350)
*                      keyboard / mouse connection status. Display-only.
*                      Reactive: state pushed by Core into g_settings; the
*                      page tick detects changes and repaints only the row
*                      that changed (MiniUI partial refresh).
********************************************************************************/
#include "app_usb.h"
#include "../UI/ui_app_common.h"
#include "../UART/uart_module.h"
#include "../settings.h"

/*=============================================================================
 *  Layout (800x480)
 *=============================================================================*/

#define USB_HDR_Y       (APP_TITLE_BAR_H + 8)
#define USB_ROW_X       48
#define USB_ROW_W       (UI_SCREEN_WIDTH - 2 * USB_ROW_X)
#define USB_ROW_H       88
#define USB_ROW_KBD_Y   (APP_TITLE_BAR_H + 44)
#define USB_ROW_MOUSE_Y (USB_ROW_KBD_Y + USB_ROW_H + 20)

#define USB_CARD_BORDER UI_HEX(0xE0E0E0)

static ui_app_page_t s_app_usb;

/* Change-detection cache (0xFF = force first paint) */
static uint8_t s_prev_kbd   = 0xFF;
static uint8_t s_prev_mouse = 0xFF;

/*=============================================================================
 *  Drawing
 *=============================================================================*/

static void usb_draw_row(int16_t y, const char *label, uint8_t connected)
{
    ui_rect_t card = {USB_ROW_X, y, USB_ROW_W, USB_ROW_H};
    ui_draw_fill_round_rect(&card, 10, UI_COLOR_BG_CARD);
    ui_draw_round_rect_border(&card, 10, USB_CARD_BORDER, 1);

    /* Status dot */
    ui_color_t dot = connected ? UI_COLOR_STATUS_ON : UI_COLOR_STATUS_OFF;
    ui_draw_fill_circle(USB_ROW_X + 34, y + USB_ROW_H / 2, 14, dot);

    /* Device label + status text */
    ui_draw_text(USB_ROW_X + 66, y + 20, label,
                 &font_montserrat_16, UI_COLOR_TEXT_PRIMARY);
    ui_draw_text(USB_ROW_X + 66, y + 50,
                 connected ? "Connected" : "Not connected",
                 &font_montserrat_12,
                 connected ? UI_COLOR_TEXT_PRIMARY : UI_COLOR_TEXT_SECONDARY);
}

/*=============================================================================
 *  Page Callbacks
 *=============================================================================*/

static void usb_page_enter(ui_page_t *page)
{
    (void)page;
    s_prev_kbd   = 0xFF;
    s_prev_mouse = 0xFF;
    /* Pull current status once on entry (Core replies with reactive pushes) */
    UART_SendGetSysStatus();
    ui_page_invalidate_all();
}

static void usb_page_update(ui_page_t *page)
{
    (void)page;
    uint8_t kbd   = g_settings.ext_keyboard_connected ? 1 : 0;
    uint8_t mouse = g_settings.ext_mouse_connected ? 1 : 0;

    if (kbd != s_prev_kbd) {
        s_prev_kbd = kbd;
        ui_rect_t r = {USB_ROW_X, USB_ROW_KBD_Y, USB_ROW_W, USB_ROW_H};
        ui_page_invalidate(&r);
    }
    if (mouse != s_prev_mouse) {
        s_prev_mouse = mouse;
        ui_rect_t r = {USB_ROW_X, USB_ROW_MOUSE_Y, USB_ROW_W, USB_ROW_H};
        ui_page_invalidate(&r);
    }
}

static void usb_page_draw(ui_page_t *page, ui_rect_t *dirty)
{
    (void)page;

    /* Title bar background (compositor clips to dirty rows) */
    ui_rect_t bar = {0, 0, UI_SCREEN_WIDTH, APP_TITLE_BAR_H};
    ui_draw_fill_rect(&bar, UI_COLOR_PRIMARY);

    int16_t dtop = dirty->y;
    int16_t dbot = dirty->y + dirty->h;

    /* Section header (only when its band is dirty) */
    if (dtop < USB_ROW_KBD_Y) {
        ui_draw_text(USB_ROW_X, USB_HDR_Y, "External USB HID Devices",
                     &font_montserrat_12, UI_COLOR_TEXT_SECONDARY);
    }

    if (dbot > USB_ROW_KBD_Y && dtop < USB_ROW_KBD_Y + USB_ROW_H)
        usb_draw_row(USB_ROW_KBD_Y, "Keyboard", g_settings.ext_keyboard_connected);

    if (dbot > USB_ROW_MOUSE_Y && dtop < USB_ROW_MOUSE_Y + USB_ROW_H)
        usb_draw_row(USB_ROW_MOUSE_Y, "Mouse", g_settings.ext_mouse_connected);
}

/*=============================================================================
 *  Init
 *=============================================================================*/

void app_usb_init(void)
{
    ui_app_page_init(&s_app_usb, "USB", 0x104);
    ui_page_set_callbacks(&s_app_usb.page, usb_page_enter, NULL, usb_page_draw, NULL);
    ui_page_set_update_cb(&s_app_usb.page, usb_page_update);
    ui_page_register(&s_app_usb.page);
}

ui_page_t *app_usb_get_page(void)
{
    return &s_app_usb.page;
}
