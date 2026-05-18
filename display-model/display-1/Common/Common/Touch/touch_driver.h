#ifndef __TOUCH_DRIVER_H
#define __TOUCH_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    bool pressed;
} Touch_Point_t;

void Touch_Init(void);
void Touch_Scan(void);
bool Touch_GetPoint(Touch_Point_t *point);

#endif
