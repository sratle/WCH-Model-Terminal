#ifndef __KEY_H__
#define __KEY_H__

#include "ch32h417.h"
#include "debug.h"

#define KEY_PLUS    1
#define KEY_SUB     2
#define KEY_ENTER   3

#define KEY_GPIO_PORT       GPIOF
#define KEY_PLUS_PIN        GPIO_Pin_10
#define KEY_SUB_PIN         GPIO_Pin_9
#define KEY_ENTER_PIN       GPIO_Pin_8
#define KEY_ALL_PINS        (KEY_PLUS_PIN | KEY_SUB_PIN | KEY_ENTER_PIN)

void Key_init(void);

uint8_t Key_scan(void);

#endif
