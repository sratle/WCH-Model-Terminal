/********************************** (C) COPYRIGHT *******************************
* File Name          : app_editor.h
* Author             : E-ink Model Team
* Description        : Text Editor app header (E-ink port).
********************************************************************************/
#ifndef __APP_EDITOR_H
#define __APP_EDITOR_H

#include "../MiniUI/miniui.h"

void app_editor_init(void);
ui_page_t *app_editor_get_page(void);

/* Open a file for editing (called from the Files app before page push) */
void app_editor_open_file(const char *path, const char *name);

#endif
