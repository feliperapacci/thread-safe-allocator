#include "core.h"
#include "mmhelper.h"
#include <pthread.h>
#include <threads.h>
#include <limits.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local void *tlcache_heads[LIST_NUM-1]; 
static _Thread_local int tlcache_counts[LIST_NUM-1];
// LIST_NUM - 1 because LIST_BIG is not cached!

#define TLCACHE_BATCH 8

static void* refill(size_t size) {
    void *batch[TLCACHE_BATCH];
    int acquired = 0;

    // allocate batch from global
    pthread_mutex_lock(&lock);
    while (acquired < TLCACHE_BATCH) {
        batch[acquired] = tsmm_malloc_impl(size);
        if (batch[acquired] != NULL) acquired++;
        else break; 
    } 
    pthread_mutex_unlock(&lock);

    if (acquired == 0) return NULL; // Failed

    // batch[0] will be returned to tsmm_malloc and passed to user
    // the rest will be placed into the thread local cache

    for (int i = 1; i < acquired; i++) {
        // get correct bucket
        size_t *header = (size_t*)batch[i] - 1;
        lists_t list = get_list(get_size(*header));

        // add to list
        *(void**)batch[i] = tlcache_heads[list]; // first word will point to the next ptr
        tlcache_heads[list] = batch[i];
        tlcache_counts[list]++;
    }

    return batch[0];
}

bool tsmm_init() {
    return tsmm_init_impl();
}

void* tsmm_malloc(size_t size) {
    if (size == 0) return NULL;
    // get correct list
    size_t block_size = align(size + DWORD);
    lists_t list = get_list(block_size);

    if (list == LIST_BIG) { // Don't cache big allocations
        pthread_mutex_lock(&lock);
        void *ret = tsmm_malloc_impl(size);
        pthread_mutex_unlock(&lock);
        return ret;
    }

    
    // local cache is empty, must refill it with maximum
    if (tlcache_counts[list] == 0) {
        unsigned int maximum_list_size = get_list_int(block_size);
        return refill(maximum_list_size - DWORD); // tsmm_malloc will re-add the DWORD
    }

    // local cache contains memory, use it!
    void *ret = tlcache_heads[list];
    tlcache_heads[list] = *(void**)tlcache_heads[list];
    tlcache_counts[list]--;
    return ret;
}

void tsmm_free(void *ptr) {
    if (!ptr) return; 

    // get proper list
    size_t *header = (size_t*)ptr - 1;
    lists_t list = get_list(get_size(*header));

    if (list == LIST_BIG) { 
        pthread_mutex_lock(&lock); 
        tsmm_free_impl(ptr);
        pthread_mutex_unlock(&lock); 
        return;
    }

    *(void**)ptr = tlcache_heads[list]; // first word will point to the next ptr
    tlcache_heads[list] = ptr;
    tlcache_counts[list]++;
}

void* tsmm_realloc(void* oldptr, size_t size) {
    if (size == 0) {
        tsmm_free(oldptr);
        return NULL;
    }
    if (!oldptr) {
        return tsmm_malloc(size);
    }



    // Get size of list for old and new ptr
    size_t *header = (size_t*)oldptr - 1;
    size_t old_block_size = get_size(*header);
    size_t new_block_size = align(size + DWORD);

    // If either is in the LIST_BIG category, resort to global list
    if (get_list(old_block_size) == LIST_BIG || get_list(new_block_size) == LIST_BIG) {
        pthread_mutex_lock(&lock); 
        void *ret = tsmm_realloc_impl(oldptr, size);
        pthread_mutex_unlock(&lock); 
        return ret;
    }

    unsigned int old_list_num = get_list_int(get_size(*header));    // get current ptrs's list
    unsigned int new_list_num = get_list_int(new_block_size);           // get new ptr's list

    // No need to resize
    if (old_block_size >= new_block_size) return oldptr;


    void *newptr = tsmm_malloc(size);
    if (!newptr) return NULL;   // failure
    
    memcpy(newptr, oldptr, old_block_size - DWORD);
    tsmm_free(oldptr);

    return newptr;
}

void* tsmm_calloc(size_t elements, size_t size) {
    if (elements != 0 && size > (SIZE_MAX / elements)) return NULL; // Checks for overflow

    size *= elements;
    void *ptr = tsmm_malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
