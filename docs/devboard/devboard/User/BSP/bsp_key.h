/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_key.h
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : 16-key driver using two cascaded 74HC165 shift
*                      registers (U3/U4). GPIO bit-banged read.
*                      Wiring: PL=PB14  CE=PB13  CP=PB12  DATA=PD8
*                      (DATA is a flying wire - not routed on the PCB)
*                      Keys S1-S16, pressed = contact to GND (active low).
*
*                      Typical use:
*                          KEY_Init();
*                          uint16_t st = KEY_Scan();       // debounced
*                          if (st & KEY_1) { ... }          // S1 pressed
********************************************************************************/
#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*=============================================================================
 *  Key Bit Masks
 *  bit0 = physical key 1 (top row, leftmost) ... bit15 = key 16,
 *  numbered row-major (left to right, top to bottom) across the PCB.
 *=============================================================================*/

#define KEY_NONE    0x0000
#define KEY_1       (1U << 0)
#define KEY_2       (1U << 1)
#define KEY_3       (1U << 2)
#define KEY_4       (1U << 3)
#define KEY_5       (1U << 4)
#define KEY_6       (1U << 5)
#define KEY_7       (1U << 6)
#define KEY_8       (1U << 7)
#define KEY_9       (1U << 8)
#define KEY_10      (1U << 9)
#define KEY_11      (1U << 10)
#define KEY_12      (1U << 11)
#define KEY_13      (1U << 12)
#define KEY_14      (1U << 13)
#define KEY_15      (1U << 14)
#define KEY_16      (1U << 15)
#define KEY_ALL     0xFFFF

/*=============================================================================
 *  Driver API
 *=============================================================================*/

/*********************************************************************
 * @fn      KEY_Init
 *
 * @brief   Configure PL/CE/CP as outputs and DATA as input,
 *          release the shift registers to the idle state.
 *
 * @return  none
 */
void KEY_Init(void);

/*********************************************************************
 * @fn      KEY_ReadRaw
 *
 * @brief   Read the instantaneous state of all 16 keys.
 *
 * @return  16-bit mask, bit0 = key 1 ... bit15 = key 16 (physical
 *          row-major order); bit SET = key is pressed (active-low
 *          inputs are inverted).
 */
uint16_t KEY_ReadRaw(void);

/*********************************************************************
 * @fn      KEY_Scan
 *
 * @brief   Debounced read: samples the keys twice ~5 ms apart and
 *          returns only the bits that are stable in both samples.
 *          Blocks for ~5 ms.
 *
 * @return  Debounced 16-bit pressed mask (bit set = pressed).
 */
uint16_t KEY_Scan(void);

/*********************************************************************
 * @fn      KEY_IsPressed
 *
 * @brief   Test helper: is key n (1..16) set in a state mask?
 *
 * @param   state - mask returned by KEY_ReadRaw / KEY_Scan
 * @param   key   - key number, 1..16
 *
 * @return  1 if pressed, 0 otherwise.
 */
uint8_t KEY_IsPressed(uint16_t state, uint8_t key);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_KEY_H */
