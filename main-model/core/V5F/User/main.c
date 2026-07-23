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
#include "CH585F/ch585f_bt.h"
#include "CS43131/cs43131.h"
#include "Config/config.h"
#include "Key/key.h"

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
    USART_Printf_Init(921600);
    printf("V5F SystemCoreClk:%d\r\n", SystemCoreClock);

    Delay_Ms(500);

    printf("V5F Wake Up\r\n");

    Hardware_Init();
    CH585F_BT_Init();

    /* 启用 USART2 接收中断，进入交互式 CLI 模式 */
    Debug_EnableRxIRQ();
    printf("\r\nCLI ready, enter command:\r\n> ");

    while(1)
    {
        Display_Process(&display_g);
        Display_SyncStatus(&display_g);   /* 主动推送 Core 侧状态变化，不依赖收到 Display 帧 */
        Audio_Process();
        Debug_CLI_Process();
        CH585F_BT_Poll();
        Key_PollAndProcess();
        Keyboard_Process(&keyboard_g);
        Power_Process(&power_g);
        Submodels_Process(&submodels_g[0]);
        Submodels_Process(&submodels_g[1]);
        Submodels_Process(&submodels_g[2]);
        CH9350_Process(&ch9350_g);
        Hardware_Heartbeat();
        Delay_Ms(1);
    }
}
