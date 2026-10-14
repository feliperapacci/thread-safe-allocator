#include "mm.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Fixed at 8 threads to match this laptop's core count: enough parallelism
// to actually exercise the allocator's thread-safety path, without
// oversubscribing the CPU and paying scheduler noise on top of allocator
// cost. Unlike benchmark.c, this doesn't sweep thread counts, so it says
// nothing about scaling above/below 8 -- just "how fast is a full-core
// workload on my machine right now."

#define NUM_THREADS 8
#define OPS_PER_THREAD 200000
#define LIVE_SLOTS 256 // per-thread working set of live allocations

#ifndef TSMM_BACKEND_NAME
#define TSMM_BACKEND_NAME "unknown"
#endif

static size_t random_size(unsigned int *seed) {
    static const size_t sizes[] = {
        16, 48, 80, 112, 144, 176, 200, 230,
        496, 1000, 2000, 4000, 8000, 16000, 32000
    };
    return sizes[rand_r(seed) % (sizeof(sizes) / sizeof(sizes[0]))];
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Runs n random malloc/realloc/free operations against a small rotating
// working set of live pointers, then frees whatever's left standing.
static void run_random_ops(unsigned int seed, unsigned long n) {
    void *live[LIVE_SLOTS] = {0};

    for (unsigned long i = 0; i < n; i++) {
        int slot = rand_r(&seed) % LIVE_SLOTS;
        int action = rand_r(&seed) % 3;

        if (!live[slot] || action == 0) {
            live[slot] = tsmm_malloc(random_size(&seed));
        } else if (action == 1) {
            live[slot] = tsmm_realloc(live[slot], random_size(&seed));
        } else {
            tsmm_free(live[slot]);
            live[slot] = NULL;
        }
    }

    for (int i = 0; i < LIVE_SLOTS; i++)
        tsmm_free(live[i]);
}

typedef struct {
    unsigned int seed;
    unsigned long ops;
} worker_args_t;

static void *worker(void *arg) {
    worker_args_t *a = (worker_args_t *)arg;
    run_random_ops(a->seed, a->ops);
    return NULL;
}

int main(void) {
    if (!tsmm_init()) {
        fprintf(stderr, "tsmm_init failed\n");
        return 1;
    }

    pthread_t threads[NUM_THREADS];
    worker_args_t args[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i] = (worker_args_t){
            .seed = (unsigned int)(i + 1) * 2654435761u,
            .ops = OPS_PER_THREAD
        };
    }

    double t0 = now_sec();
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, worker, &args[i]);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    double elapsed = now_sec() - t0;

    unsigned long total_ops = (unsigned long)NUM_THREADS * OPS_PER_THREAD;
    printf("%-10s threads=%d ops=%lu time=%.3fs ops/sec=%.0f\n",
           TSMM_BACKEND_NAME, NUM_THREADS, total_ops, elapsed,
           total_ops / elapsed);

    return 0;
}
