/********************************** (C) COPYRIGHT *******************************
 * File Name          : hardware.c
 *******************************************************************************/

#include "hardware.h"
#include "spi_slave.h"
#include "protocol.h"
#include "bt_peripheral.h"

tmosTaskID g_spi_task_id = INVALID_TASK_ID;
tmosTaskID g_bt_task_id  = INVALID_TASK_ID;

/*********************************************************************
 * @fn      Hardware_UART1Init
 */
void Hardware_UART1Init(void)
{
    GPIOA_SetBits(GPIO_Pin_9);
    GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
}

/*********************************************************************
 * @fn      Hardware_SPI0Init
 */
void Hardware_SPI0Init(void)
{
    SPI_Slave_Init();
}

/*********************************************************************
 * @fn      Hardware_GPIOInit
 */
void Hardware_GPIOInit(void)
{
    /* SPI0 default pins: PA12(MOSI), PA13(MISO), PA14(SCK), PA15(NSS)
     * The peripheral library already configures these when SPI0_SlaveInit
     * is called; no extra GPIO setup needed here unless remapping.
     */
}

/*********************************************************************
 * @fn      Hardware_GetMillis
 */
uint32_t Hardware_GetMillis(void)
{
    return SYS_GetSysTickCnt() / (FREQ_SYS / 1000);
}

/*********************************************************************
 * @fn      SPI_ProcessEvent
 */
static uint16_t SPI_ProcessEvent(uint8_t task_id, uint16_t events)
{
    if(events & SPI_PROCESS_EVT)
    {
        /* SPI protocol parsing is handled in bt_core polling */
        return (events ^ SPI_PROCESS_EVT);
    }
    return 0;
}

/*********************************************************************
 * @fn      Hardware_Init
 */
void Hardware_Init(void)
{
    g_spi_task_id = TMOS_ProcessEventRegister(SPI_ProcessEvent);
    g_bt_task_id  = TMOS_ProcessEventRegister(BT_Peripheral_ProcessEvent);

    Hardware_GPIOInit();
    Hardware_UART1Init();
    Hardware_SPI0Init();
    Protocol_Init();
}
