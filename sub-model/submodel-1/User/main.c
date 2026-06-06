/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Submodel-1 (Fingerprint) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      UART2 (PA2/PA3) @ 57600 for fingerprint sensor (Syno protocol).
 *                      PA4 TOUCHOUT interrupt for finger detection.
 *********************************************************************************/

#include "debug.h"
#include "../Common/Common/hardware.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Uart/uart_fp.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/Fingerprint/fingerprint.h"

extern volatile uint8_t g_touchout_flag;

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
        if (g_touchout_flag)
        {
            g_touchout_flag = 0;

            if (fp_ctx.state == FP_STATE_IDLE || fp_ctx.state == FP_STATE_SLEEPING)
            {
                fp_ctx.state = FP_STATE_IDLE;
                Fp_AutoIdentify(FP_SECURITY_MEDIUM);
            }
        }

        if (uart_fp_rx_ready)
        {
            uart_fp_rx_ready = 0;
            Fp_ProcessRxData((const uint8_t *)uart_fp_rx_buf, uart_fp_rx_len);
            uart_fp_rx_len = 0;
        }

        Hardware_ProcessFpResponse();
        Hardware_ProcessCoreFrame();
    }
}
