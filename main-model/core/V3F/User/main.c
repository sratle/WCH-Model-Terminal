/********************************** (C) COPYRIGHT *******************************
 * File Name          : main_v3f.c
 * Author             :
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body for V3F.
 *
 * V3F 仅负责上电初始化时钟、唤醒 V5F，然后进入 STOP 低功耗模式。
 * 所有模块初始化和业务逻辑由 V5F 统一管理。
 *******************************************************************************/

#include "debug.h"

int main(void)
{
    SystemInit();
    SystemAndCoreClockUpdate();
    Delay_Init();

    USART_Printf_Init(921600);
    printf("V3F: Waking up V5F...\r\n");

#if (Run_Core == Run_Core_V3FandV5F)
    NVIC_WakeUp_V5F(Core_V5F_StartAddr);
    HSEM_ITConfig(HSEM_ID0, ENABLE);
    NVIC->SCTLR |= 1<<4;
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
    PWR_EnterSTOPMode(PWR_Regulator_ON, PWR_STOPEntry_WFE);
    HSEM_ClearFlag(HSEM_ID0);
#endif

    printf("V3F: Entering STOP mode, V5F running.\r\n");

    /* V3F 不再执行任何业务逻辑，进入无限低功耗 */
    while(1)
    {
        __WFI();
    }
}
