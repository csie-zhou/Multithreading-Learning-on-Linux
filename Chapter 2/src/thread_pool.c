#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

int thread_pool_add(thread_pool_t *pool, void (*function)(void *), void *argument) {
    if (pool == NULL || function == NULL) {
        return -1; // Invalid arguments
    }

    /* 1. Lock for Queue */
    if (pthread_mutex_lock(&(pool->lock)) != 0) {
        return -1;
    }

    /* 2. Check if Queue is full */
    if (pool->count == pool->queue_size) {
        // Queue Full
        pthread_mutex_unlock(&(pool->lock));
        return -2; // -2 represents full (Common seen in Driver：-ENOMEM or -EBUSY)
    }

    /* 3. Add task to Tail */
    pool->queue[pool->tail].function = function;
    pool->queue[pool->tail].argument = argument;

    /* 4. Update Tail (Circular Buffer) */
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->count++;

    /* 5. Unlock */
    pthread_mutex_unlock(&(pool->lock));

    /* TODO: Chapter 6. Call pthread_cond_signal to announce Worker new task */

    return 0; // Success
}

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
