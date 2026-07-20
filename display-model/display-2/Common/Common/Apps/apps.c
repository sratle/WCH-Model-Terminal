/********************************** (C) COPYRIGHT *******************************
* File Name          : apps.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : All app page init dispatcher.
********************************************************************************/
#include "apps.h"
#include "app_music.h"
#include "app_file.h"
#include "app_editor.h"
#include "app_images.h"
#include "app_ebook.h"
#include "app_stub.h"

void apps_init_all(void)
{
    /* Fully ported apps */
    app_music_init();
    app_file_init();
    app_editor_init();
    app_images_init();
    app_ebook_init();

    /* Stub apps */
    app_stub_init_all();
}
