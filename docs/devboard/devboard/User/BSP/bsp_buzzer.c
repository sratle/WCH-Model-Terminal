/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_buzzer.c
* Author             : WCH-DevBoard Team
* Version            : V1.1.0
* Date               : 2026/08/08
* Description        : Passive buzzer driver - TIM1_CH3N hardware PWM on PB15.
*
*                      Tone frequency = TIM1_CLK / ((PSC+1) * (ARR+1)),
*                      TIM1 clock = 144 MHz (APB2). The prescaler is chosen
*                      at runtime so ARR always fits in 16 bits.
*                      Duty is fixed at 50%; when stopped, PB15 falls back
*                      to a plain GPIO output driven high (PNP cut off).
********************************************************************************/
#include "bsp_buzzer.h"
#include "ch32v30x.h"
#include "debug.h"              /* Delay_Ms */

/*=============================================================================
 *  Pin / Timer Map
 *=============================================================================*/

#define BUZZER_PORT     GPIOB
#define BUZZER_PIN      GPIO_Pin_15     /* PB15 = TIM1_CH3N              */

#define BUZZER_DUTY_PCT 50              /* Fixed duty cycle (%)          */

/*=============================================================================
 *  Module State
 *=============================================================================*/

static uint8_t s_sounding = 0;

/*=============================================================================
 *  Private Helpers
 *=============================================================================*/

/*********************************************************************
 * @fn      BUZZER_PinGpio
 *
 * @brief   PB15 as plain push-pull output, driven high (buzzer off).
 *
 * @return  none
 */
static void BUZZER_PinGpio(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = BUZZER_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(BUZZER_PORT, &gpio);
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

/*********************************************************************
 * @fn      BUZZER_PinPwm
 *
 * @brief   PB15 as TIM1_CH3N alternate function output.
 *
 * @return  none
 */
static void BUZZER_PinPwm(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin   = BUZZER_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(BUZZER_PORT, &gpio);
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void BUZZER_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    BUZZER_PinGpio();
    s_sounding = 0;
}

void BUZZER_On(uint32_t freq_hz)
{
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef       oc;
    uint32_t psc, arr;
    uint32_t timer_clk = SystemCoreClock;   /* TIM1 on APB2 = 144 MHz   */

    if (freq_hz == 0) {
        BUZZER_Off();
        return;
    }

    /* Pick the smallest prescaler that keeps ARR within 16 bits:
     *     (PSC+1)*(ARR+1) = timer_clk / freq,  ARR+1 <= 65536  */
    psc = timer_clk / (freq_hz * 65536UL);
    arr = timer_clk / ((psc + 1) * freq_hz);
    if (arr == 0) arr = 1;
    if (arr > 65536UL) arr = 65536UL;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    tim.TIM_Prescaler         = (uint16_t)psc;
    tim.TIM_CounterMode       = TIM_CounterMode_Up;
    tim.TIM_Period            = (uint16_t)(arr - 1);
    tim.TIM_ClockDivision     = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &tim);

    /* Channel 3 complementary output (CH3N = PB15), PWM1, 50% duty */
    oc.TIM_OCMode       = TIM_OCMode_PWM1;
    oc.TIM_OutputState  = TIM_OutputState_Disable;
    oc.TIM_OutputNState = TIM_OutputNState_Enable;
    oc.TIM_Pulse        = (uint16_t)(arr * BUZZER_DUTY_PCT / 100);
    oc.TIM_OCPolarity   = TIM_OCPolarity_High;
    oc.TIM_OCNPolarity  = TIM_OCNPolarity_High;
    oc.TIM_OCIdleState  = TIM_OCIdleState_Set;
    oc.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC3Init(TIM1, &oc);
    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);       /* Advanced timer: MOE bit  */
    BUZZER_PinPwm();
    TIM_Cmd(TIM1, ENABLE);

    s_sounding = 1;
}

void BUZZER_Off(void)
{
    TIM_Cmd(TIM1, DISABLE);
    TIM_CtrlPWMOutputs(TIM1, DISABLE);
    BUZZER_PinGpio();                       /* Back to GPIO high = off  */
    s_sounding = 0;
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
