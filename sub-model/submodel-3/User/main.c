/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Submodel-3 (NFC) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      UART2 (PA2/PA3) @ 9600 for NFC module (passive receive).
 *********************************************************************************/

#include "debug.h"
#include "../Common/Common/hardware.h"
#include "../Common/Common/Nfc/nfc.h"
#include "../Common/Common/Uart/uart_core.h"

extern volatile uint32_t g_usart2_isr_count;
extern volatile uint32_t g_usart2_byte_count;

int main(void)
{
    uint32_t seconds = 0;

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    Hardware_Init();

    while (1)
    {
        Delay_Ms(1000);
        seconds++;

        /* Diagnostic: isr = USART2 RXNE count, ore = overrun count, sec = uptime */
        {
            char buf[64];
            uint8_t i = 0;
            uint32_t isr = g_usart2_isr_count;
            uint32_t ore = g_usart2_byte_count;

            const char *fmt = "[DBG] sec=";
            while (*fmt) buf[i++] = *fmt++;
            { uint32_t v = seconds; char tmp[12]; int n = 0;
              if (v == 0) { tmp[n++] = '0'; }
              else { while (v > 0) { tmp[n++] = '0' + (v % 10); v /= 10; } }
              while (n > 0) buf[i++] = tmp[--n];
            }

            fmt = " isr="; while (*fmt) buf[i++] = *fmt++;
            { uint32_t v = isr; char tmp[12]; int n = 0;
              if (v == 0) { tmp[n++] = '0'; }
              else { while (v > 0) { tmp[n++] = '0' + (v % 10); v /= 10; } }
              while (n > 0) buf[i++] = tmp[--n];
            }

            fmt = " ore="; while (*fmt) buf[i++] = *fmt++;
            { uint32_t v = ore; char tmp[12]; int n = 0;
              if (v == 0) { tmp[n++] = '0'; }
              else { while (v > 0) { tmp[n++] = '0' + (v % 10); v /= 10; } }
              while (n > 0) buf[i++] = tmp[--n];
            }

            buf[i++] = '\r'; buf[i++] = '\n';
            UartCore_SendData((uint8_t *)buf, i);
        }
    }
}
