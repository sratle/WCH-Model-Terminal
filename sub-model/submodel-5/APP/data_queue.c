/*********************************************************************
 * File Name          : data_queue.c
 * Description        : Simple thread-safe ring buffer for ISR/main
 *                      loop data passing.
 *********************************************************************/

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
    if (next == q->tail) {
        return FALSE;   /* Full */
    }
    q->buf[q->head] = byte;
    q->head = next;
    return TRUE;
}

bool DataQueue_Pop(data_queue_t *q, uint8_t *byte)
{
    if (q->head == q->tail) {
        return FALSE;   /* Empty */
    }
    *byte = q->buf[q->tail];
    q->tail = (q->tail + 1) % q->size;
    return TRUE;
}

uint16_t DataQueue_Count(data_queue_t *q)
{
    if (q->head >= q->tail) {
        return q->head - q->tail;
    } else {
        return q->size - q->tail + q->head;
    }
}

bool DataQueue_IsEmpty(data_queue_t *q)
{
    return (q->head == q->tail);
}

bool DataQueue_IsFull(data_queue_t *q)
{
    return (((q->head + 1) % q->size) == q->tail);
}

uint16_t DataQueue_PushBulk(data_queue_t *q, const uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t pushed = 0;
    for (i = 0; i < len; i++) {
        if (!DataQueue_Push(q, data[i])) {
            break;
        }
        pushed++;
    }
    return pushed;
}

uint16_t DataQueue_PopBulk(data_queue_t *q, uint8_t *data, uint16_t max_len)
{
    uint16_t i;
    uint16_t popped = 0;
    for (i = 0; i < max_len; i++) {
        if (!DataQueue_Pop(q, &data[i])) {
            break;
        }
        popped++;
    }
    return popped;
}
