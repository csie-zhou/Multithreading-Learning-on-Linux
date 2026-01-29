# Chapter 4. Spawning Threads (Concurrent Execute)
From now on, our program is not executing Sequentially, but **Concurrently** with 5 threads (1 Main Thread + 4 Worker Threads).
## Key Tasks
1. Implement `worker` function: let Worker Thread taking tasks and consuming tasks.
2. Call `pthread_create`: Actual creation of the thread from OS.
### 1. Implement Worker Function
Write a `static` function in `src/thread_pool.c` for local usage. In previous chapter, we manually call `run_pending_task` to assign task to Worker. Now, we are writing a infinity loop, the Worker Thread checks the queue itself.  

Paste this at the top of `thread_pool_create`:
```C
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

```

### 2. Modify Create function to run Worker Threads
Now we have `thread_pool_worker`, we have to modify `create` fucntion as it runs `thread_pool_worker` while a worker thread is created by OS.  

Modify `thread_pool_create` in `src/thread_pool.c`. Find the annotation of `/* TODO: Chapter 4... */`, paste this:
```C
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
```

### 3. Signal(Wake Up) the Worker Thread
In Step 1, we wrote `pthread_cond_wait`(sleep). If anyone `add` a task but the Worker Thread is not waken, the task won't be executed.  

Modify `thread_pool_add` function in `src/thread_pool.c`. Add this line before `pthread_mutex_unlock`:
```C
  /* ... pool->tail Updated ... */

  pool->count++;

  /* Chapter 4. Signal(Wake Up) a Worker Thread */
  pthread_cond_signal(&(pool->notify)); // <--- Add this

  /* 5. Unlock */
  pthread_mutex_unlock(&(pool->lock));
```

### 4. Concurrent Execute
Now the Thread Pool can be auto executed, we don't need `run_pending_task` anymore.  

Modify `src/main.c`:  
1. Add Header for `usleep` and `sleep`: *`#include <unistd.h>`*
2. Remove all `thread_pool_run_pending_task` loops.
3. Add `sleep` to visualize how Worker Thread Concurrently Execute (Otherwise main executes rapidly and destroy, before Worker Thread finishes it's task).
```C
int main() {
    /* Day 4: Worker Thread */
    printf("Starting Day 4: Multi-threading Test...\n");

    /* 4 worker threads */
    thread_pool_t *pool = thread_pool_create(4, 10);
    if (!pool)
        return 1;

    /* Assume 8 tasks */
    for (int i = 0; i < 8; i++)
    {
        math_args_t *args = (math_args_t *)malloc(sizeof(math_args_t));
        if (!args)
            continue;

        args->operator_id = i;
        args->operand_a = i * 10;
        args->operand_b = i * 2;

        printf("[Main] Adding Task %d\n", i);
        thread_pool_add(pool, heavy_calculation, args);

        usleep(100000);
    }

    /* Let Worker have time to handle tasks */
    printf("[Main] Sleeping for 2 seconds to let workers finish...\n");
    sleep(2);

    printf("[Main] Destroying pool...\n");
    thread_pool_destroy(pool);

    printf("Done.\n");
    return 0;
}
```

### 5. Execute and Test
1. Compile
```
make
```
2. Execute
```
./c_thread_pool_demo
```
3. Expectations  
Ouput order may differ. Example:
```
[Main] Adding Task 0
[Main] Adding Task 1
[Worker] Task 0: ...
[Main] Adding Task 2
[Worker] Task 2: ... (Thread 2 takes Task 2)
[Worker] Task 1: ... (Thread 1 done Task 1)
```
