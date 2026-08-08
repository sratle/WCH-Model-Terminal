/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_buzzer.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : Passive buzzer driver - TIM2 update interrupt toggles
*                      PB15 at twice the requested tone frequency.
*
*                      Timebase: TIM2 clock = 144 MHz on this board.
*                      Prescaler 143 -> 1 MHz counter (1 us resolution).
*                      ARR = 1 MHz / (2 * freq) - 1.
********************************************************************************/
#include "bsp_buzzer.h"
#include "ch32v30x.h"
#include "debug.h"              /* Delay_Ms */

/*=============================================================================
 *  Pin / Timer Map
 *=============================================================================*/

#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_Pin_15     /* PB15 - PNP base, low = sound  */

#define BUZZER_IDLE()   GPIO_SetBits(BUZZER_PORT, BUZZER_PIN)   /* off   */
#define BUZZER_TOGGLE() (BUZZER_PORT->OUTDR ^= BUZZER_PIN)

/* TIM2 counter frequency after the fixed prescaler */
#define BUZZER_TIMER_HZ 1000000UL

/*=============================================================================
 *  Module State
 *=============================================================================*/

static volatile uint8_t s_sounding = 0;

/*=============================================================================
 *  Interrupt Handler
 *=============================================================================*/

/*********************************************************************
 * @fn      TIM2_IRQHandler
 *
 * @brief   Toggle the buzzer pin on every update event while sounding.
 *
 * @return  none
 */
void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        if (s_sounding) {
            BUZZER_TOGGLE();
        }
    }
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void BUZZER_Init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    NVIC_InitTypeDef nvic;

    /* PB15: push-pull output, idle high (PNP off) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin   = BUZZER_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(BUZZER_PORT, &gpio);
    BUZZER_IDLE();

    /* TIM2: 1 MHz up-counter, update interrupt, stopped for now */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    tim.TIM_Prescaler     = (uint16_t)((SystemCoreClock / BUZZER_TIMER_HZ) - 1);
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    tim.TIM_Period        = 100 - 1;            /* Placeholder 5 kHz     */
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &tim);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel                   = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 1;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

void BUZZER_On(uint32_t freq_hz)
{
    uint32_t arr;

    if (freq_hz == 0) {
        BUZZER_Off();
        return;
    }

    /* Toggle twice per period -> interrupt rate = 2 * freq */
    arr = BUZZER_TIMER_HZ / (2 * freq_hz);
    if (arr == 0) arr = 1;
    if (arr > 65536) arr = 65536;

    BUZZER_IDLE();
    TIM_Cmd(TIM2, DISABLE);
    TIM_SetAutoreload(TIM2, (uint16_t)(arr - 1));
    TIM_SetCounter(TIM2, 0);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    s_sounding = 1;
    TIM_Cmd(TIM2, ENABLE);
}

void BUZZER_Off(void)
{
    s_sounding = 0;
    TIM_Cmd(TIM2, DISABLE);
    BUZZER_IDLE();
}

void BUZZER_Beep(uint32_t freq_hz, uint32_t duration_ms)
{
    BUZZER_On(freq_hz);
    Delay_Ms(duration_ms);
    BUZZER_Off();
}

uint8_t BUZZER_IsOn(void)
{
    return s_sounding;
}
