/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : 
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body.
 *******************************************************************************/

#include "debug.h"
#include "hardware.h"
#include "CH378/CH378.h"
#include "Test/test.h"

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    SystemAndCoreClockUpdate();
    Delay_Init();
    USART_Printf_Init(115200);
    printf("V5F SystemCoreClk:%d\r\n", SystemCoreClock);

    Delay_Ms(500);

#if (Run_Core == Run_Core_V3FandV5F)
    HSEM_FastTake(HSEM_ID0);
    HSEM_ReleaseOneSem(HSEM_ID0, 0);
#endif

    printf("V5F Wake Up\r\n");

    Hardware_V5F_Init();
    test_CH378();

    /* 启动 1kHz 正弦波测试，验证音频通路 */
    Audio_PlaySineStart();

    /* 启用 USART2 接收中断，进入交互式 CLI 模式 */
    Debug_EnableRxIRQ();
    printf("\r\nCLI ready, enter command:\r\n> ");

    while(1)
    {
        Display_Process(&display_g);
        Debug_CLI_Process();
        Delay_Ms(1);
    }
}
