/********************************** (C) COPYRIGHT *******************************
* File Name          : debug_cli.h
* Author             : E-ink Model Team
* Description        : UART2 debug CLI for live waveform tuning.
********************************************************************************/
#ifndef __DEBUG_CLI_H
#define __DEBUG_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Enable USART2 RX (PA3) and print the help banner. */
void Debug_CLI_Init(void);

/* Non-blocking poll — call from the main loop. */
void Debug_CLI_Poll(void);

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_CLI_H */
