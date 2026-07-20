/********************************** (C) COPYRIGHT *******************************
* File Name          : app_music.h
* Author             : E-ink Model Team
* Description        : Music Player app header (E-ink port).
********************************************************************************/
#ifndef __APP_MUSIC_H
#define __APP_MUSIC_H

#include "../MiniUI/miniui.h"

void app_music_init(void);
ui_page_t *app_music_get_page(void);

/* Set the playlist from the file browser. names[] are wav filenames,
 * count is number of tracks, start_index is the initially selected track.
 * Must be called before pushing the music page. */
void app_music_set_playlist(const char *names[], int16_t count, int16_t start_index);

#endif
