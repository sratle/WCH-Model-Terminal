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

/* Millisecond tick counter, incremented by TIM2 ISR */
static volatile uint32_t ms_tick = 0;

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

    while (1)
    {
        /* Process incoming Core frames (GET_TYPE / NOP / SUB_GET_STATUS) */
        Hardware_ProcessCoreFrame();

        /* Debounce ISR-parsed NFC frames and detect card removal */
        Nfc_Process(ms_tick);

        /* Report debounced card detection event to Core */
        Hardware_ProcessNfcCard();
    }
}
