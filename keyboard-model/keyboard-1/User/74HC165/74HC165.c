/********************************** (C) COPYRIGHT *******************************
 * File Name          : 74HC165.c
 * Description        : 74HC165 shift register driver for key matrix reading.
 *                      5 chips daisy-chained via SPI bit-bang on GPIOB.
 *********************************************************************************/
#include "74HC165.h"

/* ====================================================================
 * GPIO convenience macros
 * ==================================================================== */
#define HC165_CP_HIGH()     GPIO_SetBits(HC165_CP_PORT, HC165_CP_PIN)
#define HC165_CP_LOW()      GPIO_ResetBits(HC165_CP_PORT, HC165_CP_PIN)

#define HC165_CE_HIGH()     GPIO_SetBits(HC165_CE_PORT, HC165_CE_PIN)
#define HC165_CE_LOW()      GPIO_ResetBits(HC165_CE_PORT, HC165_CE_PIN)

#define HC165_PL_HIGH()     GPIO_SetBits(HC165_PL_PORT, HC165_PL_PIN)
#define HC165_PL_LOW()      GPIO_ResetBits(HC165_PL_PORT, HC165_PL_PIN)

#define HC165_READ_DATA()   GPIO_ReadInputDataBit(HC165_DATA_PORT, HC165_DATA_PIN)

/*********************************************************************
 * @fn      HC165_Init
 *
 * @brief   Initialize GPIO pins for 74HC165 interface.
 *          CP/CE/PL -> push-pull output, DATA -> floating input.
 *
 * @return  none
 *********************************************************************/
void HC165_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Enable GPIOB clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* PB12 (CP), PB13 (CE), PB14 (PL) -> push-pull output */
    GPIO_InitStructure.GPIO_Pin = HC165_CP_PIN | HC165_CE_PIN | HC165_PL_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB15 (DATA) -> floating input */
    GPIO_InitStructure.GPIO_Pin = HC165_DATA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* Set idle state: CP=LOW, CE=HIGH (disabled), PL=HIGH (not loading) */
    HC165_CP_LOW();
    HC165_CE_HIGH();
    HC165_PL_HIGH();
}

/*********************************************************************
 * @fn      HC165_Read
 *
 * @brief   Read 40 bits from 5 daisy-chained 74HC165 shift registers.
 *          CE held LOW for entire read; adequate delays for 5-chip
 *          propagation.  buf[n] bit j maps to physical switch inputs.
 *
 * @param   buf  Output buffer, at least HC165_CHIP_COUNT (5) bytes.
 *
 * @note    HC165_Read() inverts bits: buf=1 means key pressed,
 *          buf=0 means key released.
 *
 * @return  none
 *********************************************************************/
void HC165_Read(uint8_t *buf)
{
    uint8_t i, j;

    /* Step 1: Parallel load - latch all 5 chips simultaneously.
     * PL LOW for sufficient time (>= 100ns) to ensure clean latch
     * across all chips including PCB trace delays. */
    HC165_PL_LOW();
    Delay_Us(1);
    HC165_PL_HIGH();
    Delay_Us(1);

    /* Step 2: Enable clock for entire shift-out sequence.
     * CE stays LOW throughout all 40 bits. */
    HC165_CE_LOW();
    Delay_Us(1);

    /* Step 3: Clock out 40 bits (5 chips x 8 bits).
     * CRITICAL: 74HC165 presents data on Q7 immediately after PL.
     * Must READ first, THEN pulse CP to shift the next bit out.
     *   After PL:  Q7 = D7  (read before any clock)
     *   After CP↑: Q7 = D6  (read before next clock)
     *   After CP↑: Q7 = D5  ...
     * So buf[n] bit j = chip n's D(7-j). */
    for (i = 0; i < HC165_CHIP_COUNT; i++)
    {
        buf[i] = 0;
        for (j = 0; j < 8; j++)
        {
            /* Sample data BEFORE clock (current bit is already on Q7) */
            if (HC165_READ_DATA() != Bit_SET)
            {
                buf[i] |= (uint8_t)(1 << j);
            }

            /* Rising edge: shift next bit onto Q7 */
            HC165_CP_HIGH();
            Delay_Us(1);  /* Wait for signal to propagate through chain */
            HC165_CP_LOW();
            Delay_Us(1);  /* Wait before next sample */
        }
    }

    /* Step 4: Disable clock */
    HC165_CE_HIGH();
}