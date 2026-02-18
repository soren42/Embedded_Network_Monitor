#include "ring_buffer.h"
#include <stddef.h>

void ring_buffer_init(ring_buffer_t *rb, float *backing_array, int capacity)
{
    if (!rb) return;
    rb->data     = backing_array;
    rb->capacity = capacity;
    rb->head     = 0;
    rb->count    = 0;
}

void ring_buffer_push(ring_buffer_t *rb, float value)
{
    if (!rb || !rb->data || rb->capacity <= 0) return;

    rb->data[rb->head] = value;
    rb->head = (rb->head + 1) % rb->capacity;

    if (rb->count < rb->capacity) {
        rb->count++;
    }
}

float ring_buffer_get(const ring_buffer_t *rb, int index)
{
    if (!rb || !rb->data || index < 0 || index >= rb->count) {
        return 0.0f;
    }

    /* index 0 = oldest entry */
    int start = (rb->head - rb->count + rb->capacity) % rb->capacity;
    int pos   = (start + index) % rb->capacity;
    return rb->data[pos];
}

void ring_buffer_clear(ring_buffer_t *rb)
{
    if (!rb) return;
    rb->head  = 0;
    rb->count = 0;
}

bool ring_buffer_full(const ring_buffer_t *rb)
{
    if (!rb) return false;
    return rb->count >= rb->capacity;
}

bool ring_buffer_empty(const ring_buffer_t *rb)
{
    if (!rb) return true;
    return rb->count == 0;
}
