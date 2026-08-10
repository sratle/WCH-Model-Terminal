/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_cmd.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : UART command-line framework implementation.
*                      Line editor: printable chars are appended, BS/DEL
*                      erase, CR or LF executes. Overlong lines are
*                      truncated with a warning.
********************************************************************************/
#include "bsp_cmd.h"
#include "bsp_uart.h"
#include <string.h>

/*=============================================================================
 *  Module State
 *=============================================================================*/

typedef struct {
    const char    *name;
    cmd_handler_t  handler;
    const char    *help;
} cmd_entry_t;

static cmd_entry_t s_table[CMD_MAX_COMMANDS];
static uint8_t     s_count = 0;

static char        s_line[CMD_LINE_MAX];
static uint8_t     s_len = 0;

/*=============================================================================
 *  Registration
 *=============================================================================*/

uint8_t CMD_Register(const char *name, cmd_handler_t handler,
                     const char *help)
{
    if (!name || !handler || s_count >= CMD_MAX_COMMANDS) return 0;
    s_table[s_count].name    = name;
    s_table[s_count].handler = handler;
    s_table[s_count].help    = help ? help : "";
    s_count++;
    return 1;
}

/*=============================================================================
 *  Parsing / Dispatch
 *=============================================================================*/

int CMD_ExecuteString(char *line)
{
    char   *argv[CMD_MAX_ARGS];
    int     argc = 0;
    char   *p    = line;
    uint8_t i;
    int     ret = CMD_OK;

    /* Tokenize in place: split on spaces/tabs */
    while (*p && argc < CMD_MAX_ARGS) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    if (argc == 0) return CMD_OK;               /* Empty line */

    for (i = 0; i < s_count; i++) {
        if (strcmp(argv[0], s_table[i].name) == 0) {
            ret = s_table[i].handler(argc, argv);
            if (ret == CMD_ERR_USAGE) {
                UART_SendString("usage: ");
                UART_SendString(s_table[i].help);
                UART_SendString("\r\n");
            } else if (ret == CMD_ERR_VALUE) {
                UART_SendString("invalid value\r\n");
            } else if (ret != CMD_OK) {
                UART_SendString("error\r\n");
            }
            return ret;
        }
    }

    UART_SendString("unknown command: ");
    UART_SendString(argv[0]);
    UART_SendString("\r\n");
    return -1;
}

/*=============================================================================
 *  Line Editor
 *=============================================================================*/

/*********************************************************************
 * @fn      CMD_FeedChar
 *
 * @brief   Feed one received character into the line editor.
 *
 * @return  none
 */
static void CMD_FeedChar(uint8_t ch)
{
    if (ch == '\r' || ch == '\n') {
        if (s_len == 0) return;                 /* Ignore blank lines   */
        UART_SendString("\r\n");
        s_line[s_len] = '\0';
        CMD_ExecuteString(s_line);
        s_len = 0;
        UART_SendString(CMD_PROMPT);
    } else if (ch == '\b' || ch == 0x7F) {      /* Backspace / DEL      */
        if (s_len > 0) {
            s_len--;
#if CMD_ECHO
            UART_SendString("\b \b");
#endif
        }
    } else if (ch >= 32 && ch < 127) {          /* Printable ASCII      */
        if (s_len < CMD_LINE_MAX - 1) {
            s_line[s_len++] = (char)ch;
#if CMD_ECHO
            UART_SendByte(ch);
#endif
        } else {
            UART_SendString("\r\n[line too long]\r\n" CMD_PROMPT);
            s_len = 0;
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void CMD_Init(void)
{
    s_len = 0;
    UART_RxFlush();
    UART_SendString("\r\nWCH-DevBoard console, 'help' for commands\r\n");
    UART_SendString(CMD_PROMPT);
}

void CMD_Task(void)
{
    int16_t ch;
    while ((ch = UART_ReadByte()) >= 0) {
        CMD_FeedChar((uint8_t)ch);
    }
}

void CMD_PrintHelp(void)
{
    uint8_t i;
    UART_SendString("commands:\r\n");
    for (i = 0; i < s_count; i++) {
        UART_SendString("  ");
        UART_SendString(s_table[i].help);
        UART_SendString("\r\n");
    }
}
