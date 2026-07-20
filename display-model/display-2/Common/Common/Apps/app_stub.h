/********************************** (C) COPYRIGHT *******************************
* File Name          : app_stub.h
* Author             : E-ink Model Team
* Description        : Stub pages for apps not yet ported to E-ink.
********************************************************************************/
#ifndef __APP_STUB_H
#define __APP_STUB_H

#include "../MiniUI/miniui.h"

void app_stub_init_all(void);

ui_page_t *app_usb_get_page(void);
ui_page_t *app_power_get_page(void);
ui_page_t *app_bt_get_page(void);
ui_page_t *app_nfc_get_page(void);
ui_page_t *app_fingerprint_get_page(void);
ui_page_t *app_health_get_page(void);
ui_page_t *app_subdisplay_get_page(void);
ui_page_t *app_rgb_get_page(void);
ui_page_t *app_irrange_get_page(void);
ui_page_t *app_emusic_get_page(void);
ui_page_t *app_terminal_get_page(void);

#endif
