/********************************** (C) COPYRIGHT *******************************
 * File Name          : data_queue.c
 *******************************************************************************/

#include "data_queue.h"

void DataQueue_Init(data_queue_t *q, uint8_t *buf, uint16_t size)
{
    q->head = 0;
    q->tail = 0;
    q->size = size;
    q->buf = buf;
}

bool DataQueue_Push(data_queue_t *q, uint8_t byte)
{
    uint16_t next = (q->head + 1) % q->size;
    if(next == q->tail)
        return FALSE; /* full */
    q->buf[q->head] = byte;
    q->head = next;
    return TRUE;
}

bool DataQueue_Pop(data_queue_t *q, uint8_t *byte)
{
    if(q->head == q->tail)
        return FALSE; /* empty */
    *byte = q->buf[q->tail];
    q->tail = (q->tail + 1) % q->size;
    return TRUE;
}

uint16_t DataQueue_Count(data_queue_t *q)
{
    return (q->head >= q->tail) ? (q->head - q->tail) : (q->size - q->tail + q->head);
}

bool DataQueue_IsEmpty(data_queue_t *q)
{
    return (q->head == q->tail);
}

bool DataQueue_IsFull(data_queue_t *q)
{
    return (((q->head + 1) % q->size) == q->tail);
}
