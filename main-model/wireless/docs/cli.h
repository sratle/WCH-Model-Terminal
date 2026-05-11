#ifndef __CLI_H__
#define __CLI_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"

void CLI_Init(void);
void CLI_Process(uint8_t *cmd, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
