/********************************** (C) COPYRIGHT *******************************
* File Name          : platform_time.c
* Author             : E-ink Model Team
* Version            : V1.0.0
* Date               : 2026/07/19
* Description        : Free-running millisecond time base on TIM2.
*
*  TIM2 input clock: HCLK = 144 MHz, PCLK1 = HCLK/2 with the APB1 timer
*  clock doubled back to 144 MHz (standard CH32V307 behaviour when the
*  APB1 prescaler is not 1).
*    Prescaler 14400 -> 10 kHz counter; period 10 -> 1 kHz (1 ms) update.
********************************************************************************/
#include "platform_time.h"
#include "ch32v30x.h"

static volatile uint32_t s_ms_ticks = 0;

void TIM2_IRQHandler(void) __attribute__((interrupt));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        s_ms_ticks++;
    }
}

void Platform_Time_Init(void)
{
    TIM_TimeBaseInitTypeDef tim = {0};

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    tim.TIM_Prescaler    = 14400 - 1;  /* 144 MHz / 14400 = 10 kHz */
    tim.TIM_CounterMode  = TIM_CounterMode_Up;
    tim.TIM_Period       = 10 - 1;     /* 10 kHz / 10 = 1 kHz (1 ms) */
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &tim);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    /* Preemption priority 2: below USART1 (1), above the main loop. */
    NVIC_SetPriority(TIM2_IRQn, 2 << 4);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM_Cmd(TIM2, ENABLE);
}

uint32_t Platform_Millis(void)
{
    return s_ms_ticks;
}
