#include "test.h"

// Tests
#include "st_sanity.h"
#include "st_invariants.h"
#include "mt_invariants.h"


int failures = 0;

int main() {
    // init
    tsmm_init();
    int n = 1;

    // Single Thread Tests
    fprintf(stderr, "\n**************************\n");
    fprintf(stderr, "* Single-Threadded Tests *\n");
    fprintf(stderr, "**************************\n");
    fprintf(stderr, "Test %d: Sanity Test:\n", n++);
    st_sanity_check();

    fprintf(stderr, "\nTest %d: Checking Heap Invariants:\n", n++);
    st_test_invariants(100000);


    fprintf(stderr, "\n**************************\n");
    fprintf(stderr, "*  Multi-Threadded Tests *\n");
    fprintf(stderr, "**************************\n");
    fprintf(stderr, "\nTest %d: Checking Heap Invariants:\n", n++);
    mt_test_invariants(100000);
    
    // Summary
    if (failures == 0) {
        fprintf(stderr, "\nAll tests passed\n");
        return 0;
    }
    else {
        fprintf(stderr, "%d tests failed\n", failures);
        return 1;
    }
}
