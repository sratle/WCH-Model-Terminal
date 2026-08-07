/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Submodel-6 (VL53L0X Laser Ranging) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      Software I2C (PB14-SCL/PB15-SDA) for VL53L0X-V2 sensor.
 *                      PB12 XSHUT, PB13 GPIO1 (data ready interrupt).
 *                      TIM2 1kHz tick for report scheduling.
 *********************************************************************************/

#include "debug.h"
#include "../Common/Common/hardware.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/VL53L0X/vl53l0x.h"

/* Millisecond tick counter, incremented by TIM2 ISR */
static volatile uint32_t ms_tick = 0;

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void TIM2_Init(void)
{
    TIM_TimeBaseInitTypeDef tim;
    RCC_ClocksTypeDef clocks;
    uint32_t tim_clk;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* Derive timer clock from actual runtime configuration.
     * NOTE: this board may run on HSI 8MHz if HSE fails to start
     * (SetSysClockTo72_HSE falls through silently), so PSC/ARR must
     * NOT be hardcoded for 72MHz. APB1 prescaler != 1 -> timer clock
     * is PCLK1 x 2. */
    RCC_GetClocksFreq(&clocks);
    tim_clk = clocks.PCLK1_Frequency;
    if ((RCC->CFGR0 & RCC_PPRE1) != RCC_PPRE1_DIV1)
        tim_clk *= 2;

    /* tim_clk / (PSC+1) = 10kHz; /10 = 1kHz = 1ms per tick */
    TIM_DeInit(TIM2);
    tim.TIM_Period = 10 - 1;
    tim.TIM_Prescaler = (uint16_t)(tim_clk / 10000 - 1);
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
        Hardware_ProcessCoreFrame();
        Hardware_ProcessRanging(ms_tick);
    }
}
