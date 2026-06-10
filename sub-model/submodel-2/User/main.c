/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Submodel-2 (Health Monitor) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      I2C (PB14/PB15) software I2C for MAX30102.
 *                      PB13 INT for MAX30102 FIFO almost-full interrupt.
 *********************************************************************************/

#include "debug.h"
#include "../Common/Common/hardware.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/Max30102/max30102.h"

extern volatile uint8_t g_max30102_int_flag;

/* Report interval counter (in main loop iterations).
 * At ~100MHz with minimal work per iteration, we approximate timing.
 * Use TIM2 for precise timing instead if needed. */
static volatile uint32_t report_counter = 0;

/* Approximate number of main loop iterations per second.
 * Calibrated empirically; rough estimate for 72MHz CH32V103. */
#define LOOPS_PER_SECOND    200000UL

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* NOTE: Do NOT call USART_Printf_Init() here.
     * printf uses USART1 (PA9/PA10) which is shared with the Core protocol UART.
     * Using printf would corrupt protocol frames. */

    Hardware_Init();

    while (1)
    {
        /* Process MAX30102 FIFO interrupt */
        if (g_max30102_int_flag)
        {
            g_max30102_int_flag = 0;
            Max30102_ProcessInt();
        }

        /* Run PPG algorithm on collected data */
        Max30102_ProcessAlgorithm();

        /* Process incoming Core frames */
        Hardware_ProcessCoreFrame();

        /* Periodic health data report when monitoring */
        if (max30102_ctx.state == HEALTH_STATE_MONITORING)
        {
            report_counter++;

            if (report_counter >= LOOPS_PER_SECOND * max30102_ctx.monitor_interval)
            {
                report_counter = 0;
                Hardware_ReportHealthData();
            }
        }
        else
        {
            report_counter = 0;
        }
    }
}
