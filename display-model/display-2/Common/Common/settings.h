/********************************** (C) COPYRIGHT *******************************
* File Name          : settings.h
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/06/12
* Description        : Global system settings management for E-ink display.
*                      Adapted from Display-1: removed SSD1963 backlight,
*                      E-ink has no backlight PWM.
********************************************************************************/
#ifndef __SETTINGS_H
#define __SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*=============================================================================
 *  System Settings Structure
 *=============================================================================*/

typedef struct {
    uint8_t volume;         /* System volume (0-100) */
    uint8_t auto_off_min;   /* Auto screen-off timeout in minutes */
    bool    auto_off_enable;/* Auto screen-off switch */

    /* External HID device status */
    bool    ext_keyboard_connected;
    bool    ext_mouse_connected;
} sys_settings_t;

/*=============================================================================
 *  Global Instance
 *=============================================================================*/

extern sys_settings_t g_settings;

/*=============================================================================
 *  Settings API
 *=============================================================================*/

void settings_init(void);
void settings_load_defaults(void);
void settings_apply(void);

#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_H */
