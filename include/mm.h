#pragma once

// init
// Initializes the allocator.
// Returns true on success, false on failure
bool tsmm_init();

// malloc
// Returns a void pointer to a 16-byte aligned payload of at least [size] bytes on success
// Returns NULL if size = 0 or on failure
void* tsmm_malloc(size_t size);

// free
// Frees the block pointed to by incoming pointer
// Only works with pointers to first byte in an allocated payload
void tsmm_free(void *ptr);

// realloc
// Returns a pointer to a 16-byte aligned payload of at least [size] bytes on success
// If oldptr is null, works just like malloc
// If size is zero, just frees oldptr
// Else, will return a ptr to a payload at least as big as size with the same a data as *oldptr
void* tsmm_realloc(void* oldptr, size_t size);

// calloc
// Same behavior as malloc, but zero-initializes payload to zero
void* tsmm_calloc(size_t elements, size_t size);

