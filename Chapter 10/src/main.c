#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "thread_pool.h"

#define TASKS_COUNT 1000000 // 1M Tasks

/* Empty task：Do nothing, just for pushing pool limit */
void dummy_task(void *arg)
{
    (void)arg;
    // Do nothing
}

/* Counter Function Helper */
double get_time_sec()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

int main()
{
    printf("Starting Chapter 10: Final Benchmark (Throughput Test)...\n");

    // 1. Create Pool (4 threads, Queue size 65536)
    // Bigger Queue can main thread be blocked, for better testing
    thread_pool_t *pool = thread_pool_create(4, 65536);
    if (!pool)
        return 1;

    printf("[Main] Dispatching %d tasks...\n", TASKS_COUNT);
    double start = get_time_sec();

    // 2. Send Tasks
    for (int i = 0; i < TASKS_COUNT; i++)
    {
        // If Queue full then retry (Busy Retry)
        while (thread_pool_add(pool, dummy_task, NULL) != 0)
        {
            // Release some CPU for Worker
            // We don't use usleep, use sched_yield() can create higher performance
            // To make it simple, we do no-op, let it spin
        }
    }

    // 3. Wait for all tasks done
    // Use atomic counter to check progress, not using sleep to guess the time
    while (1)
    {
        // Atomic Load
        int completed = atomic_load(&(pool->task_completed)); // Load to int to compare
        if (completed >= TASKS_COUNT)
        {
            break;
        }
        // Sleep a while to release some CPU to Worker, don't let Main Occupies
        usleep(1000);
    }

    double end = get_time_sec();
    double duration = end - start;

    // 4. Show the Performance
    thread_pool_destroy(pool);

    printf("\n========================================\n");
    printf("Final Results:\n");
    printf("Tasks Processed: %d\n", TASKS_COUNT);
    printf("Time Taken:      %.4f seconds\n", duration);
    printf("Throughput:      %.2f Tasks/Sec\n", TASKS_COUNT / duration);
    printf("========================================\n");

    return 0;
}