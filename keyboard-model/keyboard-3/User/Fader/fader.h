#ifndef __FADER_H__
#define __FADER_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

#define FADER_COUNT         3
#define FADER_FILTER_SIZE   8      /* Moving average window */
#define FADER_CHANGE_THRESHOLD 16  /* Minimum change to trigger update */

/*
 * Fader order (left to right): FADER_L (PA3), FADER_M (PA1), FADER_R (PA2)
 * Output values: 0~65535 (uint16), 0=lowest, 65535=highest
 */

void     Fader_Init(void);
void     Fader_Scan(void);
void     Fader_GetValues(uint16_t values[FADER_COUNT]);
uint8_t  Fader_HasChanged(void);

#ifdef __cplusplus
}
#endif

#endif /* __FADER_H__ */
