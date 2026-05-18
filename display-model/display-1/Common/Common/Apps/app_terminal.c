/********************************** (C) COPYRIGHT *******************************
* File Name          : app_terminal.c
* Author             : LCD Model Team
* Version            : V1.0.0
* Date               : 2025/04/20
* Description        : Terminal app — keyboard input & command execution.
*                      Placeholder: page registered, UI not yet implemented.
********************************************************************************/
#include "app_terminal.h"
#include "../UI/ui_app_common.h"

static ui_app_page_t s_app_terminal;

void app_terminal_init(void)
{
    ui_app_page_init(&s_app_terminal, "Terminal", 0x10F);
    ui_page_register(&s_app_terminal.page);
}

ui_page_t *app_terminal_get_page(void)
{
    return &s_app_terminal.page;
}
