#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

/* Testing function for Chapter 2, manually consume task for Worker Thread */
int thread_pool_run_pending_task(thread_pool_t *pool) {
    thread_task_t task;

    if (pool == NULL) return -1;

    pthread_mutex_lock(&(pool->lock));

    /* Check if empty */
    if (pool->count == 0) {
        pthread_mutex_unlock(&(pool->lock));
        return -1; // Empty
    }

    /* 1. Consume task from Head */
    task.function = pool->queue[pool->head].function;
    task.argument = pool->queue[pool->head].argument;

    /* 2. Update Head (Circular Buffer) */
    pool->head = (pool->head + 1) % pool->queue_size;
    pool->count--;

    pthread_mutex_unlock(&(pool->lock));

    /* 3. Execute (After UNLOCK, such that avoid deadlock) */
    (*(task.function))(task.argument);

    return 0;
}
