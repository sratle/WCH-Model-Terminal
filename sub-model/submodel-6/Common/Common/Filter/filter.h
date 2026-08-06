#ifndef __FILTER_H__
#define __FILTER_H__

#ifdef __cplusplus
 extern "C" {
#endif

#include "ch32v10x.h"

/* Moving average filter window size */
#define FILTER_WINDOW_SIZE     8

typedef struct {
    uint16_t buf[FILTER_WINDOW_SIZE];
    uint8_t  head;      /* next write position */
    uint8_t  count;     /* valid samples (<= FILTER_WINDOW_SIZE) */
    uint32_t sum;       /* running sum of window */
} filter_avg_t;

void Filter_Init(filter_avg_t *f);
void Filter_Push(filter_avg_t *f, uint16_t value);
uint8_t Filter_Count(const filter_avg_t *f);
uint16_t Filter_Average(const filter_avg_t *f);

#ifdef __cplusplus
}
#endif

#endif
