/*********************************************************************
 * File Name          : rgb_main.c
 * Description        : Submodel-5 (RGB LED) main entry.
 *                      CH585F MCU, no BLE/TMOS, simple superloop.
 *                      - UART0 (PA14-TX, PA15-RX) for Core protocol
 *                      - SPI0 MOSI (PB14) for WS2812 LED chain
 *                      - 49x WS2812 7x7 matrix, 4 effect modes
 *********************************************************************/

#include "CH58x_common.h"
#include "rgb_app.h"
#include "ws2812.h"
#include "effect.h"

int main(void)
{
    /* System clock init (from StdPeriphDriver) */
    SetSysClock(SYSCLK_FREQ);

    /* SysTick 1ms counter init */
    App_SysTick_Init();

    /* UART0 init: PA14-TX, PA15-RX, 115200 8-N-1, RX interrupt */
    App_UART_Init();

    /* Application init: protocol, WS2812, effect engine */
    App_Init();

    /* ---- Super loop ---- */
    while (1) {
        /* Process incoming UART bytes from Core */
        App_ProcessUART();

        /* Update LED effect engine */
        App_UpdateEffect();
    }
}
