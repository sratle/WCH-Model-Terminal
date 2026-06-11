#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

void Hardware_Init(void);
void Hardware_ProcessCoreFrame(void);
void Hardware_ProcessNfcCard(void);

extern uint8_t g_my_module_id;

#ifdef __cplusplus
}
#endif

#endif
