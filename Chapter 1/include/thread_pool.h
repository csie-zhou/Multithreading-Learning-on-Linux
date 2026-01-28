#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

/*  1. Define thread task structure
    function: a func pointer, points to the task
    argument: arguments passing to function (void*)
 */
typedef struct {
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
} thread_pool_t;

/* API Declaration */
thread_pool_t* thread_pool_create(int thread_count, int queue_size);
int thread_pool_destroy(thread_pool_t *pool);

#endif // THREAD_POOL_H
