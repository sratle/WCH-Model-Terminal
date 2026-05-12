#ifndef __DATA_QUEUE_H
#define __DATA_QUEUE_H

#include "CH58x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

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
uint16_t DataQueue_PushBulk(data_queue_t *q, const uint8_t *data, uint16_t len);
uint16_t DataQueue_PopBulk(data_queue_t *q, uint8_t *data, uint16_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __DATA_QUEUE_H */
