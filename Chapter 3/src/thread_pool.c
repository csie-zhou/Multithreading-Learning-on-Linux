#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

thread_pool_t* thread_pool_create(int thread_count, int queue_size) {
    thread_pool_t *pool;

    if (thread_count <= 0 || queue_size <= 0) {
        return NULL;
    }

    /* 1. Allocate thread pool */
    pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (pool == NULL) {
        perror("Failed to allocate thread pool"); // 印出系統錯誤訊息
        return NULL;
    }

    /* 2. Initialize variables */
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = 0;

    /* 3. Allocate Arrays (Threads & Queue) */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (thread_task_t *)malloc(sizeof(thread_task_t) * queue_size);

    /* (Check memory after every malloc) */
    if (pool->threads == NULL || pool->queue == NULL) {
        perror("Failed to allocate threads or queue");
        goto err_cleanup; // Use goto to error handling
    }

    /* 4. Initialize Lock & Conditional Variable */
    if (pthread_mutex_init(&(pool->lock), NULL) != 0 ||
        pthread_cond_init(&(pool->notify), NULL) != 0) {
        perror("Failed to init mutex or cond");
        goto err_cleanup;
    }

    /* * TODO: Chapter 4. Call pthread_create to activate worker thread */

    pool->thread_count = thread_count;

    return pool;

err_cleanup:
    /* Release resource */
    if (pool->threads) free(pool->threads);
    if (pool->queue) free(pool->queue);
    free(pool);
    return NULL;
}

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

int thread_pool_destroy(thread_pool_t *pool) {
    if (pool == NULL) return -1;

    /* Release resource (Order Matters!!!) */
    /* From pthread.h library */
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));

    /* Free Memory */
    free(pool->queue);
    free(pool->threads);
    free(pool);
    
    return 0;
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
