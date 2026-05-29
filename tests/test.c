#include "test.h"

// Tests
#include "sanity.h"

int failures = 0;

int main() {
    // init
    tsmm_init();
    int n = 1;

    printf("Test %d: Sanity Test:\n", n++);
    test_simple_allocation();
    test_edge_cases();
    test_realloc();
    test_calloc();
    test_multiple_allocations();
    
    // Test summary
    if (failures == 0) {
        printf("All tests passed\n");
        return 0;
    }
    else {
        printf("%d tests failed\n", failures);
        return 1;
    }
}
