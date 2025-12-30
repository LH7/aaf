#ifndef _SHARED_ALLOC_H
#define _SHARED_ALLOC_H

#include <stdio.h>

typedef enum {
    SM_BUFFER_COMMON,
    _SHARED_MALLOC_BUFFERS
} sm_buffer_idx_t;

void* shared_malloc(size_t new_size, sm_buffer_idx_t buffer_idx);

#endif
