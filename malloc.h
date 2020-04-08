#include <stddef.h>

#ifndef _MY_MALLOC_H
#define _MY_MALLOC_H

void free(void* ptr);
void* realloc(void* ptr, size_t size);
void* calloc(size_t num_elements, size_t element_size);
void* malloc(size_t size);

#endif
