/******************************************************************************/
#ifndef __CONFIG_H
#define __CONFIG_H

#define CHIP_ID                 ID_CH585

#include "CH58x_common.h"

/*********************************************************************
 * System Configuration
 */
#ifndef SYSCLK_FREQ
#define SYSCLK_FREQ             60000000    /* 60 MHz */
#endif

/*********************************************************************
 * Debug Configuration
 * Submodel-5 does not use debug UART. UART0 is dedicated to
 * Core protocol communication. Leave DEBUG undefined.
 */
/* #define DEBUG                   Debug_UART1 */

/*********************************************************************
 * RGB Module Configuration
 */
#define RGB_LED_COUNT           49          /* 7x7 WS2812 matrix */
#define RGB_SPI_CLOCK_DIV       24          /* 60MHz/24 = 2.5MHz for WS2812 */

#endif /* __CONFIG_H */
