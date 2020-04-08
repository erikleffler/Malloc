#include <stddef.h>

#ifndef _MY_MALLOC_H
#define _MY_MALLOC_H

void free1(void* ptr);
void* realloc1(void* ptr, size_t size);
void* calloc1(size_t num_elements, size_t element_size);
void* malloc1(size_t size);

#endif
