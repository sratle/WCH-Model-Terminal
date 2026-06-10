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

/* Millisecond tick counter, incremented by TIM2 ISR */
static volatile uint32_t ms_tick = 0;
static uint32_t last_report_tick = 0;
static uint32_t last_poll_tick = 0;

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef tim;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* 72MHz / 72000 = 1kHz = 1ms per tick */
    TIM_DeInit(TIM2);
    tim.TIM_Period = 1000 - 1;
    tim.TIM_Prescaler = 72 - 1;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tim);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_SetPriority(TIM2_IRQn, 0x80);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        ms_tick++;
    }
}

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* NOTE: Do NOT call USART_Printf_Init() here.
     * printf uses USART1 (PA9/PA10) which is shared with the Core protocol UART.
     * Using printf would corrupt protocol frames. */

    Hardware_Init();
    TIM2_Init();

    last_report_tick = ms_tick;
    last_poll_tick = ms_tick;

    while (1)
    {
        /* Process MAX30102 FIFO interrupt */
        if (g_max30102_int_flag)
        {
            g_max30102_int_flag = 0;
            Max30102_ProcessInt();
        }

        /* Poll FIFO every 10ms as fallback (reference code uses polling) */
        if (ms_tick - last_poll_tick >= 10)
        {
            last_poll_tick = ms_tick;
            Max30102_PollFIFO();
        }

        /* Run PPG algorithm on collected data */
        Max30102_ProcessAlgorithm();

        /* Process incoming Core frames */
        Hardware_ProcessCoreFrame();

        /* Periodic health data report when monitoring */
        if (max30102_ctx.state == HEALTH_STATE_MONITORING)
        {
            uint32_t elapsed = ms_tick - last_report_tick;
            if (elapsed >= (uint32_t)max30102_ctx.monitor_interval * 1000)
            {
                last_report_tick = ms_tick;
                Hardware_ReportHealthData();
            }
        }
        else
        {
            last_report_tick = ms_tick;
        }
    }
}
