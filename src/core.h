#pragma once 

#include "mmhelper.h"
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

extern size_t* tsmm_free_lists[LIST_NUM]; 

bool tsmm_init_impl(void);
void* tsmm_malloc_impl(size_t size);
void tsmm_free_impl(void* ptr);
void* tsmm_realloc_impl(void* oldptr, size_t size);
void* tsmm_calloc_impl(size_t elements, size_t size);
