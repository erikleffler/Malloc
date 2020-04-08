#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "malloc.h"

#define HEADER_SIZE sizeof(struct list_t)
#define align4(x) (((((x)-1)>>2)<<2) + 4)

typedef struct list_t list_t;

list_t* allocate_block(list_t* last, size_t size);
list_t* find_block(list_t** last, size_t size);

struct list_t {
	list_t* next;
	size_t size; 
	int free;
};

list_t* base = NULL;

void free(void* ptr) {
	printf("FREE: %p\n", ptr);

	if(!ptr) {
		return;
	}

	list_t* block = ((list_t*)ptr - 1);
	printf("FREE: block %p\n", block);

	block->free = 1;
	fflush(stdout);
	return;
}

void* realloc(void* ptr, size_t size) {
	printf("REALLOC: %p %d \n", ptr, (int)size);

	if(!ptr) {
		fflush(stdout);
		return malloc(size);
	}

	list_t* block = ((list_t*)ptr - 1);
	
	if(size > block->size) {

		list_t* new_ptr = malloc(size);
		if(!new_ptr) {
			fflush(stdout);
			return NULL;
		}

		memcpy(new_ptr, ptr, block->size);
		free(ptr);
		fflush(stdout);
		return new_ptr;
	}
	fflush(stdout);
	return ptr;

}

void* calloc(size_t num_elements, size_t element_size) {
	size_t size = align4(num_elements * element_size);
	printf("CALLOC: %d %d \n", (int)num_elements, (int)element_size);
	void* ptr = malloc(size);
	memset(ptr, 0, size);
	fflush(stdout);
	return ptr;
}

void* malloc(size_t size) {

	size = align4(size);
	printf("MALLOC: requested size: %d \n", (int)size);

	list_t* block;
	if(base) {

		list_t* last = base;
		block = find_block(&base, size);
		printf("MALLOC: found block: %p\n", block);
		if(!block) {
			block = allocate_block(last, size);
			printf("MALLOC: allocated block: %p\n", block);
			if(!block) {


				printf("MALLOC: returning null w base\n");
				fflush(stdout);
				return NULL;
			}
		}
	
	} else {

		block = allocate_block(NULL, size);
		printf("MALLOC: no base, allocated block: %p\n", block);
		if(!block) {
			printf("MALLOC: returning null no base\n");
			fflush(stdout);
			return NULL;
		}
		base = block;
	}

	block->free = 0;
	printf("MALLOC: returning ptr: %p\n", (block + 1));
	fflush(stdout);

	return (block + 1);
}

// Request new memeory and if successfull, create a
// new block (entry in free list). Also append new block
// to free list via list_t* last
list_t* allocate_block(list_t* last, size_t size) {

	list_t* block = sbrk(0);
	
	list_t* req = sbrk(HEADER_SIZE + size);
	if(req == (void*) - 1) {
		fflush(stdout);
		return NULL;
	}
	if(req != block) {
		printf("NOOOO: %p %p \n", req, block);
		fflush(stdout);
		exit(1);
	}

	printf("ALLOCATE_BLOCK: allocated block: %p\n", block);
	block->size = size;
	printf("ALLOCATE_BLOCK: allocated block size: %d\n", (int)block->size);
	block->next = NULL;

	if(last) {
		last->next = block;
		printf("ALLOCATE_BLOCK: last\n");
	}

	printf("ALLOCATE_BLOCK: current brk: %p\n", sbrk(0));
	fflush(stdout);

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
