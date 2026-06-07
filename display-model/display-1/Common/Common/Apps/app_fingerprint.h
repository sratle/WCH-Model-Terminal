/********************************** (C) COPYRIGHT *******************************
* File Name          : app_fingerprint.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/07
* Description        : Fingerprint manager app — query, register, delete,
*                      LED control, security level via CLI passthrough.
********************************************************************************/
#ifndef __APP_FINGERPRINT_H
#define __APP_FINGERPRINT_H

#include "../MiniUI/miniui.h"

void app_fingerprint_init(void);
ui_page_t *app_fingerprint_get_page(void);

#endif
