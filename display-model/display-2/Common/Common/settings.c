/********************************** (C) COPYRIGHT *******************************
* File Name          : settings.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : Global system settings implementation for E-ink display.
*                      No SSD1963 backlight dependency.
********************************************************************************/
#include "settings.h"
#include <string.h>

/*=============================================================================
 *  Global Settings Instance
 *=============================================================================*/

sys_settings_t g_settings;

/*=============================================================================
 *  Settings Implementation
 *=============================================================================*/

void settings_load_defaults(void)
{
    memset(&g_settings, 0, sizeof(g_settings));

    g_settings.volume          = 60;
    g_settings.auto_off_min    = 5;
    g_settings.auto_off_enable = false;
    g_settings.ext_keyboard_connected = false;
    g_settings.ext_mouse_connected    = false;
}

void settings_apply(void)
{
    /* E-ink: no backlight to sync.
     * Future: sync volume to audio DAC, etc. */
}

void settings_init(void)
{
    settings_load_defaults();
    settings_apply();
}
