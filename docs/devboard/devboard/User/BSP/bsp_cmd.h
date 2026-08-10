/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_cmd.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : UART command-line framework. Bytes from the bsp_uart
*                      RX ring buffer are assembled into lines, split into
*                      argv tokens and dispatched to registered handlers.
*
*                      Typical use:
*                          static int Cmd_Led(int argc, char *argv[]) {
*                              if (argc > 1 && !strcmp(argv[1], "on")) LED_On();
*                              return 0;
*                          }
*                          CMD_Init();
*                          CMD_Register("led", Cmd_Led, "led on|off|toggle");
*                          while (1) CMD_Task();      // non-blocking
********************************************************************************/
#ifndef __BSP_CMD_H
#define __BSP_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Configuration
 *=============================================================================*/

#define CMD_LINE_MAX        96      /* Max characters per command line   */
#define CMD_MAX_ARGS        8       /* Max tokens (argv[0] = name)       */
#define CMD_MAX_COMMANDS    16      /* Registration table size           */
#define CMD_ECHO            1       /* Echo received characters          */
#define CMD_PROMPT          "> "    /* Prompt printed before each line   */

/* Handler return codes (printed by the framework when nonzero) */
#define CMD_OK              0
#define CMD_ERR_USAGE       1       /* Prints the command's help text    */
#define CMD_ERR_VALUE       2       /* Prints "invalid value"            */

/*=============================================================================
 *  Types
 *=============================================================================*/

/*********************************************************************
 * Command handler.
 *   argc/argv follow the C convention: argv[0] is the command name.
 *   Return CMD_OK, or CMD_ERR_USAGE to have the framework print help.
 */
typedef int (*cmd_handler_t)(int argc, char *argv[]);

/*=============================================================================
 *  Framework API
 *=============================================================================*/

/*********************************************************************
 * @fn      CMD_Init
 *
 * @brief   Reset the line editor and print the first prompt.
 *          Requires UART_Init() to be called first.
 *
 * @return  none
 */
void CMD_Init(void);

/*********************************************************************
 * @fn      CMD_Register
 *
 * @brief   Add a command to the dispatch table.
 *
 * @param   name    - command word (matched case-sensitively)
 * @param   handler - function called with argc/argv
 * @param   help    - one-line usage text for "help" / usage errors
 *
 * @return  1 = registered, 0 = table full / bad arguments.
 */
uint8_t CMD_Register(const char *name, cmd_handler_t handler,
                     const char *help);

/*********************************************************************
 * @fn      CMD_Task
 *
 * @brief   Poll the UART RX buffer, assemble lines and execute complete
 *          commands. Call it from the main loop; fully non-blocking.
 *          Handles CR/LF, backspace and optional echo.
 *
 * @return  none
 */
void CMD_Task(void);

/*********************************************************************
 * @fn      CMD_ExecuteString
 *
 * @brief   Parse and execute one command line directly (without UART).
 *          Useful for self-tests or command scripting from code.
 *
 * @param   line - command line, modified in place (tokenized)
 *
 * @return  Handler return code, or -1 if the command was not found.
 */
int CMD_ExecuteString(char *line);

/*********************************************************************
 * @fn      CMD_PrintHelp
 *
 * @brief   Print the registered command list over UART.
 *
 * @return  none
 */
void CMD_PrintHelp(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CMD_H */
