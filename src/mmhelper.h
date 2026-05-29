#pragma once
#include <stddef.h>
#include <stdbool.h>

// ----------
// ----------
// CONSTANTS:
// ----------
// ----------

#define SIZE_MASK (~(size_t)0xF)
#define ALLOC_MASK ((size_t)0x1)

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

// -------
// GLOBALS
// -------
extern size_t *lists[LIST_NUM];

// -----------------
// -----------------
// FORWARDED HELPERS
// -----------------
// -----------------

// Returns appropriate list for a given block size 
lists_t get_list(size_t block_size);

// Input must be size of block including header and footer. Does not need to be aligned
// Will return a pointer to the header if an appropriate block is found
// Returned block will be at least as big as block_size
// Else, will return NULL
size_t *find_free(size_t block_size);

// Insert block that starts with "header" to beginning of "list"
void insert(size_t *header, lists_t list);

// Removes block that starts with "header" from its list
// UB if not part of list
void remove_header(size_t *header, lists_t list);

// This function will look at adjacent blocks and coalesce if possible
// It takes in the a pointer to the header of the pointer being freed
// Will output header of coalesced block
size_t *coalesce(size_t *header);

// Returns lowest memory on heap
// Only works after tsmm_init()
void *mem_heap_lo();

// Returns highest memory on heap
void *mem_heap_hi();

// -------------------
// -------------------
// IMPLEMENTED HELPERS
// -------------------
// -------------------

// Returns size field of a header/footer
static inline size_t get_size(size_t header) {
    return header & (~0b1111);  // zeroes the 4 LSBs
}

// Return allocated field of a header/footer
static inline size_t get_alloc(size_t header) {
    return header & 1;        // zeroes all but first bit
}

// Returns the footer of a given block
static inline size_t* get_footer(size_t *header) {
    return (size_t *) ((char *)header + get_size(*header) - WORD);
}

// Will set header fields appropriately
static inline void set_header(size_t *header, size_t size, bool allocated) {
    *header = get_size(size) | get_alloc((size_t)allocated);
}

// Will set footer fields appropriately
static inline void set_footer(size_t *header, size_t size, bool allocated) {
    size_t *footer = (size_t *) ((char *)header + size - WORD);
    set_header(footer, size, allocated);
}

// Sets both the header & footer fields to appropriate values
static inline void set_block(size_t *header, size_t size, bool allocated){
    set_header(header, size, allocated);
    set_footer(header, size, allocated);
}

// Will set allocated bit to 1
static inline void set_alloc(size_t *header) {
    // X | 1 == 1
    // X | 0 == X
    *header |= 1;
}

// Will set allocated bit to 0
static inline void reset_alloc(size_t *header) {
    // X & 0 == 0
    // X & 1 == X
    *header &= (~1);
}

// Inputs pointer to header of a free block
// Outputs pointer to this block's PREV_PTR (type size_t **)
static inline size_t **get_this_prev(size_t *header) {
    return (size_t **)header + 1;
}

// Inputs pointer to header of a free block
// Outputs pointer to this block's NEXT_PTR (type size_t **)
static inline size_t **get_this_next(size_t *header) {
    return (size_t **)header + 2;
}

// Inputs pointer to header of a free block and a desired pointer
// Sets the PREV_PTR of the block to {value}
// No Output
static inline void set_this_prev(size_t *header, size_t *value){
    *get_this_prev(header) = value;
}

// Inputs pointer to header of a free block and a desired pointer
// Sets the NEXT_PTR of the block to {value}
// No Output
static inline void set_this_next(size_t *header, size_t *value){
    *get_this_next(header) = value;
}

// Input pointer to header of a free block 
// Output pointer to header of previous block in list
static inline size_t *get_prev_free(size_t *header) {
    // get_this_prev will you give you pointer to this head's PREV_PTR
    // dereference it and you will get the previous pointer's head
    return *get_this_prev(header);
}

// Input pointer to header of a free block
// Output pointer to header of next free block in list
static inline size_t *get_next_free(size_t *header) {
    // get_this_next will you give you pointer to this head's NEXT_PTR
    // dereference it and you will get the next pointer's head
    return *get_this_next(header);
}

// Input ptr to header (size_t *)
// Outputs ptr to previous adjacent head
// If there is no prev, will return NULL
static inline size_t *get_prev_adjacent(size_t *header){
    size_t *prev_footer = header - 1;
    if (get_size(*prev_footer) == 0)         
        return NULL;    // In this case, inputted header to first block

    // prev_footer - size of block + WORD will point to header
    return (size_t *)((char *)prev_footer - get_size(*prev_footer) + WORD);
}

// Input ptr to header (size_t *)
// Output to ptr of next adjacent block's header
// If there is no next, will return NULL
static inline size_t *get_next_adjacent(size_t *header){
	size_t size = get_size(*header);
	if (size == 0 && get_alloc(*header)) 
        size = WORD; //prologue and epilogue both are only 8 bytes...
    size_t *next_header = (size_t *) ((char *)header + size);

    if ((void *)next_header > mem_heap_hi())
        return NULL; //there won't be a "next" if it's the last block...
    return next_header;
}

// rounds up to the nearest multiple of ALIGNMENT
static inline size_t align(size_t x)
{
	return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

