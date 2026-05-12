/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Description        : CH585F Wireless MCU main entry
 *                      - Initializes BLE stack, SPI slave, protocol layer
 *                      - Bridges APP (BLE) <-> Core (SPI) CLI data
 *********************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "gattprofile.h"
#include "peripheral.h"

__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if(defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

__HIGH_CODE
__attribute__((noinline))
void Main_Circulation()
{
    while(1) {
        TMOS_SystemProcess();
    }
}

int main(void)
{
#if(defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

#if(defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif

#ifdef DEBUG
    GPIOA_SetBits(GPIO_Pin_14);
    GPIOPinRemap(ENABLE, RB_PIN_UART0);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
#endif

    PRINT("WCH Wireless Boot\n");
    PRINT("Build: %s %s\n", __DATE__, __TIME__);

    /* BLE stack init */
    CH58x_BLEInit();
    HAL_Init();

    /* GAP role init */
    GAPRole_PeripheralInit();

    /* Application init (includes SPI slave, protocol, profile) */
    Peripheral_Init();

    Main_Circulation();
}
