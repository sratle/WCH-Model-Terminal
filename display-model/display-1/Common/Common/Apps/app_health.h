/********************************** (C) COPYRIGHT *******************************
* File Name          : app_health.h
* Author             : LCD Model Team
* Version            : V2.0.0
* Date               : 2026/06/10
* Description        : Health monitor app — real-time HR/SpO2/HRV display
*                      with line charts and averages. Data received via
*                      DISP_EXT_SUBMODEL_EVENT from Core.
********************************************************************************/
#ifndef __APP_HEALTH_H
#define __APP_HEALTH_H

#include "../MiniUI/miniui.h"

void app_health_init(void);
ui_page_t *app_health_get_page(void);

#endif
