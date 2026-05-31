#include "sanity.h"

void test_simple_allocation() {
    fprintf(stderr, "----Testing simple allocation...\n");

    void *p = tsmm_malloc(16);
    CHECK(p != NULL);               // Assert allocation worked    
    CHECK((uintptr_t)p % 16 == 0);  // Assert correct alignment 
    tsmm_free(p);
}

void test_edge_cases() {
    fprintf(stderr, "----Testing edge cases...\n");

    CHECK(tsmm_malloc(0) == NULL);      // Assert size 0 returns NULL
    tsmm_free(NULL);                    // Should be ok

    void *p = tsmm_malloc(16);
    p = tsmm_realloc(p, 0);     
    CHECK(p == NULL);                   // Asserting realloc to size 0 returns NULL
    
    p = tsmm_realloc(NULL, 32);
    CHECK(p != NULL);                   // Asserting realloc with NULL ptr behaves like malloc
    tsmm_free(p);
}

void test_realloc() {
    fprintf(stderr, "----Testing realloc...\n");

    void *p = tsmm_malloc(32); 
    p = tsmm_realloc(p, 64);
    CHECK(p != NULL);
    tsmm_free(p);
}

void test_calloc() {
    fprintf(stderr, "----Testing calloc...\n");
    
    void *p = tsmm_calloc(1, sizeof(int));
    CHECK(p != NULL);       // Assert allocation works
    CHECK(*(int *)p == 0);  // Assert zero intialization
    tsmm_free(p);
}

void test_multiple_allocations() {
    fprintf(stderr, "----Testing multiple allocations...\n");
    void *ptrs[1000];
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = tsmm_malloc((i * i)/2 + 1);
        CHECK(ptrs[i] != NULL);
    }
    for (int i = 0; i < 1000; i++)
        tsmm_free(ptrs[i]);
}

