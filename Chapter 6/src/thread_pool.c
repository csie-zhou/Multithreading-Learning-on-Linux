#include <unistd.h>
#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

/* Every thread execute this function after called, until pool is destroyed */
static void *thread_pool_worker(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    thread_task_t task;

    while (1)
    {
        /* 1. Lock for Queue */
        pthread_mutex_lock(&(pool->lock));

        /* 2. Wait condition
         * If Queue is empty --> wait
         * We use "Polling" here
         * In order to avoid CPU usage here reaches 100%, we use pthread_cond_wait here
         * (In Chapter 6, we will learn the concept of Condition Variable)
         */
        while (pool->count == 0 && pool->shutdown == 0)
        {
            // Wait mode: (realease lock -> wait -> awake -> get lock)
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        /* 3. Judge if shutdown */
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL); // thread exit
        }

        /* 4. Consume a task */
        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;

        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count--;

        /* 5. Unlock */
        pthread_mutex_unlock(&(pool->lock));

        /* 6. Execute */
        (*(task.function))(task.argument);
    }

    return NULL;
}

thread_pool_t *thread_pool_create(int thread_count, int queue_size)
{
    if (thread_count <= 0 || queue_size <= 0)
        return NULL;

    /* 1. Allocate thread pool */
    thread_pool_t *pool = malloc(sizeof(thread_pool_t));
    if (pool == NULL)
    {
        perror("Failed to allocate thread pool.");
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

    if (pool->threads == NULL || pool->queue == NULL)
    {
        perror("Failed to allocate threads or queue.");
        goto err_cleanup;
    }

    /* 4. Initialize Lock & Conditional Variable */
    if (pthread_mutex_init(&(pool->lock), NULL) != 0 || pthread_cond_init(&(pool->notify), NULL) != 0)
    {
        perror("Failed to init mutex lock or cond");
        goto err_cleanup;
    }

    /* TODO: Chapter 4. Call pthread_create to activate worker thread */
    for (int i = 0; i < thread_count; i++)
    {
        /* * pthread_create parameter:
         * 1. &pool->threads[i]: thread ID location
         * 2. NULL: default properties
         * 3. thread_pool_worker: function that worker thread is going to run
         * 4. pool: function argument
         */
        if (pthread_create(&(pool->threads[i]), NULL, thread_pool_worker, (void *)pool) != 0)
        {
            perror("Failed to create thread.");
            thread_pool_destroy(pool);
            return NULL;
        }
    }

    /* Successfully activate all worker threads*/
    pool->thread_count = thread_count;

    return pool;

err_cleanup:
    /* Handle allocate errors: free all resource*/
    if (pool->threads)
        free(pool->threads);
    if (pool->queue)
        free(pool->queue);
    free(pool);
    return NULL;
}

int thread_pool_add(thread_pool_t *pool, void (*function)(void *), void *argument)
{
    if (pool == NULL || function == NULL)
    {
        return -1; // Invalid arguments
    }

    /* 1. Lock (protect queue structure) */
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return -1;
    }

    /* 2. Check if Queue is full */
    if (pool->count == pool->queue_size)
    {
        pthread_mutex_unlock(&(pool->lock));
        return -2; // -2: Full queue
    }

    /* 3. Add task in tail */
    pool->queue[pool->tail].function = function;
    pool->queue[pool->tail].argument = argument;

    /* Update tail (Ring Buffer/Circular Logic) */
    pool->tail = (pool->tail + 1) % pool->queue_size;
    pool->count++;

    /* Chapter 4: Signal(Wake Up) a Worker Thread */
    pthread_cond_signal(&(pool->notify));

    /* 5. Unlock */
    pthread_mutex_unlock(&(pool->lock));

    /* pthread_cond_signal */

    return 0;
}

int thread_pool_destroy(thread_pool_t *pool)
{
    if (pool == NULL)
        return -1;

    /* 1. Get Lock */
    if (pthread_mutex_lock(&(pool->lock)) != 0)
    {
        return -1;
    }

    /* 2. Set shoutdown flag, so that worker leaves while loop */
    pool->shutdown = 1;

    /* 3. Wake all sleep workers */
    if (pthread_cond_broadcast(&(pool->notify)) != 0)
    {
        pthread_mutex_unlock(&(pool->lock)); // Unlock before every return
        return -1;
    }

    /* 4. Unlock to let worker threads join */
    /* If you don't unlock first, worker cannot get lock when awake, cannot correctly check shutdown == 1 */
    /* Cause deadlock in main thread */
    pthread_mutex_unlock(&(pool->lock));

    for (int i = 0; i < pool->thread_count; i++)
    {
        if (pthread_join(pool->threads[i], NULL) != 0)
        {
            // Realistic, here will have log
        }
    }

    /* 5. Resource recycle */
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));

    /* 6. Free Memory */
    free(pool->queue);
    free(pool->threads);
    free(pool);

    return 0;
}
