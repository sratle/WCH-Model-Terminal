#ifndef __APP_EDITOR_H
#define __APP_EDITOR_H

#include "../MiniUI/miniui.h"

void app_editor_init(void);
ui_page_t *app_editor_get_page(void);

/* Open a file in the editor (called from file manager) */
void app_editor_open_file(const char *path, const char *name);

#endif
