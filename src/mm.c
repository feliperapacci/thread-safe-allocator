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


#include "mm.h"
#include "mmhelper.h"

// -------
// GLOBALS
// -------
size_t* lists[LIST_NUM]; // List headers stored here, starts empty (CHANGE TO STATIC GLOBAL LATER)
static void *heap_start;


// -------------
// ALLOCATOR API
// -------------

// init
// Initializes the allocator.
// Returns true on success, false on failure
bool tsmm_init(void)
{
    // init lists to NULL
    for (int i = 0; i < LIST_NUM; ++i)
        lists[i] = NULL;
    
    // get heap_start
    heap_start = sbrk(0);
    // allocate 8 byte header (prologue) and 8 byte epilogue
    if (sbrk(2*WORD) == (void *) -1)
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
    // If size is zero, fails
    if (size == 0) return NULL;
    
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

        return (void *)(header + 1);
    }

    
    // NO FREE MEMORY AVAILABLE, OBTAIN MORE:
    // Reserve more memory with sbrk
    void *new_ptr = sbrk(block_size);


    if (new_ptr == (void *)-1)
        return NULL;


    // new_ptr currently points to word AFTER epilogue
    // This means it points to the payload.

    //header = (size_t *)((char *)new_ptr - block_size);  	     // Header should be where current epilogue is
	header = (size_t *)new_ptr - 1;
    size_t *epilogue = (size_t *)((char *)header + block_size); // Epilogue after new block

    set_block(header, block_size, true);   // Set new header + footer
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
		return tsmm_malloc(size);
	}

	// if size == 0, treat this as a free(oldptr) call and return oldptr
	if (size == 0){
		tsmm_free(oldptr);
		return NULL;
	}

	// otherwise, adjust the size of memory block pointed to by oldptr to size
    // MAYBE TODO: OPTIMIZE

    // allocate new space
    void *newptr = tsmm_malloc(size);

    // find how large old block was and how many bytes to move
    size_t *header = (size_t *)oldptr - 1;
    size_t old_size = get_size(*header) - WORD;    // size of block - header
    size_t min = old_size > size ? size : old_size;     


    // copy data and free old memory
    memcpy(newptr, oldptr, min);
    tsmm_free(oldptr);

    return newptr;
}

// calloc
// Same behavior as malloc, but zero-initializes payload to zero
void* tsmm_calloc(size_t elements, size_t size)
{
    void* ptr;
    size *= elements;
    ptr = tsmm_malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}


// -------
// HELPERS
// -------

void *mem_heap_lo() {
    return heap_start;
}

void *mem_heap_hi() {
    return (char *)sbrk(0) - 1;
}

