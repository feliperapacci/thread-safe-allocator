/*
 * 1) Allocator Style:             **Explicit**                    (CSAPP 9.9.0)
 * 2) Free block organization:     **Segregated List**             (CSAPP 9.9.6)
 *     - Segregated lists can find small existing free blocks in O(1), and generally faster for bigger blocks
 * 3) Placement:                   **First fit**                    (CSAPP 9.9.7)
 *     - Segregated Lists already alleviate a good portion of the fragmenting issues related to placement policy. Using best fit here would compromise throughput for marginal benefits in space.
 * 4) Splitting:                   **Split**                       (CSAPP 9.9.8)
 *     - Decreases fragmentation with marginal decreases in throughput (placement is still constant)
 * 5) Coalescing:                  **Immediate**                   (CSAPP 9.9.10)
 *     - Notable decrease in fragmentation with marginal decreases in throughput (freeing is still constant)
 *     - Higher memory utilization due to additional, mitigated by less fragmentation 
 * 6) Initial Heap Size:           **16 Bytes** (Prologue + Epilogue)
 * 7) Word Size, Alignment         **8, 16**
 *
 * FREE BLOCK FORMAT: 
 * [ HEADER (8b) ][ PREV_PTR (8b) ][ NEXT_PTR (8b) ][ PAYLOAD (Xb) ][ FOOTER (8b) ]
 *
 * ALLOCATED BLOCK FORMAT:
 * [ HEADER (8b) ][ PAYLOAD (Nb) ][ PADDING (Mb) ][ FOOTER (8b) ]
 *
 *
 * SEGREGATED LISTS:
 * all free blocks will use 32 bytes for header, prev_ptr, next_ptr, and footer 
 * however, once allocated, the 16 bytes used for the pointer can be used for the payload 
 * We are also limited to 128 bytes in the global space, so at most 16 pointers at most
 * thus, the following block sizes (including footer and header) will be used:
 *
 * 32, 64, 96, 128, 160, 192, 224, 256, 512, 1024, 2048, 4096, 8192, 16384, 32764, >32764
 * There are 16 total lists
 * 
 *
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

// --------
// GLOBALS:
// --------

// CONSTANTS:
enum { 
    LIST_NUM = 16,
    ALIGNMENT = 16,
    WORD = 8,
    DWORD = 16,
    SMALLEST_BLOCK = 32
};

typedef enum {
    LIST32, LIST64, LIST96, LIST128,
    LIST160, LIST192, LIST224, LIST256,
    LIST512, LIST1024, LIST2048, LIST4096,
    LIST8192, LIST16384, LIST32764, LIST_BIG 
} lists_t;

// OBJECTS
static size_t* lists[LIST_NUM];// List headers stored here, starts empty


// -----------------
// HELPER FUNCTIONS:
// -----------------

// Returns size field of a header/footer
static size_t get_size(size_t header) {
    return header & (~0b1111);  // zeroes the 4 LSBs
}

// Return allocated field of a header/footer
static size_t get_alloc(size_t header) {
    return header & 1;        // zeroes all but first bit
}

// Returns the footer of a given block
static size_t* get_footer(size_t *header) {
    return (size_t *) ((char *)header + get_size(*header) - WORD);
}

// Will set header fields appropriately
static void set_header(size_t *header, size_t size, bool allocated) {
    *header = get_size(size) | get_alloc((size_t)allocated);
}

// Will set footer fields appropriately
static void set_footer(size_t *header, size_t size, bool allocated) {
    size_t *footer = (size_t *) ((char *)header + size - WORD);
    set_header(footer, size, allocated);
}

// Sets both the header & footer fields to appropriate values
static void set_block(size_t *header, size_t size, bool allocated){
    set_header(header, size, allocated);
    set_footer(header, size, allocated);
}

// Will set allocated bit to 1
static void set_alloc(size_t *header) {
    // X | 1 == 1
    // X | 0 == X
    *header |= 1;
}

// Will set allocated bit to 0
static void reset_alloc(size_t *header) {
    // X & 0 == 0
    // X & 1 == X
    *header &= (~1);
}

// Inputs pointer to header of a free block
// Outputs pointer to this block's PREV_PTR (type size_t **)
static size_t **get_this_prev(size_t *header) {
    return (size_t **)header + 1;
}

// Inputs pointer to header of a free block
// Outputs pointer to this block's NEXT_PTR (type size_t **)
static size_t **get_this_next(size_t *header) {
    return (size_t **)header + 2;
}

// Inputs pointer to header of a free block and a desired pointer
// Sets the PREV_PTR of the block to {value}
// No Output
static void set_this_prev(size_t *header, size_t *value){
    *get_this_prev(header) = value;
}

// Inputs pointer to header of a free block and a desired pointer
// Sets the NEXT_PTR of the block to {value}
// No Output
static void set_this_next(size_t *header, size_t *value){
    *get_this_next(header) = value;
}

// Input pointer to header of a free block 
// Output pointer to header of previous block in list
static size_t *get_prev_free(size_t *header) {
    // get_this_prev will you give you pointer to this head's PREV_PTR
    // dereference it and you will get the previous pointer's head
    return *get_this_prev(header);
}

// Input pointer to header of a free block
// Output pointer to header of next free block in list
static size_t *get_next_free(size_t *header) {
    // get_this_next will you give you pointer to this head's NEXT_PTR
    // dereference it and you will get the next pointer's head
    return *get_this_next(header);
}

// Input ptr to header (size_t *)
// Outputs ptr to previous adjacent head
// If there is no prev, will return NULL
static size_t *get_prev_adjacent(size_t *header){
    size_t *prev_footer = header - 1;
    if (get_size(*prev_footer) == 0)         
        return NULL;    // In this case, inputted header to first block

    // prev_footer - size of block + WORD will point to header
    return (size_t *)((char *)prev_footer - get_size(*prev_footer) + WORD);
}

// rounds up to the nearest multiple of ALIGNMENT
static size_t align(size_t x)
{
	return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

// Input ptr to header (size_t *)
// Output to ptr of next adjacent block's header
// If there is no next, will return NULL
static size_t *get_next_adjacent(size_t *header){
	size_t size = get_size(*header);
	//size_t total_size = align(payload_size + DWORD); //get the size of the block including header & footer
	if (size == 0 && get_alloc(*header)) size = WORD; //prologue and epilogue both are only 8 bytes...
    size_t *next_header = (size_t *) ((char *)header + size);

    if ((void *)next_header > mem_heap_hi()) return NULL; //there won't be a "next" if it's the last block...
	//if (get_size(*next_header) == 0) return NULL;    // In this case, inputted header to last block
    return next_header;
}

static lists_t get_list(size_t block_size) {
    if (block_size <= 32) 
        return LIST32;
    else if (block_size <= 64)
        return LIST64;
    else if (block_size <= 96)
        return LIST96;
    else if (block_size <= 128)
        return LIST128;
    else if (block_size <= 160)
        return LIST160;
    else if (block_size <= 192)
        return LIST192;
    else if (block_size <= 224)
        return LIST224;
    else if (block_size <= 256)
        return LIST256;
    else if (block_size <= 512)
        return LIST512;
    else if (block_size <= 1024)
        return LIST1024;
    else if (block_size <= 2048)
        return LIST2048;
    else if (block_size <= 4096)
        return LIST4096;
    else if (block_size <= 8192)
        return LIST8192;
    else if (block_size <= 16384)
        return LIST16384;
    else if (block_size <= 32764)
        return LIST32764;
    else
        return LIST_BIG;
}

// Input must be size of block including header and footer. Does not need to be aligned
// Will return a pointer to the header if an appropriate block is found
// Returned block will be at least as big as block_size
// Else, will return NULL
size_t *find_free(size_t block_size) {
    
    // Find initial list:
    int list_index = get_list(block_size);

    // look in every sufficiently big list
    for (; list_index < LIST_NUM; ++list_index) {
        size_t *header = lists[list_index]; // header will point to the first header (NULL if empty)
        while (header != NULL) {
            if (get_size(*header) >= block_size) // If block fits, return it
                return header;
            else                                            // If not, look at next
                header = get_next_free(header);
        }
    }

    // If this is reached, there is no fitting free block
    return NULL;
}

// Insert block that starts with "header" to beginning of "list"
void insert(size_t *header, lists_t list) { 
    if (lists[list] != NULL)    // If list not empty, first->prev  = curr
        set_this_prev(lists[list], header);    

    set_this_prev(header, NULL);            // Curr node.prev->NULL
    set_this_next(header, lists[list]);     // Curr node.next->next_block
    lists[list] = header;                   // List starts with new node
}

// Removes block that starts with "header" from its list
// UB if not part of list
void remove_header(size_t *header, lists_t list) {
    // MUST CONNECT prev->next with next, and next->prev with prev
    size_t *prev_header = get_prev_free(header);
    size_t *next_header = get_next_free(header);

    if (lists[list] == header)      // If trying to remove first node:
        lists[list] = next_header;  // first = curr->next
    else                                        
        set_this_next(prev_header, next_header);   // prev->next = curr->next 
        
    if (next_header)    // If next exists, next->prev = prev
        set_this_prev(next_header, prev_header);
    // if next is NULL, nothing should be done (removing last node)
}


// This function will look at adjacent blocks and coalesce if possible
// It takes in the a pointer to the header of the pointer being freed
// Will output header of coalesced block
size_t *coalesce(size_t *header) {
    // Coalescing consists of removing free adjacent blocks from their lists and merging them with the current freed block

    // Save all of the relevant info as variables so we don't have to
    // access it with the getter over and over again
    size_t *prev = get_prev_adjacent(header);   // ptr to prev block head
    size_t *next = get_next_adjacent(header);   // ptr to next block head
    size_t total_size = get_size(*header);

    /* Coalesce left */
    // Only coalesce left if prev is not NULL and is free
    if (prev && !get_alloc(*prev)) {
        // free prev from its list
        size_t prev_size = get_size(*prev);
        remove_header(prev, get_list(prev_size));

        // part of merging
        header = prev;
        total_size += prev_size;
    }

    /* Coalesce right */
    // Only coalesce right if next is not NULL and is free
    if (next && !get_alloc(*next)) {
        // free next from its list
        size_t next_size = get_size(*next);
        remove_header(next, get_list(next_size));

        // part of merging
        total_size += next_size;
    }

    // merge
    set_block(header, total_size, false);

    return header;
}


// -------------
// ALLOCATOR API
// -------------

// init
// Initializes the allocator.
// Returns true on success, false on failure
bool mm_init(void)
{
    // init lists to NULL
    for (int i = 0; i < LIST_NUM; ++i)
        lists[i] = NULL;
    
    // allocate 8 byte header (prologue) and 8 byte epilogue
    if (mem_sbrk(2*WORD) == (void *) -1)
        return false;

    // set prologue and epilogue to correct values
    size_t *init_mem = mem_heap_lo();
    set_header(init_mem, 0, true); 		//prologue header
    set_header(init_mem + 1, 0, true);	//epilogue header

    return true; //return true if we get here 
}

// malloc
// Returns a void pointer to a 16-byte aligned payload of at least [size] bytes on success
// Returns NULL if size = 0 or on failure
void* tsmm_malloc(size_t size)
{
    // block must fit payload (size), and header + footer (double word)
    size_t block_size = align(size + DWORD);

    // find free head
    size_t *header = find_free(block_size);
    if (header != NULL) {

        // What is the size of the obtained block?
        size_t new_block_size = get_size(*header);

        // remove from its free list and set header/footer alloc bits
        remove_header(header, get_list(new_block_size));
        set_alloc(header);
        set_alloc(get_footer(header));

        // difference is the number of extra blocks in the selected block.
        // (selected block refers to the block return by find_free())
        size_t difference = new_block_size - block_size;
        if (difference < SMALLEST_BLOCK)    // In this case, can't split
            return (void *)(header + 1);

        // SPLITTING LOGIC (remainder of if block):
        
        // remainder_block points to what will be the header of the free portion of the selected block
        size_t *remainder_block = (size_t *) ((char *)header + block_size);

        // setting header/footer for newly allocated block
        set_block(header, block_size, 1);

        // setting header/footer for free block and placing in list
        set_block(remainder_block, difference, 0);
        insert(remainder_block, get_list(difference));

		//For debugging/using mm_checkheap:
		#ifdef DEBUG
		static int malloc_count = 0;
		malloc_count++;
		if (malloc_count % 1000 == 0) mm_checkheap(__LINE__);
		#endif

        return (void *)(header + 1);
    }

    
    // NO FREE MEMORY AVAILABLE, OBTAIN MORE:
    // Reserve more memory with mem_sbkr
    void *new_ptr = mem_sbrk(block_size);

	//printf("mem_sbrk returned %p, heap_lo = %p, epilogue at %p\n",
	//	new_ptr, mem_heap_lo(), (char *)mem_heap_lo() + 8);

    if (new_ptr == (void *)-1)
        return NULL;

    // new_ptr currently points to word AFTER epilogue
    // This means it points to the payload.

    //header = (size_t *)((char *)new_ptr - block_size);  	     // Header should be where current epilogue is
	header = (size_t *)new_ptr - 1;
    size_t *epilogue = (size_t *)((char *)header + block_size); // Epilogue after new block

	//debug stmt:
	//printf("new_ptr=%p block_size=%zu, header=%p payload=%p heap_lo=%p, heap_hi=%p\n",
    //    new_ptr, block_size, header, (void *)(header+1), mem_heap_lo(), mem_heap_hi());

    set_block(header, block_size, true);   // Set new header 
    set_header(epilogue, 0, true);         // Restore epilogue

    return (void *)(header + 1);
}

// free
// Frees the block pointed to by incoming pointer
// Only works with pointers to first byte in an allocated payload
void tsmm_free(void* ptr)
{
    // NULL pointers should not be affected by this function, so:
    if (ptr == NULL) return;

    // Get header
    size_t *header = (size_t *)ptr - 1;

    // Store the size of the block to use in set_block() and insert() funcs
    size_t size = get_size(*header);

    // Use the block and its size to reset the header & footer:
    set_block(header, size, 0);

    // coalesce it with any free neighboring blocks
    header = coalesce(header);
    
    // Insert coalesced block into the list of unallocated blocks
    insert(header, get_list(get_size(*header)));

	//For debugging/using mm_checkheap():
	//For debugging/using mm_checkheap:
   	#ifdef DEBUG
   	static int free_count = 0;
    free_count++;
    if (free_count % 1000 == 0) mm_checkheap(__LINE__);
    #endif
}

// realloc
// Returns a pointer to a 16-byte aligned payload of at least [size] bytes on success
// If oldptr is null, works just like malloc
// If size is zero, just frees oldptr
// Else, will return a ptr to a payload at least as big as size with the same a data as *oldptr
void* tsmm_realloc(void* oldptr, size_t size)
{
	// if oldptr == NULL, treat this as a malloc(size) call
	if (oldptr == NULL){
		return malloc(size);
	}

	// if size == 0, treat this as a free(oldptr) call and return oldptr
	if (size == 0){
		free(oldptr);
		return oldptr;
	}

	// otherwise, adjust the size of memory block pointed to by oldptr to size
    // MAYBE TODO: OPTIMIZE

    // allocate new space
    void *newptr = malloc(size);

    // find how large old block was and how many bytes to move
    size_t *header = (size_t *)oldptr - 1;
    size_t old_size = get_size(*header) - WORD;    // size of block - header
    size_t min = old_size > size ? size : old_size;     


    // copy data and free old memory
    memcpy(newptr, oldptr, min);
    free(oldptr);

    return newptr;
}

// calloc
// Same behavior as malloc, but zero-initializes payload to zero
void* tsmm_calloc(size_t elements, size_t size)
{
    void* ptr;
    size *= elements;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
