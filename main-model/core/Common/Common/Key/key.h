#ifndef __KEY_H__
#define __KEY_H__

#include "ch32h417.h"
#include "ch32h417_gpio.h"
#include "debug.h"

#define KEY_PLUS    1
#define KEY_SUB     2
#define KEY_ENTER   3

#define KEY_GPIO_PORT       GPIOF
#define KEY_SUB_PIN         GPIO_Pin_10
#define KEY_PLUS_PIN        GPIO_Pin_9
#define KEY_ENTER_PIN       GPIO_Pin_8
#define KEY_ALL_PINS        (KEY_PLUS_PIN | KEY_SUB_PIN | KEY_ENTER_PIN)

#define KEY_DEBOUNCE_MS     20
#define KEY_LONG_PRESS_MS   1000

typedef enum {
    KEY_EVT_NONE = 0,
    KEY_EVT_PRESS,
    KEY_EVT_RELEASE,
    KEY_EVT_LONG_PRESS
} key_event_t;

typedef struct {
    uint8_t  key_id;
    uint8_t  pressed;
    uint8_t  long_pressed;
    uint32_t press_tick;
} key_state_t;

void Key_Init(void);
key_event_t Key_Scan(uint8_t *key_id);

void Key_PollAndPush(void);
void Key_ProcessEvents(void);

#endif
