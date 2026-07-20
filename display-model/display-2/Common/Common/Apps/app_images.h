/********************************** (C) COPYRIGHT *******************************
* File Name          : app_images.h
* Author             : E-ink Model Team
* Description        : Image browser app header (E-ink port).
********************************************************************************/
#ifndef __APP_IMAGES_H
#define __APP_IMAGES_H

#include "../MiniUI/miniui.h"

void app_images_init(void);
ui_page_t *app_images_get_page(void);

/* Open a specific BMP file (called from the Files app before page push).
 * The Core CLI working directory must already be the file's directory. */
void app_images_open_file(const char *name);

#endif
