#ifndef __HARDWARE_H__
#define __HARDWARE_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

#define TOUCHOUT_PORT       GPIOA
#define TOUCHOUT_PIN        GPIO_Pin_4
#define TOUCHOUT_IRQn       EXTI4_IRQn
#define TOUCHOUT_EXTI_LINE  EXTI_Line4

void Hardware_Init(void);
void Hardware_ProcessCoreFrame(void);
void Hardware_ProcessFpResponse(void);
void Hardware_SendIdentifyResult(uint8_t success, uint16_t page_id);
void Hardware_SendEnrollResult(uint8_t success, uint16_t page_id);
void Hardware_SendTemplateCount(uint16_t count);
void Hardware_SendIndexList(void);

extern volatile uint8_t g_touchout_flag;
extern uint8_t g_my_module_id;

#ifdef __cplusplus
}
#endif

#endif
