#ifndef __CLI_H__
#define __CLI_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32h417.h"

void CLI_Init(void);
void CLI_Process(uint8_t *cmd, uint8_t len);

/* 按文件名形式自动选择 8.3 或 LFN 方式打开文件（音频引擎重开通道文件专用） */
uint8_t CLI_OpenFileByName(const char *full_path);

#ifdef __cplusplus
}
#endif

#endif
