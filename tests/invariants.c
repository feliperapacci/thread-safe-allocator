#include "mm.h"
#include "mmhelper.h"
#include "test.h"
#include "invariants.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Heap invariants:
// 1. Valid prologue
// --- Block Invariants ---
// 2. Block header + footer match
// 3. Every payload aligned to 16 bytes
// 4. Every block's size is at least SMALLEST_BLOCK
// 5. Every block's size is a multiple of 16
// 6. No two consecutive free blocks (should be coalesced)
// ------------------------
// 7. Valid Epilogue
// 
// --- List  Invariants ---
// 8. All blocks in between mem_heap_lo() and mem_heap_hi()
// 9. Every block in list should be free
// 10. Every block should be in the correct list
// 11. Free list's prev and next should be consistent
// ------------------------

void check_invariants() {
    fprintf(stderr, "----Testing for valid prologue...\n");
    // 1. Valid Prologue
    size_t *prologue = mem_heap_lo();
    CHECK(get_size(*prologue) == 0);     // Size should be zero
    CHECK(get_alloc(*prologue) == 1);    // Should be allocated
    
    // --- Block Invariants ---
    fprintf(stderr, "----Testing block-level invariants...\n");
    size_t *curr = get_next_adjacent(prologue);
    size_t *prev = NULL;

    int prev_failures = failures;
    size_t *heap_top = mem_heap_hi();

    while (get_size(*curr) != 0 && curr <= heap_top){

        // 2. Header and Footer Match
        CHECK(*get_footer(curr) == *curr);

        // 3. Payload aligned to 16 bytes
        CHECK(((uintptr_t) (curr+1)) % 16 == 0);

        // 4. Every block's size is at least SMALLEST_BLOCK
        CHECK(get_size(*curr) >= SMALLEST_BLOCK);
        
        // 5. Every block's size is a multiple of 16
        CHECK(get_size(*curr) % 16 == 0);

        // 6. No two consecutive free blocks
        if (prev != NULL) {
            bool curr_free = get_alloc(*curr) == 0; // get_alloc returns 0 if free
            bool prev_free = get_alloc(*prev) == 0;
            CHECK(!(curr_free && prev_free));
        }

        // Iterate
        prev = curr;
        curr = get_next_adjacent(curr);

        // If any of the previous checks failed, stop checking 2-6 to avoid clogging stderr
        // Iterate until expected epilogue is reached to check invariant 7
        while (prev_failures != failures && get_size(*curr) != 0) {
            curr = get_next_adjacent(curr);
        }
    }
    
    // 7. Valid Epilogue
    CHECK(get_size(*curr) == 0);    // Size should be zero
    CHECK(get_alloc(*curr) == 1);   // Should be allocated

    // --- List Invariants ---
    prev_failures = failures;
    fprintf(stderr, "----Testing list invariants...\n");
    for (int i = 0; i < LIST_NUM; i++) {
        size_t *curr = lists[i]; // header to first item in list
        while (curr != NULL) {
            // 8. All blocks in between mem_heap_lo() and mem_heap_hi()
            CHECK(curr >= prologue && curr <= heap_top);

            // 9. Every block in list should be free
            CHECK(get_alloc(*curr) == 0);

            // 10. Every block should be in the correct list
            size_t size = get_size(*curr);
            CHECK((int)get_list(size) == i);

            // 11. Free list's prev and next should be consistent
            size_t *next = get_next_free(curr);
            if (next) {
                CHECK(get_prev_free(next) == curr);
            }

            // iterate
            curr = next;

            // If any of the previous checks failed, stop checking 8-11 to avoid clogging stderr
            if (prev_failures != failures) break;
        }
        if (prev_failures != failures) break;
    }
}

size_t random_size() {
    int num = rand() % LIST_NUM; 
    switch (num) {
        case 0:     return 16;
        case 1:     return 48;
        case 2:     return 80;
        case 3:     return 112;
        case 4:     return 144;
        case 5:     return 176;
        case 6:     return 200;
        case 7:     return 230;
        case 8:     return 496;
        case 9:     return 1000;
        case 10:     return 2000;
        case 11:    return 4000;
        case 12:    return 8000;
        case 13:    return 16000;
        case 14:    return 32000;
        default:    return 50000;
    }
}


#define MAX_PTRS 50000

void test_invariants(unsigned int n) {

    if (n > MAX_PTRS) n = MAX_PTRS;

    void *ptrs[MAX_PTRS];
    srand(time(NULL));

    unsigned int checks = 4;
    unsigned int op_per_check = n / checks;
    for (unsigned int i = 0; i < checks; i++) {
        // Malloc
        for (unsigned int j = 0; j < op_per_check; j++) {
            ptrs[i*op_per_check + j] = tsmm_malloc(random_size());
        }
        fprintf(stderr, "--Testing Malloc %d..\n", i);
        check_invariants();

        // Realloc
        for (unsigned int j = 0; j < op_per_check; j++) {
            ptrs[i*op_per_check + j] = tsmm_realloc(ptrs[i*op_per_check + j], random_size());
        }
        fprintf(stderr, "--Testing Realloc %d..\n", i);
        check_invariants();

        // Free
        for (unsigned int j = 0; j < op_per_check; j++) {
            tsmm_free(ptrs[i*op_per_check + j]);
        }
        fprintf(stderr, "--Testing Free %d..\n", i);
        check_invariants();
    }

}
