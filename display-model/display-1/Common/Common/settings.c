/********************************** (C) COPYRIGHT *******************************
* File Name          : settings.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : Global system settings implementation.
********************************************************************************/
#include "settings.h"
#include "SSD1963/ssd1963.h"
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

    g_settings.backlight       = 50;   /* Match SSD1963 init default */
    g_settings.volume          = 60;
    g_settings.auto_off_min    = 5;
    g_settings.auto_off_enable = false;
}

void settings_apply(void)
{
    /* Sync backlight to SSD1963 */
    SSD1963_SetBacklight(g_settings.backlight);

    /* Future: sync volume to audio DAC, etc. */
}

void settings_init(void)
{
    settings_load_defaults();
    settings_apply();
}
