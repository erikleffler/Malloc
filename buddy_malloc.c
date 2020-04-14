#include <unistd.h>
#include "buddy_malloc.h"


// The glibc malloc aligns memory to 16 bytes on 64 bit and 8 bytes on 32 bit.
// This is exactly sizeof(size_t) * 2.
#define ALIGN_REQ (sizeof(size_t) << 1)

#define align_to_req(num) \
    (((num) + ((ALIGN_REQ) - 1)) & ~((ALIGN_REQ) - 1))

#define min(x, y) (x < y ? x : y);

#define max(x, y) (x > y ? x : y);

#define MIN_KVAL 5



//#define set_debug
#ifdef set_debug
#include <stdio.h>

// Dont want to recursively call printf since printf will use this malloc (sometimes?)
// Hence, the printing macros above will not run for mallocs in printf or fflush.
static int print_in_malloc = 0;

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
                printf("        block->kval:    %zu\n", block->kval); \
                printf("        block->free:    %x\n", block->free); \
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
	char kval : 7; 
	unsigned int free : 1;
} __attribute__ ((aligned (ALIGN_REQ)));

// Init free list to be way larger than we may ever need, this should
// not bottleneck how much memory our proc can allocate. 
//
// We let free_list[0] be the blocks of size 1 << MIN_KVAL (i.e this is the 
// smallest block size). The largest will be 1 << (24 + MIN_KVAL).
static list_t* free_list[24];

static char curr_max_kval = 0; // Keep track of end of free_list

static void* mem_base = NULL; // Start of mempool

int init_mempool(size_t adj_size);
int double_mempool();
list_t* find_and_split_block(size_t adj_size);

void free(void* ptr) {
	debug("FREE: %p\n", ptr);

	if(!ptr) {
		return;
	}

	list_t* block = ((list_t*)ptr - 1);

	debug_block("FREE: block", block);
	debug("FREE: curr_max_kval: %d\n", curr_max_kval);

	if(block->kval < curr_max_kval) { // If block is max size it has no buddies


		debug("FREE: mem_base: %p\n", mem_base);

		intptr_t block_offset = (intptr_t)block - (intptr_t)mem_base;
		debug("FREE: block_offset: %d\n", block_offset);

		intptr_t buddy_offset = block_offset ^ (1 << block->kval);
		debug("FREE: buddy_offset: %d\n", buddy_offset);

		list_t* buddy = (list_t*)((char*)mem_base + buddy_offset);
		debug_block("FREE: buddy", buddy);


		if(buddy->free && buddy->kval == block->kval) {

			// Remove buddy from free_list
			if(buddy->prev) {
				buddy->prev->next = buddy->next;
			} else { // Buddy block is first in list, need to remove the ref
				free_list[buddy->kval - MIN_KVAL] = buddy->next;
			}

			if(buddy->next) {
				buddy->next->prev = buddy->prev;
			}

			

			// Merge buddies. Use list_t for first buddy in memory
			list_t* merged = min(block, buddy);
			merged->kval += 1;

			debug_block("FREE: merged", merged);

			// Call free recursively to merge merged with it's buddy if free
			free((merged + 1));
			return;
		} 
	}

	block->free = 1; 

	// Put block first in free list
	block->next = free_list[block->kval - MIN_KVAL];
	block->prev = NULL;

	if(block->next) {
		block->next->prev = block;
	}

	free_list[block->kval - MIN_KVAL] = block;

	debug_block("FREE: final block:", block);

	return;
}

void* realloc(void* ptr, size_t size) {//
	debug("REALLOC: %p %d \n", ptr, (int)size);

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
		int min_size = min(size, (1 << block->kval));
		for(int i = 0; i < min_size; i++) {
			((char*)new_ptr)[i] = ((char*)ptr)[i];
		}

		debug("REALLOC: return ptr %p\n", new_ptr);

	}
	free(ptr);
	return new_ptr;
}

void* calloc(size_t num_elements, size_t element_size) {

	size_t size = num_elements * element_size;
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

	if(!size) {
		return NULL;
	}

	// Adjust size to account for list_t struct
	size_t adj_size = size + sizeof(list_t);

	debug("MALLOC: requested size adjusted: %zu \n", adj_size);

	if(!mem_base) { // First ever malloc

		// Initialize to max(adj_size, min_size)
		if(init_mempool(adj_size) == -1) { // Error
			return NULL;
		}

	}

	list_t* block = find_and_split_block(adj_size);

	if(!block) {
		return NULL;
	}

	block->free = 0;

	return (block + 1);
}

// This function is the work horse of malloc, it will:
//  * search the free_list for a block of large enough size
//  * if no such block exist, increase memory
//  * if we can't increase memory, return NULL
//  * otherwise once block is found, split it down
//  * until it is the correct size
//  * insert all split remainders in free_list
//  * return the first one of correct size
list_t* find_and_split_block(size_t adj_size) {

	debug("FIND_AND_SPLIT_BLOCK: adj_size: %zu \n", adj_size);

	char kval = 0;
	while(adj_size > (1 << kval)) {
		kval += 1; // kval = Ceil of log2 of adj_size
	}

	kval = max(kval, MIN_KVAL);

	debug("FIND_AND_SPLIT_BLOCK: kval: %zu \n", kval);

	char avail_kval = kval;

	while(!free_list[avail_kval - MIN_KVAL]) {

		avail_kval++;
		if(avail_kval > curr_max_kval) { // No free block big enough

			if(double_mempool() == -1) { // Coudn't allocate new one either
				return NULL;
			}
			avail_kval--;
		}
	}

	debug("FIND_AND_SPLIT_BLOCK: avail_kval: %zu \n", avail_kval);

	// We found our block, now start splitting until we have approp size
	
	// This block will be splitted and returned
	list_t* to_split = free_list[avail_kval - MIN_KVAL];

	debug_block("FIND_AND_SPLIT_BLOCK: found to_split", to_split);

	//remove found block from free list
	if(to_split->next) {
		to_split->next->prev = NULL;
	}
	free_list[avail_kval - MIN_KVAL] = to_split->next;

	// Do the splitting and add blocks to free_list along the way
	list_t* split_remainder;
	while(avail_kval != kval) {
		
		avail_kval--;
		split_remainder = (list_t*)((char*)to_split + (1 << avail_kval));
		split_remainder->next = NULL;
		split_remainder->prev = NULL;
		split_remainder->kval = avail_kval;
		split_remainder->free = 1;
		free_list[avail_kval - MIN_KVAL] = split_remainder;

		debug_block("FIND_AND_SPLIT_BLOCK: split_remainder", split_remainder);
	}

	to_split->kval = kval;
	to_split->next = NULL;
	to_split->prev = NULL;

	debug_block("FIND_AND_SPLIT_BLOCK: returning to_split", to_split);

	return to_split;
}

// Dobules mempool, returns -1 on failure, 0 on success
int double_mempool() {

	debug("DOUBLE_MEMPOOL: curr_max_kval: %d\n",curr_max_kval);
	// Is any memory occupied? if not make sure resulting 
	// mempool is one big free block
	if(free_list[curr_max_kval - MIN_KVAL]) {

		// Make request to double mem pool
		if(sbrk(1 << curr_max_kval) == (void*) - 1) { // Error retval of sbrk
			return -1;
		}

		// Move block up one kval (i.e double size).
		free_list[curr_max_kval - (MIN_KVAL - 1)] = free_list[curr_max_kval - MIN_KVAL];
		free_list[curr_max_kval - (MIN_KVAL - 1)]->kval++;
		free_list[curr_max_kval - MIN_KVAL] = NULL;

	} else { // we have non free memory. Just create a new free block

		list_t* block = sbrk(1 << curr_max_kval);

		if(block == (void*) - 1) { // Error retval of sbrk
			return -1;
		}
			
		block->prev = NULL;
		block->next = NULL;
		block->kval = curr_max_kval;
		block->free = 1;

		free_list[curr_max_kval - MIN_KVAL] = block;
	}

	curr_max_kval++;
	return 0;
}

// Inits mempool to smallest power of 2 larger than size or page size, 
// depending on which is biggest. Returns -1 on failure and 0 on success
// Also sets membase. Should only be called once during first malloc.
// Needs to be called with adjusted size
int init_mempool(size_t adj_size) {

	if(getpagesize() > adj_size) {
		// Gawk uses gcc and page size is power of 2. __builtin_ctz works with gcc
		// and counts trailing zeros. In our case (power of 2) this is exactly log2.
		curr_max_kval = __builtin_ctz(getpagesize()); 
	} else {
		while(adj_size > (1 << curr_max_kval)) {
			curr_max_kval += 1;  // kval = Ceil of log2 of adj_size
		}
	}

	debug("INIT_MEMPOOL: curr_max_kval: %zu \n", curr_max_kval);
	list_t* block = sbrk(1 << curr_max_kval);

	mem_base = (void*)block; // This is the first block allocated

	if(block == (void*) - 1) { // Error retval of sbrk
		curr_max_kval = 0;
		mem_base = NULL;
		return -1;
	}

	block->next = NULL;
	block->prev = NULL;
	block->kval = curr_max_kval;
	block->free = 1;

	debug_block("INIT_MEMPOOL: done init\n", block);

	free_list[curr_max_kval - MIN_KVAL] = block;
	
	return 0;
}
