#ifndef _SHARED_ALLOC_H
#define _SHARED_ALLOC_H

#include <stdio.h>

void* shared_malloc(size_t new_size);
void* shared_VirtualAlloc(size_t new_size);

#endif
