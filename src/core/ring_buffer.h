#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>

typedef struct {
    float *data;
    int    capacity;
    int    head;
    int    count;
} ring_buffer_t;

void  ring_buffer_init(ring_buffer_t *rb, float *backing_array, int capacity);
void  ring_buffer_push(ring_buffer_t *rb, float value);
float ring_buffer_get(const ring_buffer_t *rb, int index);
void  ring_buffer_clear(ring_buffer_t *rb);
bool  ring_buffer_full(const ring_buffer_t *rb);
bool  ring_buffer_empty(const ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
