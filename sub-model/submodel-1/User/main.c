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

/* 注册超时阈值：主循环无新应答迭代次数，约对应 1~2 秒 */
#define ENROLL_IDLE_TIMEOUT  500000UL

/* 验证超时阈值：AutoIdentify 无应答时的超时保护 */
#define IDENTIFY_TIMEOUT     1000000UL

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
        /* 手指检测：仅在 IDLE/SLEEPING 时触发验证 */
        if (g_touchout_flag)
        {
            g_touchout_flag = 0;

            if (fp_ctx.state == FP_STATE_IDLE || fp_ctx.state == FP_STATE_SLEEPING)
            {
                fp_ctx.state = FP_STATE_IDLE;
                fp_ctx.enroll_idle_counter = 0;
                Fp_AutoIdentify(FP_SECURITY_MEDIUM);
            }
        }

        /* 安全读取 UART2 接收缓冲区：
         * 仅屏蔽 USART2 中断，不影响 USART1 (Core) 通信 */
        if (uart_fp_rx_ready)
        {
            uint16_t rx_len;

            USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
            USART_ITConfig(USART2, USART_IT_IDLE, DISABLE);

            uart_fp_rx_ready = 0;
            rx_len = uart_fp_rx_len;
            uart_fp_rx_len = 0;

            USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
            USART_ITConfig(USART2, USART_IT_IDLE, ENABLE);

            if (rx_len > 0)
            {
                Fp_ProcessRxData((const uint8_t *)uart_fp_rx_buf, rx_len);
            }
        }

        Hardware_ProcessFpResponse();
        Hardware_ProcessCoreFrame();

        /* 注册超时检测：当注册进行中且已收到应答，
         * 若长时间无新应答则视为注册完成 */
        if (fp_ctx.state == FP_STATE_ENROLLING && fp_ctx.enroll_resp_count > 0)
        {
            fp_ctx.enroll_idle_counter++;
            if (fp_ctx.enroll_idle_counter > ENROLL_IDLE_TIMEOUT)
            {
                fp_ctx.state = FP_STATE_IDLE;
                Hardware_SendEnrollResult(1, 0);
                fp_ctx.enroll_idle_counter = 0;
            }
        }

        /* 验证超时保护：AutoIdentify 长时间无应答时恢复 IDLE，
         * 防止状态卡死导致后续触摸失效 */
        if (fp_ctx.state == FP_STATE_IDENTIFYING)
        {
            fp_ctx.enroll_idle_counter++;
            if (fp_ctx.enroll_idle_counter > IDENTIFY_TIMEOUT)
            {
                fp_ctx.state = FP_STATE_IDLE;
                fp_ctx.enroll_idle_counter = 0;
            }
        }
    }
}
