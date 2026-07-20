/********************************** (C) COPYRIGHT *******************************
* File Name          : debug_cli.c
* Author             : E-ink Model Team
* Version            : V1.1.0
* Date               : 2026/07/19
* Description        : UART2 debug CLI for live e-paper waveform tuning.
*
*  Waveform tuning on real hardware needs fast iteration — reflashing for
*  every LUT/VCOM experiment is too slow.  This CLI exposes the tunables
*  over the debug UART (921600 8-N-1), effective immediately:
*
*    help                    show commands
*    dump                    print current tuning (vdcs/frames/dfv/pof/temp)
*    vdcs <v>                VCOM DC level, 7-bit (hex 0x.. or decimal)
*    fast <n>                FAST transition frames (partial refresh drive)
*    maint <n>               FAST maintenance frames (unchanged pixels)
*    clear <s> <e> <d>       CLEAR phases: shake / white-erase / drive frames
*    dfv <0|1>               DFV_EN differential refresh toggle
*    pof <0|1>               POF after every refresh toggle
*    scale <0|1|2>           frame scale: auto (TSC) / force x1 / force x2
*    temp                    re-read panel temperature sensor
*    tdy <aa> <vv>           write TDY register (hex addr, hex val)
*    refresh                 force a full CLEAR refresh of the current page
*
*  Numbers are decimal unless prefixed with 0x.
*
*  V1.1: RX moved to the USART2 interrupt (ring buffer).  The previous
*  main-loop polling lost bytes whenever an e-paper refresh blocked the
*  loop (300ms+), latched the USART overrun flag and stopped receiving
*  entirely (only the first typed character was echoed).
********************************************************************************/
#include "debug_cli.h"
#include "Eink/epaper.h"
#include "ch32v30x.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>

/* ui_system.c */
extern void ui_system_full_refresh(void);

#define DBG_LINE_MAX  64
#define DBG_RX_RING   128

static char    s_line[DBG_LINE_MAX];
static uint8_t s_line_len = 0;

/* ISR -> poll ring buffer */
static volatile uint8_t  s_rx_ring[DBG_RX_RING];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

/*=============================================================================
 *  USART2 IRQ — capture RX bytes into the ring, keep ORE cleared
 *===========================================================================*/

void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
        uint8_t b = (uint8_t)USART_ReceiveData(USART2);
        uint16_t next = (uint16_t)(s_rx_head + 1) % DBG_RX_RING;
        if (next != s_rx_tail) {
            s_rx_ring[s_rx_head] = b;
            s_rx_head = next;
        }
    }
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
        (void)USART_ReceiveData(USART2);        /* RDR read clears ORE */
        USART_ClearFlag(USART2, USART_FLAG_ORE);
    }
}

/*=============================================================================
 *  Init — enable USART2 RX + RXNE interrupt
 *===========================================================================*/

void Debug_CLI_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* PA3 (USART2 RX): floating input */
    gpio.GPIO_Pin   = GPIO_Pin_3;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* Enable receiver (TX already configured by USART_Printf_Init) */
    USART2->CTLR1 |= USART_Mode_Rx;

    USART_ClearFlag(USART2, USART_FLAG_ORE);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    NVIC_SetPriority(USART2_IRQn, 3 << 4);   /* lowest: debug only */
    NVIC_EnableIRQ(USART2_IRQn);

    printf("\r\n[dbgcli] type 'help' for waveform tuning commands\r\n> ");
}

/*=============================================================================
 *  Command handling
 *===========================================================================*/

static void cli_show_help(void)
{
    printf("commands:\r\n"
           "  dump                  show current tuning\r\n"
           "  vdcs <v>              VCOM DC (0x1D..0x3C typical)\r\n"
           "  fast <n>              FAST transition frames\r\n"
           "  maint <n>             FAST maintenance frames\r\n"
           "  clear <s> <e> <d>     CLEAR shake/erase/drive frames\r\n"
           "  dfv <0|1>             differential refresh\r\n"
           "  pof <0|1>             POF after refresh\r\n"
           "  scale <0|1|2>         frame scale: auto / force x1 / force x2\r\n"
           "  temp                  re-read panel temperature\r\n"
           "  tdy <aa> <vv>         write TDY reg (hex)\r\n"
           "  refresh               full CLEAR refresh now\r\n");
}

/* Split a line into up to max_tokens tokens (modifies the line) */
static uint8_t cli_tokenize(char *line, char *tokens[], uint8_t max_tokens)
{
    uint8_t n = 0;
    char *p = line;
    while (*p && n < max_tokens) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        tokens[n++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

static long cli_num(const char *s)
{
    return strtol(s, NULL, 0);   /* handles 0x prefix and decimal */
}

static void cli_execute(char *line)
{
    char *tok[5];
    uint8_t n = cli_tokenize(line, tok, 5);
    if (n == 0) return;

    if (strcmp(tok[0], "help") == 0) {
        cli_show_help();
    } else if (strcmp(tok[0], "dump") == 0) {
        Epaper_DumpTune();
    } else if (strcmp(tok[0], "vdcs") == 0 && n >= 2) {
        uint8_t v = (uint8_t)cli_num(tok[1]);
        Epaper_SetVdcs(v);
        printf("vdcs = 0x%02X\r\n", v);
    } else if (strcmp(tok[0], "fast") == 0 && n >= 2) {
        Epaper_SetFastFrames((uint8_t)cli_num(tok[1]));
        printf("fast = %ld\r\n", cli_num(tok[1]));
    } else if (strcmp(tok[0], "maint") == 0 && n >= 2) {
        Epaper_SetMaintFrames((uint8_t)cli_num(tok[1]));
        printf("maint = %ld\r\n", cli_num(tok[1]));
    } else if (strcmp(tok[0], "clear") == 0 && n >= 4) {
        Epaper_SetClearFrames((uint8_t)cli_num(tok[1]),
                              (uint8_t)cli_num(tok[2]),
                              (uint8_t)cli_num(tok[3]));
        printf("clear = %ld %ld %ld\r\n",
               cli_num(tok[1]), cli_num(tok[2]), cli_num(tok[3]));
    } else if (strcmp(tok[0], "dfv") == 0 && n >= 2) {
        Epaper_SetDfv((uint8_t)cli_num(tok[1]));
        printf("dfv = %ld\r\n", cli_num(tok[1]));
    } else if (strcmp(tok[0], "pof") == 0 && n >= 2) {
        Epaper_SetPofAfterRefresh((uint8_t)cli_num(tok[1]));
        printf("pof = %ld\r\n", cli_num(tok[1]));
    } else if (strcmp(tok[0], "scale") == 0 && n >= 2) {
        Epaper_SetFrameScale((uint8_t)cli_num(tok[1]));
        printf("scale mode = %ld\r\n", cli_num(tok[1]));
    } else if (strcmp(tok[0], "temp") == 0) {
        int8_t t = Epaper_RefreshTemperature();
        printf("temp = %d C\r\n", (int)t);
    } else if (strcmp(tok[0], "tdy") == 0 && n >= 3) {
        Epaper_WriteTdy((uint8_t)cli_num(tok[1]), (uint8_t)cli_num(tok[2]));
        printf("tdy 0x%02lX = 0x%02lX\r\n", cli_num(tok[1]), cli_num(tok[2]));
    } else if (strcmp(tok[0], "refresh") == 0) {
        ui_system_full_refresh();
        printf("refresh done\r\n");
    } else {
        printf("unknown: %s (type 'help')\r\n", tok[0]);
    }
}

/*=============================================================================
 *  Poll — drain the RX ring, echo, execute complete lines
 *===========================================================================*/

void Debug_CLI_Poll(void)
{
    while (s_rx_tail != s_rx_head) {
        uint8_t b = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)(s_rx_tail + 1) % DBG_RX_RING;

        if (b == '\r' || b == '\n') {
            if (s_line_len > 0) {
                s_line[s_line_len] = '\0';
                printf("\r\n");
                cli_execute(s_line);
                s_line_len = 0;
                printf("> ");
            }
        } else if (b == 0x08 || b == 0x7F) {   /* backspace */
            if (s_line_len > 0) {
                s_line_len--;
                printf("\b \b");
            }
        } else if (s_line_len < DBG_LINE_MAX - 1) {
            s_line[s_line_len++] = (char)b;
            /* echo */
            while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) { }
            USART_SendData(USART2, b);
        }
    }
}
