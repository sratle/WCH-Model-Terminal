/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_hcsr04.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : HC-SR04 ultrasonic ranging driver.
*
*                      Measurement principle:
*                        1. Trig high for >= 10 us starts a burst.
*                        2. Echo goes high for 150 us .. 25 ms (38 ms when
*                           out of range), proportional to distance:
*                           distance_mm = pulse_us * 0.343 / 2
*                                       ~ pulse_us * 17 / 100.
*
*                      TIM3 is a free-running 1 MHz / 16-bit counter
*                      (wraps every 65.5 ms - longer than any echo).
********************************************************************************/
#include "bsp_hcsr04.h"
#include "ch32v30x.h"
#include "debug.h"              /* Delay_Us */

/*=============================================================================
 *  Pin / Timer Map
 *=============================================================================*/

#define HCSR04_TRIG_PORT    GPIOC
#define HCSR04_TRIG_PIN     GPIO_Pin_14     /* PC14 - trigger output     */
#define HCSR04_ECHO_PORT    GPIOC
#define HCSR04_ECHO_PIN     GPIO_Pin_13     /* PC13 - echo input         */

#define HCSR04_TRIG_LOW()   GPIO_ResetBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN)
#define HCSR04_TRIG_HIGH()  GPIO_SetBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN)
#define HCSR04_ECHO_READ()  GPIO_ReadInputDataBit(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN)

#define HCSR04_TIMER_HZ     1000000UL       /* TIM3 counter: 1 us ticks  */

/*=============================================================================
 *  Private Helpers
 *=============================================================================*/

/*********************************************************************
 * @fn      HCSR04_WaitLevel
 *
 * @brief   Wait until Echo reaches the requested level or the timeout
 *          expires.
 *
 * @param   level      - 0 or 1
 * @param   timeout_us - maximum wait in microseconds
 *
 * @return  1 = level reached, 0 = timeout.
 */
static uint8_t HCSR04_WaitLevel(uint8_t level, uint32_t timeout_us)
{
    uint16_t start = (uint16_t)TIM_GetCounter(TIM3);
    while (HCSR04_ECHO_READ() != level) {
        if ((uint16_t)((uint16_t)TIM_GetCounter(TIM3) - start) > timeout_us) {
            return 0;
        }
    }
    return 1;
}

/*=============================================================================
 *  Public API
 *=============================================================================*/

void HCSR04_Init(void)
{
    GPIO_InitTypeDef      gpio;
    TIM_TimeBaseInitTypeDef tim;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* Trig: push-pull output, idle low */
    gpio.GPIO_Pin   = HCSR04_TRIG_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(HCSR04_TRIG_PORT, &gpio);
    HCSR04_TRIG_LOW();

    /* Echo: floating input */
    gpio.GPIO_Pin  = HCSR04_ECHO_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HCSR04_ECHO_PORT, &gpio);

    /* TIM3: 1 MHz free-running up counter, full 16-bit period */
    tim.TIM_Prescaler     = (uint16_t)((SystemCoreClock / 1000) - 1);
    tim.TIM_CounterMode   = TIM_CounterMode_Up;
    tim.TIM_Period        = 0xFFFF;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM3, &tim);
    TIM_Cmd(TIM3, ENABLE);
}

int32_t HCSR04_ReadRawUs(void)
{
    uint16_t start, stop;

    /* Trigger pulse: >= 10 us high */
    HCSR04_TRIG_LOW();
    Delay_Us(2);
    HCSR04_TRIG_HIGH();
    Delay_Us(12);
    HCSR04_TRIG_LOW();

    /* Wait for the echo rising edge (module reacts within ~1 ms) */
    if (!HCSR04_WaitLevel(1, 20000)) {
        return HCSR04_INVALID;
    }
    start = (uint16_t)TIM_GetCounter(TIM3);

    /* Measure the high pulse (max ~38 ms when out of range) */
    if (!HCSR04_WaitLevel(0, HCSR04_TIMEOUT_MS * 1000UL)) {
        return HCSR04_INVALID;
    }
    stop = (uint16_t)TIM_GetCounter(TIM3);

    return (int32_t)(uint16_t)(stop - start);   /* Handles 16-bit wrap  */
}

int32_t HCSR04_ReadMm(void)
{
    int32_t us = HCSR04_ReadRawUs();
    int32_t mm;
    if (us < 0) return HCSR04_INVALID;

    /* mm = us * 0.343 / 2  ~=  us * 17 / 100  (error < 0.3%) */
    mm = (us * 17) / 100;

    /* Out-of-range objects make the module hold Echo high for its
     * maximum pulse (~30-38 ms -> bogus 5-6.5 m readings); clamp them
     * to the spec range so the API only returns plausible values. */
    if (mm > HCSR04_MAX_DISTANCE_MM) return HCSR04_INVALID;

    return mm;
}
