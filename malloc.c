//#include <stdlib.h>
#include <pthread.h> 
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "malloc.h"

typedef struct list_t list_t;

list_t* allocate_block(list_t* last, size_t size);
list_t* find_block(list_t** last, size_t size);
void split_block(list_t* block, size_t size);

#define align4(x) (((((x)-1)>>2)<<2) + 4)

//#define set_debug
#ifdef set_debug
#define debug(...) printf(__VA_ARGS__)
#define debug_block(fun, block) \
	printf("%s: debug block\n", fun); \
	printf("	block:		%p\n", block); \
	printf("	block->next:	%p\n", block->next); \
	printf("	block->prev:	%p\n", block->prev); \
	printf("	block->size:	%p\n", block->size); \
	printf("	block->free:	%p\n", block->free); \
	printf("	block data:	%p\n\n", ((list_t*)block + 1));
#else
#define debug(...)
#define debug_block(fun, block)
#endif

struct list_t {
	list_t* next;
	list_t* prev; // Need this for merging blocks
	size_t size; 
	int free;
};

list_t* base = NULL;

pthread_mutex_t free_list_lock;

void free(void* ptr) {
	pthread_mutex_lock(&free_list_lock);
	debug("FREE: %p\n", ptr);

	if(!ptr) {
		return;
	}

	/*
	if(ptr > ) {
		void* callstack[128];
		int i, frames = backtrace(callstack, 128);
		char** strs = backtrace_symbols(callstack, frames);
		for (i = 0; i < frames; ++i) {
			printf("%s\n", strs[i]);
		}
		free(strs);
	}*/

	list_t* block = ((list_t*)ptr - 1);
	debug_block("FREE, prior to merge, block", block);

	// Merge backward and/or forward if possible
	if(block->prev && block->prev->free) {
		debug_block("FREE, prior to merge back, block->prev", block->prev);
		block->prev->size += block->size + sizeof(struct list_t);
		block->prev->next = block->next;
		debug_block("FREE, after merge back, block->prev", block->prev);
		if(block->next) {
			debug_block("FREE, before merge backward, block->next", block->next);
			block->next->prev = block->prev;
			debug_block("FREE, after merge back, block->next", block->next);
		}
		block = block->prev;
	}
	if(block->next && block->next->free) {
		debug_block("FREE, prior to merge forward, block", block);
		block->size += block->next->size + sizeof(struct list_t);
		block->next = block->next->next;
		debug_block("FREE, after merge forward, block", block);
		if(block->next) {
			debug_block("FREE, before merge forward, block->next", block->next);
			block->next->prev = block;
			debug_block("FREE, after merge forward, block->next", block->next);
		}
	}
	block->free = 1; 

	pthread_mutex_unlock(&free_list_lock);
	return;
}

void* realloc(void* ptr, size_t size) {
	debug("REALLOC: %p %d \n", ptr, (int)size);

	if(!ptr) {
		return malloc(size);
	}

	list_t* block = ((list_t*)ptr - 1);
	
	if(size > block->size) {

		list_t* new_ptr = malloc(size);
		if(!new_ptr) {
			return NULL;
		}

		memcpy(new_ptr, ptr, block->size);
		free(ptr);
		debug("REALLOC: return ptr %p\n", ptr);
		return new_ptr;
	}
	debug("REALLOC: return ptr %p\n", ptr);
	return ptr;
}

void* calloc(size_t num_elements, size_t element_size) {
	size_t size = align4(num_elements * element_size);
	debug("CALLOC: %d %d \n", (int)num_elements, (int)element_size);
	void* ptr = malloc(size);
	memset(ptr, 0, size);
	debug("CALLOC: return ptr %p\n", ptr);
	return ptr;
}

void* malloc(size_t size) {

	pthread_mutex_lock(&free_list_lock);
	size = align4(size);
	debug("MALLOC: requested size: %d \n", (int)size);

	list_t* block;
	if(base) {

		list_t* last = base;
		block = find_block(&last, size);
		if(!block) {
			block = allocate_block(last, size);
			if(!block) {

				debug("MALLOC: returning null w base\n");
				list_t* current = base;
				while(current) {
					if(current->free) {
						debug("size: %zu\n", current->size);
					}
					current = current->next;
				}
				debug("base: %p\n", base);
				debug("sbrk(0): %p\n", sbrk(0));

				pthread_mutex_unlock(&free_list_lock);
				return NULL;
			}
		}
	
	} else {

		block = allocate_block(NULL, size);
		if(!block) {
			debug("MALLOC: returning null no base\n");
			pthread_mutex_unlock(&free_list_lock);
			return NULL;
		}
		base = block;
	}

	block->free = 0;
	debug_block("MALLOC, post, block", block);

	pthread_mutex_unlock(&free_list_lock);
	return (block + 1);
}

// Request new memeory and if successfull, create a
// new block (entry in free list). Also append new block
// to free list via list_t* last
list_t* allocate_block(list_t* last, size_t size) {

	list_t* block = sbrk(0);
	
	list_t* req = sbrk(sizeof(struct list_t) + size);
	if(req == (void*) - 1) {
		return NULL;
	}
	if(req != block) {
		debug("ERROR: sbrk returned different values (thread?): req: %p, block: %p\n", req, block);
		exit(1);
	}

	block->size = size;
	block->next = NULL;

	if(last) {
		last->next = block;
		block->prev = last;
		debug_block("ALLOCATE_BLOCK, post, last", last);
	} else {
		block->prev = NULL;
	}
	debug_block("ALLOCATE_BLOCK, post, block", block);

	debug("ALLOCATE_BLOCK: current brk: %p\n", sbrk(0));

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

	if(current && current->size >= (size + sizeof(struct list_t) + 4)) {
		// +4 as just an arbitrary min split size
		split_block(current, size);
	}

	return current;
}

// Splits a block into one block of size size and another block
// with size of whatever remains.
void split_block(list_t* block, size_t size) {

	debug_block("SPLIT, pre, block", block);
	list_t* new_block = (list_t*)((char*)block + sizeof(struct list_t) + size);
	new_block->next = block->next;
	new_block->prev = block;
	new_block->size = block->size - (size + sizeof(struct list_t));
	new_block->free = 1;

	debug_block("SPLIT, new_block",new_block);
	block->next = new_block;
	block->size = size;
	debug_block("SPLIT, post, block", block);

	if(new_block->next) {
		new_block->next->prev = new_block;
		debug_block("SPLIT, post, new_block->next", new_block->next);
	}
}
