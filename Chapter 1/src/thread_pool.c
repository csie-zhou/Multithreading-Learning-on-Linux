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
