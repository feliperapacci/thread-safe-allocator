#include "st_invariants.h"
#include "mt_invariants.h"
#include "test.h"
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

void mt_check_invariants() {
    // maybe acquire lock to avoid problems?
    // instead, require all threads to be joined for this function.
    st_check_invariants();  
}

#define PTR_ARR_SIZE 100000
#define NUM_THREADS 8
#define MAX_PTRS_PER_THREAD (PTR_ARR_SIZE / NUM_THREADS)

typedef struct {
    void **first_ptr;
    int num_ops;
} launch_args_t;

void *launch_mallocs(void *arg) {
    launch_args_t *args = (launch_args_t*) arg;
    void **first_ptr = args->first_ptr;
    int num_ops = args->num_ops;

    for (int i = 0; i < num_ops; i++) {
        first_ptr[i] = tsmm_malloc(random_size());
    }

    return NULL;
}

void *launch_reallocs(void *arg) {
    launch_args_t *args = (launch_args_t*) arg;
    void **first_ptr = args->first_ptr;
    int num_ops = args->num_ops;

    for (int i = 0; i < num_ops; i++) {
        first_ptr[i] = tsmm_realloc(first_ptr[i], random_size());
    }

    return NULL;
}

void *launch_frees(void *arg) {
    launch_args_t *args = (launch_args_t*) arg;
    void **first_ptr = args->first_ptr;
    int num_ops = args->num_ops;

    for (int i = 0; i < num_ops; i++) {
        tsmm_free(first_ptr[i]);
        first_ptr[i] = NULL;
    }

    return NULL;
}

// n is the number of ops per thread
void mt_test_invariants(unsigned int n) {
    if (n > MAX_PTRS_PER_THREAD) n = MAX_PTRS_PER_THREAD;
    void *ptrs[PTR_ARR_SIZE];
    srand(time(NULL));

    pthread_t threads[NUM_THREADS];
    launch_args_t args[NUM_THREADS];
    for (int checks = 0; checks < 4; checks++) {
        // Malloc
        fprintf(stderr, "----Testing Malloc %d...\n", checks);
        for (int i = 0; i < NUM_THREADS; i++) {
            args[i] = (launch_args_t){ .first_ptr = (ptrs + i*n), .num_ops = n };
            if (pthread_create(&threads[i], NULL, launch_mallocs, &args[i]) != 0) {
                fprintf(stderr, "------Failed to launch malloc[%d]\n", i);
            }
        } 
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        mt_check_invariants();

        // Realloc 
        fprintf(stderr, "----Testing Realloc %d...\n", checks);
        for (int i = 0; i < NUM_THREADS; i++) {
            args[i] = (launch_args_t){ .first_ptr = (ptrs + i*n), .num_ops = n };
            if (pthread_create(&threads[i], NULL, launch_reallocs, &args[i]) != 0) {
                fprintf(stderr, "------Failed to launch realloc[%d]\n", i);
            }
        } 
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        mt_check_invariants();

        // Free
        fprintf(stderr, "----Testing Free %d...\n", checks);
        for (int i = 0; i < NUM_THREADS; i++) {
            args[i] = (launch_args_t){ .first_ptr = (ptrs + i*n), .num_ops = n };
            if (pthread_create(&threads[i], NULL, launch_frees, &args[i]) != 0) {
                fprintf(stderr, "------Failed to launch free[%d]\n", i);
            }
        } 
        for (int i = 0; i < NUM_THREADS; i++) {
            pthread_join(threads[i], NULL);
        }
        mt_check_invariants();
    }
}
