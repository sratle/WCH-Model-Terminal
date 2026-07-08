/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Description        : Display-2 E-paper demo with MiniUI.
*********************************************************************************
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
*******************************************************************************/

#include "debug.h"
#include "../Common/Common/Eink/epaper.h"
#include "../Common/Common/MiniUI/miniui.h"
#include "../Common/Common/MiniUI/font/font_montserrat_12.h"
#include "../Common/Common/MiniUI/font/font_montserrat_16.h"
#include "../Common/Common/Games/games.h"
#include "../Common/Common/UART/uart_module.h"

/* ui_system functions */
void ui_system_init(void);
void ui_system_tick(void);

/* UI page functions */
void ui_main_init(void);

/*=============================================================================
 *  Main
 *=============================================================================*/

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();

    /* Initialize debug UART2 (PA2-TX) at 921600/8-N-1 */
    USART_Printf_Init(921600);

    printf("\r\n=== Display-2 MiniUI Demo ===\r\n");

    /* Initialize UART1 module for Core communication (USART1 PA9-TX/PA10-RX) */
    UART_Module_Init();

    /* Initialize MiniUI system (includes e-paper init) */
    ui_system_init();

    /* Initialize UI pages (home, settings, models, games, etc.) */
    ui_main_init();

    /* Initialize all game modules */
    games_init_all();

    printf("[main] entering main loop\r\n");

    while (1)
    {
        UART_Module_Poll();   /* Process received UART frames from Core */
        ui_system_tick();
        Delay_Ms(10);  /* ~100Hz for touch responsiveness; e-ink refresh throttled by dirty list */
    }
}
