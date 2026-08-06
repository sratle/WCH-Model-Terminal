#include "filter.h"

void Filter_Init(filter_avg_t *f)
{
    uint8_t i;

    f->head = 0;
    f->count = 0;
    f->sum = 0;
    for (i = 0; i < FILTER_WINDOW_SIZE; i++)
        f->buf[i] = 0;
}

void Filter_Push(filter_avg_t *f, uint16_t value)
{
    if (f->count < FILTER_WINDOW_SIZE)
    {
        f->buf[f->head] = value;
        f->sum += value;
        f->count++;
    }
    else
    {
        f->sum -= f->buf[f->head];
        f->buf[f->head] = value;
        f->sum += value;
    }
    f->head = (f->head + 1) % FILTER_WINDOW_SIZE;
}

uint8_t Filter_Count(const filter_avg_t *f)
{
    return f->count;
}

uint16_t Filter_Average(const filter_avg_t *f)
{
    if (f->count == 0)
        return 0;
    return (uint16_t)(f->sum / f->count);
}
