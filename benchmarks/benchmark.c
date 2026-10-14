#include "mm.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Compares whichever backend tsmm_lib was built with (see TSMM_BACKEND in
// CMakeLists.txt) across thread counts, size distributions, and access
// patterns. Build once per backend into separate build dirs and diff the
// two tables.

#define OPS_PER_THREAD 20000
#define MAX_THREADS 16

static const int THREAD_COUNTS[] = {1, 2, 4, 8, 16};
static const int NUM_THREAD_COUNTS = sizeof(THREAD_COUNTS) / sizeof(THREAD_COUNTS[0]);

// Same 16-value table used by st_invariants.c/mt_invariants.c's
// random_size(), kept local here so this file has no dependency on tests/.
// include_big=false restricts the draw to the 15 sizes that stay within a
// cached class; include_big=true adds the one size (50000) that falls into
// LIST_BIG, which neither backend caches.
static size_t random_size(unsigned int *seed, bool include_big) {
    static const size_t sizes[] = {
        16, 48, 80, 112, 144, 176, 200, 230,
        496, 1000, 2000, 4000, 8000, 16000, 32000, 50000
    };
    int n = include_big ? 16 : 15;
    return sizes[rand_r(seed) % n];
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

typedef struct {
    void **ptrs;
    unsigned int ops;
    bool include_big;
    unsigned int seed;
} size_args_t;

typedef struct {
    void **ptrs;
    unsigned int ops;
} free_args_t;

static void *malloc_worker(void *arg) {
    size_args_t *a = (size_args_t *)arg;
    for (unsigned int i = 0; i < a->ops; i++)
        a->ptrs[i] = tsmm_malloc(random_size(&a->seed, a->include_big));
    return NULL;
}

static void *realloc_worker(void *arg) {
    size_args_t *a = (size_args_t *)arg;
    for (unsigned int i = 0; i < a->ops; i++)
        a->ptrs[i] = tsmm_realloc(a->ptrs[i], random_size(&a->seed, a->include_big));
    return NULL;
}

static void *free_worker(void *arg) {
    free_args_t *a = (free_args_t *)arg;
    for (unsigned int i = 0; i < a->ops; i++)
        tsmm_free(a->ptrs[i]);
    return NULL;
}

static void report(int num_threads, const char *phase, const char *dist,
                    unsigned long total_ops, double elapsed_sec) {
    printf("%-8d %-11s %-12s %10lu %14.0f\n",
           num_threads, phase, dist, total_ops, total_ops / elapsed_sec);
}

int main(void) {
    if (!tsmm_init()) {
        fprintf(stderr, "tsmm_init failed\n");
        return 1;
    }

    printf("%-8s %-11s %-12s %10s %14s\n",
           "threads", "phase", "size_dist", "ops", "ops/sec");

    for (int t = 0; t < NUM_THREAD_COUNTS; t++) {
        int num_threads = THREAD_COUNTS[t];
        unsigned long total_ops = (unsigned long)num_threads * OPS_PER_THREAD;

        for (int inc_big = 0; inc_big <= 1; inc_big++) {
            const char *dist = inc_big ? "with_big" : "cached_only";

            void **ptrs = malloc(sizeof(void *) * num_threads * OPS_PER_THREAD);
            if (!ptrs) { fprintf(stderr, "benchmark: out of memory\n"); return 1; }

            pthread_t threads[MAX_THREADS];
            size_args_t sargs[MAX_THREADS];
            for (int i = 0; i < num_threads; i++) {
                sargs[i] = (size_args_t){
                    .ptrs = ptrs + (size_t)i * OPS_PER_THREAD,
                    .ops = OPS_PER_THREAD,
                    .include_big = inc_big,
                    .seed = (unsigned int)(i + 1) * 2654435761u
                };
            }

            // Malloc phase.
            double t0 = now_sec();
            for (int i = 0; i < num_threads; i++)
                pthread_create(&threads[i], NULL, malloc_worker, &sargs[i]);
            for (int i = 0; i < num_threads; i++)
                pthread_join(threads[i], NULL);
            report(num_threads, "malloc", dist, total_ops, now_sec() - t0);

            // Realloc phase -- each thread resizes what it already owns.
            double t1 = now_sec();
            for (int i = 0; i < num_threads; i++)
                pthread_create(&threads[i], NULL, realloc_worker, &sargs[i]);
            for (int i = 0; i < num_threads; i++)
                pthread_join(threads[i], NULL);
            report(num_threads, "realloc", dist, total_ops, now_sec() - t1);

            // Free phase -- own pointers.
            free_args_t fargs[MAX_THREADS];
            for (int i = 0; i < num_threads; i++)
                fargs[i] = (free_args_t){ .ptrs = sargs[i].ptrs, .ops = OPS_PER_THREAD };

            double t2 = now_sec();
            for (int i = 0; i < num_threads; i++)
                pthread_create(&threads[i], NULL, free_worker, &fargs[i]);
            for (int i = 0; i < num_threads; i++)
                pthread_join(threads[i], NULL);
            report(num_threads, "free_own", dist, total_ops, now_sec() - t2);

            // Cross-thread free: malloc again, then have each thread free
            // the slice a DIFFERENT thread allocated (rotated ownership).
            for (int i = 0; i < num_threads; i++)
                pthread_create(&threads[i], NULL, malloc_worker, &sargs[i]);
            for (int i = 0; i < num_threads; i++)
                pthread_join(threads[i], NULL);

            free_args_t xargs[MAX_THREADS];
            for (int i = 0; i < num_threads; i++) {
                int owner = (i + 1) % num_threads;
                xargs[i] = (free_args_t){ .ptrs = sargs[owner].ptrs, .ops = OPS_PER_THREAD };
            }

            double t3 = now_sec();
            for (int i = 0; i < num_threads; i++)
                pthread_create(&threads[i], NULL, free_worker, &xargs[i]);
            for (int i = 0; i < num_threads; i++)
                pthread_join(threads[i], NULL);
            report(num_threads, "free_cross", dist, total_ops, now_sec() - t3);

            free(ptrs);
        }
    }

    return 0;
}
