#include "core.h"
#include <pthread.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

bool tsmm_init() {
    return tsmm_init_impl();
}

void* tsmm_malloc(size_t size) {
    pthread_mutex_lock(&lock); 
    void *ret = tsmm_malloc_impl(size);
    pthread_mutex_unlock(&lock);
    return ret;
}

void tsmm_free(void *ptr) {
    pthread_mutex_lock(&lock);
    tsmm_free_impl(ptr);
    pthread_mutex_unlock(&lock);
}

void* tsmm_realloc(void* oldptr, size_t size) {
    pthread_mutex_lock(&lock);
    void *ret = tsmm_realloc_impl(oldptr, size);
    pthread_mutex_unlock(&lock);
    return ret;
}

void* tsmm_calloc(size_t elements, size_t size) {
    pthread_mutex_lock(&lock);
    void *ret = tsmm_calloc_impl(elements, size);
    pthread_mutex_unlock(&lock);
    return ret;
}

