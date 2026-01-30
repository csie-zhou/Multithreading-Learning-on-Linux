# Chapter 10. High Efficiency
If we want to know how great is our thread pool **performance**? What's the **throughput**? An easy way is to add a `pthread_mutex` to count, but it extremely lower the efficiency.

## Key Tasks
1. Introduce **C11 Atomics**
2. Compute TPS (Task Per Second)

## Mutex vs Atomics
- **Mutex**: When you `lock`, it might requires System Call, and even make Workers turn into kernel mode to sleep. **Very Slow!**
- **Atomic**: Actomic directly compile special commands of CPU (eg. `lock xadd` in x86). This can be done in **User Mode**, quicker.

## Compute TPS
We have to modify Thread Pool, add a counter for done tasks.  

### 1. Add Header and Counter
Add a header of C11 Actomic function library, and add a counter in struct.  

Modify the lines refer to Chapter 10 in `include/thread_pool.h`. 
```C
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdatomic.h> // <--- Chapter 10. Add library of C11 Atomic

typedef struct
{
    void (*function)(void *);
    void *argument;
} thread_task_t;

/*  2. Define thread pool structure
    With Sync (Lock/Cond), Ring Buffer(Task Queue) and array of threads
*/
typedef struct
{
    pthread_mutex_t lock;  // Mutex Lock of Queue
    pthread_cond_t notify; // Conditional Variable of worker thread
    pthread_t *threads;    // Array of thread ID (Dynamic allocate)
    thread_task_t *queue;  // Task Queue
    int thread_count;      // Numbers of threads
    int queue_size;        // Size of Queue
    int head;              // Ring Buffer Head (taking next task)
    int tail;              // Ring Buffer Tail (adding a task)
    int count;             // Number of threads
    int shutdown;          // Flag (0: operate, 1: shutdown)

    /* TODO: Chapter 10. Add atomic counter */
    /* _Atomic is keyword in C11, ensure the variable doing ++ -- is atomic exectued */
    atomic_int task_completed;
} thread_pool_t;

/* API Declaration */
thread_pool_t *thread_pool_create(int thread_count, int queue_size);
int thread_pool_add(thread_pool_t *pool, void (*function)(void *), void *argument);
int thread_pool_destroy(thread_pool_t *pool);

#endif
```

### 2. Modify Counter Logic
In `thread_pool.c`, we have to initialize `task_completed` in `create`, and add 1 after `worker` complete the task.  

Modify these in `thread_pool_create`:
```C
    /* 2. ... Initialize variables ... */
    pool->shutdown = 0;

    /* TODO: Chapter 10. Initialize task_complete */
    atomic_init(&(pool->task_completed), 0); // Not pool->task_completed = 0

    /* 3.Allocate Arrays (Threads & Queue) ... */
```
Modify these in `thread_pool_worker`:
```C
        /* ... (In while) ... */

        /* 6. Execute */
        (*(task.function))(task.argument);

        /* TODO: Chapter 10. Atomic Add */
        /* This line can speed up to 10x compare to Mutex Lock */
        atomic_fetch_add(&(pool->task_completed), 1);
    }
```

### 3. Write Benchmark in Main
We are going to send 1,000,000 empty tasks, see how our benchmark performs.  

Copy paste this in `src/main.c`:
```C
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "thread_pool.h"

#define TASKS_COUNT 1000000 // 1M Tasks

/* Empty task：Do nothing, just for pushing pool limit */
void dummy_task(void *arg) {
    (void)arg;
    // Do nothing
}

/* Counter Function Helper */
double get_time_sec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

int main() {
    printf("Starting Chapter 10: Final Benchmark (Throughput Test)...\n");

    // 1. Create Pool (4 threads, Queue size 65536)
    // Bigger Queue can main thread be blocked, for better testing
    thread_pool_t *pool = thread_pool_create(4, 65536);
    if (!pool) return 1;

    printf("[Main] Dispatching %d tasks...\n", TASKS_COUNT);
    double start = get_time_sec();

    // 2. Send Tasks
    for (int i = 0; i < TASKS_COUNT; i++) {
        // If Queue full then retry (Busy Retry)
        while (thread_pool_add(pool, dummy_task, NULL) != 0) {
            // Release some CPU for Worker
            // We don't use usleep, use sched_yield() can create higher performance
            // To make it simple, we do no-op, let it spin
        }
    }

    // 3. Wait for all tasks done
    // Use atomic counter to check progress, not using sleep to guess the time
    while (1) {
        // Atomic Load
        int completed = atomic_load(&(pool->tasks_completed)); // Load to int to compare
        if (completed >= TASKS_COUNT) {
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
```

### 4. Compile and Execute
1. `make`
2. `./c_thread_pool_demo`
3. Expected: In modern computer, the throughput can be up to 1M ~ 3M tasks per second.
