#include "test.h"

// Tests
#include "sanity.h"
#include "invariants.h"

int failures = 0;

int main() {
    // init
    tsmm_init();
    int n = 1;

    fprintf(stderr, "Test %d: Sanity Test:\n", n++);
    test_simple_allocation();
    test_edge_cases();
    test_realloc();
    test_calloc();
    test_multiple_allocations();

    fprintf(stderr, "\nTest %d: Checking Heap Invariants:\n", n++);
    check_invariants(500);
    
    // Test summary
    if (failures == 0) {
        fprintf(stderr, "All tests passed\n");
        return 0;
    }
    else {
        fprintf(stderr, "%d tests failed\n", failures);
        return 1;
    }
}
