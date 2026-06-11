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

int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* NOTE: Do NOT call USART_Printf_Init() here.
     * printf uses USART1 (PA9/PA10) which is shared with the Core protocol UART.
     * Using printf would corrupt protocol frames. */

    Hardware_Init();

    while (1)
    {
        /* Process NFC module data: debounce + card-absent detection */
        Nfc_Process();

        /* Report new card to Core if debounce passed */
        Hardware_ProcessNfcCard();

        /* Handle Core protocol frames (GET_TYPE, NOP, GET_STATUS) */
        Hardware_ProcessCoreFrame();
    }
}
