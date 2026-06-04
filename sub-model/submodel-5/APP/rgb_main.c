/*********************************************************************
 * File Name          : rgb_main.c
 * Description        : Submodel-5 (RGB LED) main entry.
 *                      CH585F MCU, no BLE/TMOS, simple superloop.
 *                      - UART0 (PA14-TX, PA15-RX) for Core protocol
 *                      - SPI0 MOSI (PB14) for WS2812 LED chain
 *                      - 49x WS2812 7x7 matrix, 4 effect modes
 *
 *                      Frame rate controlled by NOP delays inside
 *                      App_UpdateEffect() (see rgb_app.c).
 *********************************************************************/

#include "CH58x_common.h"
#include "rgb_app.h"
#include "ws2812.h"
#include "effect.h"

int main(void)
{
    SetSysClock(SYSCLK_FREQ);
    App_UART_Init();
    App_Init();

    while (1) {
        App_ProcessUART();
        App_UpdateEffect();
    }
}
