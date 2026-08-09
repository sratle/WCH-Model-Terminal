/********************************** (C) COPYRIGHT *******************************
* File Name          : bsp_key.c
* Author             : WCH-DevBoard Team
* Version            : V1.0.0
* Date               : 2026/08/08
* Description        : 74HC165 x2 cascaded key reader.
*                      Timing reference: keyboard-model/keyboard-1
*                      (74HC165.c) - "sample first, then clock".
*
*                      Shift-out order on this board (see schematic):
*                          sample 0      = U3.D7 = silk S8
*                          ...           ...
*                          sample 7      = U3.D0 = silk S1
*                          sample 8      = U4.D7 = silk S16
*                          ...           ...
*                          sample 15     = U4.D0 = silk S9
*                      On the PCB the keys sit in exactly that order
*                      (top row left->right = silk S8..S1), so the sample
*                      index already IS the physical row-major position:
*                      API bit i = sample i = physical key i+1 (top-left
*                      = key 1). No further remapping is needed.
********************************************************************************/
#include "bsp_key.h"
#include "ch32v30x.h"
#include "debug.h"              /* Delay_Us / Delay_Ms */

/*=============================================================================
 *  Pin Map (see docs/devboard/HARDWARE.md)
 *=============================================================================*/

#define KEY_PL_PORT     GPIOB
#define KEY_PL_PIN      GPIO_Pin_14     /* PB14 - SH/LD, low = load      */
#define KEY_CE_PORT     GPIOB
#define KEY_CE_PIN      GPIO_Pin_13     /* PB13 - CLK INH, low = enable  */
#define KEY_CP_PORT     GPIOB
#define KEY_CP_PIN      GPIO_Pin_12     /* PB12 - shift clock, rising    */
#define KEY_DATA_PORT   GPIOD
#define KEY_DATA_PIN    GPIO_Pin_8      /* PD8  - serial data from U3.Q7
                                          * (flying wire, not on PCB)    */

#define KEY_PL_LOW()    GPIO_ResetBits(KEY_PL_PORT, KEY_PL_PIN)
#define KEY_PL_HIGH()   GPIO_SetBits(KEY_PL_PORT, KEY_PL_PIN)
#define KEY_CE_LOW()    GPIO_ResetBits(KEY_CE_PORT, KEY_CE_PIN)
#define KEY_CE_HIGH()   GPIO_SetBits(KEY_CE_PORT, KEY_CE_PIN)
#define KEY_CP_LOW()    GPIO_ResetBits(KEY_CP_PORT, KEY_CP_PIN)
#define KEY_CP_HIGH()   GPIO_SetBits(KEY_CP_PORT, KEY_CP_PIN)
#define KEY_READ_DATA() GPIO_ReadInputDataBit(KEY_DATA_PORT, KEY_DATA_PIN)

#define KEY_COUNT       16
#define KEY_DEBOUNCE_MS 5

/*=============================================================================
 *  Public API
 *=============================================================================*/

void KEY_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD,
                           ENABLE);

    /* PL / CE / CP: push-pull outputs */
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Pin   = KEY_PL_PIN | KEY_CE_PIN | KEY_CP_PIN;
    GPIO_Init(GPIOB, &gpio);

    /* DATA: floating input (74HC165 Q7 is a push-pull output) */
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin  = KEY_DATA_PIN;
    GPIO_Init(GPIOD, &gpio);

    /* Idle levels: PL/CE high, CP low */
    KEY_PL_HIGH();
    KEY_CE_HIGH();
    KEY_CP_LOW();
}

uint16_t KEY_ReadRaw(void)
{
    uint16_t state = 0;
    uint8_t  i;

    /* Step 1: pulse PL low -> parallel-load both chips */
    KEY_PL_LOW();
    Delay_Us(1);
    KEY_PL_HIGH();
    Delay_Us(1);

    /* Step 2: enable the shift clock for the whole sequence */
    KEY_CE_LOW();
    Delay_Us(1);

    /* Step 3: clock out 16 bits.
     * After PL, Q7 already carries the first bit, so SAMPLE FIRST,
     * then pulse CP to bring the next bit out (see keyboard-1 notes).
     * Sample i is physical key (i+1) in row-major order -> bit i. */
    for (i = 0; i < KEY_COUNT; i++) {
        if (KEY_READ_DATA() == Bit_RESET) {     /* Low = pressed */
            state |= (uint16_t)(1U << i);
        }
        KEY_CP_HIGH();
        Delay_Us(1);
        KEY_CP_LOW();
        Delay_Us(1);
    }

    /* Step 4: back to idle */
    KEY_CE_HIGH();

    return state;
}

uint16_t KEY_Scan(void)
{
    uint16_t a = KEY_ReadRaw();
    uint16_t b;
    Delay_Ms(KEY_DEBOUNCE_MS);
    b = KEY_ReadRaw();
    return (uint16_t)(a & b);       /* Keep only bits stable in both */
}

uint8_t KEY_IsPressed(uint16_t state, uint8_t key)
{
    if (key < 1 || key > KEY_COUNT) return 0;
    return (state & (uint16_t)(1U << (key - 1))) ? 1 : 0;
}
