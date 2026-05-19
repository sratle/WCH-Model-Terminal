/********************************** (C) COPYRIGHT *******************************
* File Name          : settings.h
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/19
* Description        : Global system settings management.
*                      Single source of truth for all persistent UI/HW state.
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
    uint8_t backlight;      /* Backlight PWM level (0-255) */
    uint8_t volume;         /* System volume (0-100) */
    uint8_t auto_off_min;   /* Auto screen-off timeout in minutes */
    bool    auto_off_enable;/* Auto screen-off switch */
    /* Extend here for future settings (WiFi, BT, theme, etc.) */
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
