/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : CH585F Wireless MCU main entry
 *******************************************************************************/

#include "CH58x_common.h"
#include "CONFIG.h"
#include "hardware.h"
#include "bt_core.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

/*********************************************************************
 * @fn      main
 */
int main(void)
{
    /* Clock */
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    /* Debug UART1 */
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    PRINT("\n=====================================\n");
    PRINT("WCH Terminal Wireless (CH585F) Boot\n");
    PRINT("Ver: 0.1.0\n");
    PRINT("=====================================\n");

    /* Hardware layer: SPI Slave, Protocol, UART */
    Hardware_Init();

    /* BLE stack */
    BT_Core_Init();

    /* Enable global interrupts */
    PFIC_EnableAllIRQ();

    /* Enter TMOS main loop */
    while(1)
    {
        TMOS_SystemProcess();
        BT_Core_Poll();
    }
}
