/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_led.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : On-board user LED driver (PC0, active low).
********************************************************************************/
#include "bsp_led.h"
#include "ch32v30x.h"

#define LED_PORT        GPIOC
#define LED_PIN         GPIO_Pin_0

static uint8_t s_state = 0;             /* Software-tracked LED state   */

#if LED_ACTIVE_LOW
#define LED_PIN_ON()    GPIO_ResetBits(LED_PORT, LED_PIN)
#define LED_PIN_OFF()   GPIO_SetBits(LED_PORT, LED_PIN)
#else
#define LED_PIN_ON()    GPIO_SetBits(LED_PORT, LED_PIN)
#define LED_PIN_OFF()   GPIO_ResetBits(LED_PORT, LED_PIN)
#endif

void LED_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    gpio.GPIO_Pin   = LED_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(LED_PORT, &gpio);

    s_state = 0;
    LED_PIN_OFF();
}

void LED_On(void)
{
    s_state = 1;
    LED_PIN_ON();
}

void LED_Off(void)
{
    s_state = 0;
    LED_PIN_OFF();
}

void LED_Toggle(void)
{
    if (s_state) LED_Off();
    else         LED_On();
}

void LED_Set(uint8_t on)
{
    if (on) LED_On();
    else    LED_Off();
}

uint8_t LED_Get(void)
{
    return s_state;
}
