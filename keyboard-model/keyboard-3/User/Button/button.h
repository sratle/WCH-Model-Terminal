#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

#define BUTTON_COUNT         12
#define BUTTON_BITS_BYTES    2    /* 12 bits = 2 bytes bitmap */
#define BUTTON_DEBOUNCE_CNT  1    /* consecutive same reads to confirm (10ms @ 10ms scan) */

/*
 * Button bitmap layout (1-based button IDs, bit = button_id - 1):
 *   byte0: bit0=KEY1 .. bit7=KEY8
 *   byte1: bit0=KEY9 .. bit3=KEY12, bit4~7=reserved(0)
 */

void    Button_Init(void);
void    Button_Scan(void);
void    Button_GetBitmap(uint8_t bitmap[BUTTON_BITS_BYTES]);

#ifdef __cplusplus
}
#endif

#endif /* __BUTTON_H__ */
