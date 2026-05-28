#include "mmhelper.h"

// Returns appropriate list for a given block size 
lists_t get_list(size_t block_size) {
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
