#define _GNU_SOURCE // Enalbe Linux Extension
#include <sched.h>  // Marcos like: CPU_SET, CPU_ZERO
#include <unistd.h>
#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

/* Every thread execute this function after called, until pool is destroyed */
static void *thread_pool_worker(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;
    thread_task_t task;

    /* Debug Log */
    int current_core = sched_getcpu();
    printf("[Worker Debug] Thread ID %lu bound to Core %d\n",
           pthread_self(), current_core);
    while (1)
    {
        /* 1. Lock for Queue */
        pthread_mutex_lock(&(pool->lock));

        /* 2. Wait condition
         * If Queue is empty --> wait
         * We use busy waiting here
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

        /* TODO: Chapter 10. Atomic Add */
        /* This line can speed up to 10x compare to Mutex Lock */
        atomic_fetch_add(&(pool->task_completed), 1);
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

    /* TODO: Chapter 10. Initialize task_complete */
    atomic_init(&(pool->task_completed), 0); // Not pool->task_completed = 0

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

    /* TODO: Chapter 7. Get the cores of CPU / Add CPU Affinity */
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    for (int i = 0; i < thread_count; i++)
    {
        if (pthread_create(&(pool->threads[i]), NULL, thread_pool_worker, (void *)pool) != 0)
        {
            // Trouble shooting
            return NULL;
        }

        /* Add CPU Affinity */
        /* 7-1: Announce spu_set_t variable (Bitmask) */
        cpu_set_t cpuset;

        /* 7-2: Clean set */
        CPU_ZERO(&cpuset);

        /* 7-3: Define which core to allocate (Round Robin) */
        int target_core = i % num_cores;

        /* 7-4: Add target core into set */
        CPU_SET(target_core, &cpuset);

        /* 7-5: Call Linux Affinity */
        /* Parameters:
         * 1. thread ID
         * 2. size of cpuset
         * 3. pointer to cpuset
         */
        int rc = pthread_setaffinity_np(pool->threads[i], sizeof(cpu_set_t), &cpuset);

        if (rc != 0)
        {
            perror("Failed to set affinity (non-fatal)");
        }
        else
        {
            // printf("Thread %d bound to Core %d\n", i, target_core);
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

    /* Chapter 4: Comes a new task, call a worker thread */
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
