#include <unistd.h>
#include "list_malloc.h"


// The glibc malloc aligns memory to 16 bytes on 64 bit and 8 bytes on 32 bit.
// This is exactly sizeof(size_t) * 2.
#define ALIGN_REQ (sizeof(size_t) << 1)

#define align_to_req(num) \
    (((num) + ((ALIGN_REQ) - 1)) & ~((ALIGN_REQ) - 1))

#define min(x, y) (x < y ? x : y);


//#define set_debug
#ifdef set_debug

#include <stdio.h>

// Dont want to recursively call printf since printf will use this malloc (sometimes?)
// Hence, the printing macros above will not run for mallocs in printf or fflush.
int print_in_malloc = 0;

#define debug(...) \
        if(!print_in_malloc) { \
                print_in_malloc = 1; \
                printf(__VA_ARGS__); \
                fflush(stdout); \
                print_in_malloc = 0; \
        }

#define debug_block(fun, block) \
        if(!print_in_malloc) { \
                print_in_malloc = 1; \
                printf("%s: debug block\n", fun); \
                printf("        block:          %p\n", block); \
                printf("        block->next:    %p\n", block->next); \
                printf("        block->prev:    %p\n", block->prev); \
                printf("        block->size:    %zu\n", block->size); \
                printf("        block->free:    %d\n", block->free); \
                printf("        block data:     %p\n\n", ((list_t*)block + 1)); \
                fflush(stdout); \
                print_in_malloc = 0; \
        }
#else
#define debug(...)
#define debug_block(fun, block)
#endif


typedef struct list_t list_t;

struct list_t {
	list_t* next;
	list_t* prev;
	size_t size; 
	int free;
} __attribute__ ((aligned (ALIGN_REQ)));


list_t* allocate_block(list_t* last, size_t size);
list_t* find_block(list_t** last, size_t size);
void split_block(list_t* block, size_t size);



list_t* base = NULL;

void free(void* ptr) {
	debug("FREE: %p\n", ptr);

	if(!ptr) {
		return;
	}

	// Get block pointer so we can merge with surrounding free blocks
	list_t* block = ((list_t*)ptr - 1);

	debug_block("FREE, prior to merge, block", block);

	// Merge backward and/or forward if possible
	if(block->prev && block->prev->free) {
		debug_block("FREE, prior to merge back, block->prev", block->prev);

		// Merge current block back into prev block.
		// Aim to dismiss current block reference and update block to be block-prev 
		// Hence, we are updating block->prev instead of block
		block->prev->size += block->size + sizeof(struct list_t);
		block->prev->next = block->next;

		debug_block("FREE, after merge back, block->prev", block->prev);
		if(block->next) { 
			debug_block("FREE, before merge backward, block->next", block->next);

			block->next->prev = block->prev;

			debug_block("FREE, after merge back, block->next", block->next);
		}
		// Aformentioned update
		block = block->prev;
	}
	if(block->next && block->next->free) {
		debug_block("FREE, prior to merge forward, block", block);

		// Here we instead merge next block into current block
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

	return;
}

void* realloc(void* ptr, size_t size) {
	debug("REALLOC: %p %d \n", ptr, (int)size);

	size = align_to_req(size);

	if(!ptr) {
		return malloc(size);
	}
	
	void * new_ptr;
	if(size) {

		list_t* block = ((list_t*)ptr - 1);

		new_ptr = malloc(size);

		if(!new_ptr) {
			debug("REALLOC: failed with size: %zu\n", size)
			return NULL;
		}

		// Getting some wierd errors, not going to use library functions in case they call malloc. Hence, the make-shift memcpy.
		int min_size = min(size, block->size);
		for(int i = 0; i < min_size; i++) {
			((char*)new_ptr)[i] = ((char*)ptr)[i];
		}

		debug("REALLOC: return ptr %p\n", new_ptr);

	}
	free(ptr);
	return new_ptr;
}

void* calloc(size_t num_elements, size_t element_size) {

	size_t size = align_to_req(num_elements * element_size);
	debug("CALLOC: %d %d \n", (int)num_elements, (int)element_size);
	void* ptr = malloc(size);

	if(!ptr) {
		return NULL;
	}
	
	// Getting some wierd errors, think that memset might have been using this calloc, resulting in an infinite loop. Hence, make-shift memset
	for(int i = 0; i < size; i++) {
			((char*)ptr)[i] = 0;
	}

	debug("CALLOC: return ptr %p\n", ptr);
	return ptr;
}

void* malloc(size_t size) {

	size = align_to_req(size);
	debug("MALLOC: requested size: %zu \n", size);

	list_t* block;
	if(base) {

		// Need to keep track of end of free list in case
		// we allocate new block (need to set prev ref).
		list_t* last = base;

		block = find_block(&last, size);

		if(!block) {
			block = allocate_block(last, size);
			if(!block) {

				debug("MALLOC: returning null w base\n");
				debug("base: %p\n", base);
				debug("sbrk(0): %p\n", sbrk(0));

				return NULL;
			}
		}
	
	} else {

		block = allocate_block(NULL, size);
		if(!block) {
			debug("MALLOC: returning null no base\n");
			return NULL;
		}

		// First malloc of process, set base.
		base = block;
	}

	block->free = 0;
	debug_block("MALLOC, post, block", block);

	return (block + 1);
}

// Request new memeory and if successfull, create a
// new block (entry in free list).
// As the program break is initiated on a page boundary (i.e algined),
// the list_t struct is aligned and we align any requested size, the
// pointers from here will always be aligned.
list_t* allocate_block(list_t* last, size_t size) {

	list_t* block = sbrk(sizeof(struct list_t) + size);
	if(block == (void*) - 1) { // Error retval of sbrk
		return NULL;
	}

	block->size = size;
	block->next = NULL;

	if(last) {

		// append to free list
		last->next = block;
		block->prev = last;
		debug_block("ALLOCATE_BLOCK, post, last", last);
	} else {

		// Base block
		block->prev = NULL;
	}

	debug_block("ALLOCATE_BLOCK, post, block", block);
	debug("ALLOCATE_BLOCK: current brk: %p\n", sbrk(0));

	return block;
}

// Find a free block that meets our size requirement.
// If no such block exist, keep track of the last block in
// our free list so that we may update its next pointer
// after we have allocated a new block. Will split blocks that 
// are larger (if they're free) than the requested size.
list_t* find_block(list_t** last, size_t size) {
	list_t* current = base;
	while(current && !(current->free && current->size >= size)) {
		*last = current;
		current = current->next;
	}

	if(current && current->size >= (size + sizeof(struct list_t) + 8)) {
		// 8 as min split size seems good for alignment
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
