#ifndef __TTP229_H__
#define __TTP229_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* ======================== Pin Assignments ======================== */

/* TTP229 #1: PB15-SDO, PB14-SCL (TP0~TP15, uses TP0~3 + TP12~15) */
#define TTP1_SDO_PORT      GPIOB
#define TTP1_SDO_PIN       GPIO_Pin_15
#define TTP1_SCL_PORT      GPIOB
#define TTP1_SCL_PIN       GPIO_Pin_14

/* TTP229 #2: PB13-SDO, PB12-SCL (TP16~TP31, all 16 used) */
#define TTP2_SDO_PORT      GPIOB
#define TTP2_SDO_PIN       GPIO_Pin_13
#define TTP2_SCL_PORT      GPIOB
#define TTP2_SCL_PIN       GPIO_Pin_12

/* ======================== Constants ======================== */

#define TTP_KEY_COUNT      24      /* Total music keys */
#define TTP_BITS_BYTES     3       /* 24 bits = 3 bytes bitmap */

/*
 * Key bitmap layout (1-based key IDs, bit = key_id - 1):
 *   byte0: bit0=Key1(C4) .. bit7=Key8(G4)
 *   byte1: bit0=Key9(G#4) .. bit7=Key16(D#5)
 *   byte2: bit0=Key17(E5) .. bit7=Key24(B5)
 */

/* ======================== API ======================== */

void    TTP229_Init(void);
void    TTP229_Read(uint8_t key_bitmap[TTP_BITS_BYTES]);

#ifdef __cplusplus
}
#endif

#endif /* __TTP229_H__ */
