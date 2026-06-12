/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : Submodel-6 (VL53L0X Laser Ranging) main program.
 *                      CH32V103C8T6, 72MHz HSE.
 *                      UART1 (PA9/PA10) @ 230400 for Core protocol communication.
 *                      Software I2C (PB14/PB15) for VL53L0X-V2 sensor.
 *                      PB12 XSHUT, PB13 GPIO1 (data ready interrupt).
 *********************************************************************************/

#include "debug.h"
#include "../Common/Common/hardware.h"
#include "../Common/Common/Uart/uart_core.h"
#include "../Common/Common/Protocol/protocol.h"
#include "../Common/Common/VL53L0X/vl53l0x.h"

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* DEBUG: enable printf on USART1 (PA9/PA10) for init diagnostics.
     * This shares the same UART as the Core protocol — safe during init
     * before Core starts polling. Remove after debugging. */
    USART_Printf_Init(230400);

    printf("=== Submodel-6 boot ===\r\n");

    Hardware_Init();

    printf("=== Init done, IR init=%d ===\r\n", VL53L0X_IsInitialized());

    while (1)
    {
        Hardware_ProcessCoreFrame();
    }
}
