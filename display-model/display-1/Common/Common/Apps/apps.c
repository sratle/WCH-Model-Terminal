/********************************** (C) COPYRIGHT *******************************
* File Name          : apps.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : All app page init dispatcher.
********************************************************************************/
#include "apps.h"
#include "app_music.h"
#include "app_file.h"
#include "app_editor.h"
#include "app_images.h"
#include "app_usb.h"
#include "app_power.h"
#include "app_bt.h"
#include "app_nfc.h"
#include "app_fingerprint.h"
#include "app_health.h"
#include "app_subdisplay.h"
#include "app_lights.h"
#include "app_irrange.h"
#include "app_ebook.h"
#include "app_emusic.h"
#include "app_terminal.h"

void apps_init_all(void)
{
    app_music_init();
    app_file_init();
    app_editor_init();
    app_images_init();
    app_usb_init();
    app_power_init();
    app_bt_init();
    app_nfc_init();
    app_fingerprint_init();
    app_health_init();
    app_subdisplay_init();
    app_lights_init();
    app_irrange_init();
    app_ebook_init();
    app_emusic_init();
    app_terminal_init();
}
