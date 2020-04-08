#include "malloc.h"

#define HEADER_SIZE sizeof(struct list_t)

typedef struct list_t list_t;

list_t* allocate_block(list_t* last, size_t size);
list_t* find_block(list_t** last, size_t size);

struct list_t {
	list_t* next;
	//list_t* prev;
	size_t size; 
	int free;
};

list_t* base = NULL;

void free1(void* ptr) {

	if(!ptr) {
		return;
	}

	list_t* block = (list_t*)ptr - 1;

	block->free = 1;
}

void* realloc1(void* ptr, size_t size) {

	if(!ptr) {
		return malloc1(size);
	}

	list_t* block = (list_t*)ptr - 1;
	
	if(size > block->size) {

		list_t* new_block = malloc1(size);
		if(!new_block) {
			return NULL;
		}

		memcpy((new_block + 1), ptr, block->size);
		free1(ptr);
		return (new_block + 1);
	}

}

void* calloc1(size_t num_elements, size_t element_size) {
	void* ptr = malloc1(num_elements * element_size);
	memset(ptr, 0, num_elements * element_size);
	return ptr;
}

void* malloc1(size_t size) {

	list_t* block;
	if(base) {

		list_t* last = base;
		block = find_block(base, size);
		if(!block) {
			block = allocate_block(last, size);
			if(!block) {
				return NULL;
			}
		}
	
	} else {

		list_t* block = allocate_block(NULL, size);
		if(!block) {
			return NULL;
		}
		base = block;
	}

	block->free = 0;

	return (block + 1);
}

// Request new memeory and if successfull, create a
// new block (entry in free list). Also append new block
// to free list via list_t* last
list_t* allocate_block(list_t* last, size_t size) {

	list_t* block = sbrk(0);
	
	if(sbrk(HEADER_SIZE + size) == (void*) -1) {
		return NULL;
	}

	block->size = size;
	block->next = NULL;

	if(last) {
		last->next = block;
	}

	return block;
}

// Find a free block that meets our size requirement.
// If no such block exist, keep track of the last block in
// our free list so that we may update its next pointer
// after we have allocated a new block.
list_t* find_block(list_t** last, size_t size) {
	list_t* current = base;
	while(current && !(current->free && current->size >= size)) {
		*last = current;
		current = current->next;
	}
	return current;
}
