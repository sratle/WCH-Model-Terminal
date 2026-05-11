/********************************** (C) COPYRIGHT *******************************
 * File Name          : data_queue.h
 * Description        : Simple byte ring buffer for ISR / main loop data passing
 *******************************************************************************/

#ifndef __DATA_QUEUE_H__
#define __DATA_QUEUE_H__

#include "CH58x_common.h"

typedef struct {
    volatile uint16_t head;
    volatile uint16_t tail;
    uint16_t size;
    uint8_t *buf;
} data_queue_t;

void DataQueue_Init(data_queue_t *q, uint8_t *buf, uint16_t size);
bool DataQueue_Push(data_queue_t *q, uint8_t byte);
bool DataQueue_Pop(data_queue_t *q, uint8_t *byte);
uint16_t DataQueue_Count(data_queue_t *q);
bool DataQueue_IsEmpty(data_queue_t *q);
bool DataQueue_IsFull(data_queue_t *q);

#endif /* __DATA_QUEUE_H__ */
