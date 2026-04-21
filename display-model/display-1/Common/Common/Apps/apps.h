/********************************** (C) COPYRIGHT *******************************
* File Name          : apps.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : All app page declarations and init functions.
********************************************************************************/
#ifndef __APPS_H
#define __APPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../MiniUI/miniui.h"

void apps_init_all(void);

ui_page_t *app_music_get_page(void);
ui_page_t *app_file_get_page(void);
ui_page_t *app_editor_get_page(void);
ui_page_t *app_images_get_page(void);
ui_page_t *app_usb_get_page(void);
ui_page_t *app_power_get_page(void);
ui_page_t *app_bt_get_page(void);
ui_page_t *app_nfc_get_page(void);
ui_page_t *app_fingerprint_get_page(void);
ui_page_t *app_health_get_page(void);
ui_page_t *app_subdisplay_get_page(void);
ui_page_t *app_lights_get_page(void);
ui_page_t *app_irrange_get_page(void);
ui_page_t *app_ebook_get_page(void);
ui_page_t *app_emusic_get_page(void);

#ifdef __cplusplus
}
#endif

#endif /* __APPS_H */
