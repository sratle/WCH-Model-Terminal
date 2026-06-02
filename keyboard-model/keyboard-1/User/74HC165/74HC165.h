/********************************** (C) COPYRIGHT *******************************
 * File Name          : 74HC165.h
 * Description        : 74HC165 shift register driver for key matrix reading.
 *                      5 chips daisy-chained, 40 bits total (39 keys used).
 *********************************************************************************/
#ifndef __74HC165_H
#define __74HC165_H

#include "debug.h"

/* ====================================================================
 * 74HC165 GPIO pin definitions (active-low logic per datasheet)
 * ==================================================================== */

/* PB12 - CP (Clock Pulse) */
#define HC165_CP_PORT       GPIOB
#define HC165_CP_PIN        GPIO_Pin_12

/* PB13 - CE (Clock Enable, active LOW) */
#define HC165_CE_PORT       GPIOB
#define HC165_CE_PIN        GPIO_Pin_13

/* PB14 - PL (Parallel Load, active LOW) */
#define HC165_PL_PORT       GPIOB
#define HC165_PL_PIN        GPIO_Pin_14

/* PB15 - DATA (Q7H serial output from last chip in chain) */
#define HC165_DATA_PORT     GPIOB
#define HC165_DATA_PIN      GPIO_Pin_15

/* ====================================================================
 * 74HC165 chain configuration
 * ==================================================================== */
#define HC165_CHIP_COUNT    5       /* 5 chips daisy-chained */
#define HC165_TOTAL_BITS    40      /* 5 x 8 = 40 bits */
#define HC165_KEY_COUNT     39      /* 39 keys wired (5th chip has 7 keys) */

/* ====================================================================
 * Function declarations
 * ==================================================================== */

/**
 * @brief  Initialize GPIO pins for 74HC165 interface.
 *         CP/CE/PL as push-pull output, DATA as floating input.
 */
void HC165_Init(void);

/**
 * @brief  Read all 40 bits from the daisy-chained 74HC165 shift registers.
 * @param  buf  Output buffer, must hold at least HC165_CHIP_COUNT bytes (5).
 *              buf[0] = first chip in chain (keys 1-8)
 *              buf[4] = last chip in chain (keys 33-39, bit7 unused)
 * @note   Bit ordering within each byte:
 *           buf[n] bit0 = first bit clocked out of that chip
 *           buf[0] bit0 = Key 1 (leftmost key, first in chain)
 *           buf[0] bit7 = Key 8
 *           buf[1] bit0 = Key 9, ...
 *           buf[4] bit6 = Key 39
 *           buf[4] bit7 = unused (no key wired)
 */
void HC165_Read(uint8_t *buf);

#endif /* __74HC165_H */