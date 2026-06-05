/*********************************************************************
 * File Name          : rgb_main.c
 * Description        : Submodel-5 (RGB LED) main entry.
 *                      CH585F MCU, simple superloop @ 50fps.
 *                      - UART0 (PA14-TX, PA15-RX) @ 230400 for Core
 *                      - GPIO PB14 for WS2812 LED chain (bit-bang)
 *                      - 49x WS2812 7x7 matrix, 4 effect modes
 *
 *                      Frame timing: render(~0ms) + send(~1.5ms)
 *                      + delay(~18ms) = 20ms → 50fps
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
