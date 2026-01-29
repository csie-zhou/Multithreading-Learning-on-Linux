# Chapter 6. Memory Leak and Zombie Threads
In Chapter 5, we mentioned that `destroy` may casue Deadlock (holding the lock to Join). We are going to modify it in this chapter.

## Key Tasks
1. Modify 'thread_pool_destroy`
2. Use Valgrind to verify the result

## Modify thread_pool_destroy
Logic:
1. Broadcast to all threads that the pool is going to destroy.
2. Ensure all sleeping threads are waken (or they will sleep after destroy and cause Deadlock in Join).
3. Unlock first.
4. Wait till all threads join, then free memory.

Modify the function `thread_pool_destroy` in `src/thread_pool.c`:
```C
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
```
## Install Valgrind and Verify
Compile the above code by `make`. We use **Valgring** to debug.  

**Valgrind** is a powerful debug tool in Linux, it simulates CPU executing the program, and moniters `malloc` and `free` for every byte.  
### 1. Install Valgrind(In terminal)
```
sudo apt-get update
sudo apt-get install valgrind
```
### 2. Verify
```
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./c_thread_pool_demo
```

### 3. Result  
Focus in the **LEAK SUMMARY** in the end of the output. 

- Expected:
```
HEAP SUMMARY:
  in use at exit: 0 bytes in 0 blocks
  total heap usage: ... allocs, ... frees, ... bytes allocated
All heap blocks were freed -- no leaks are possible
ERROR SUMMARY: 0 errors from 0 contexts
```
Success if you see `All heap blocks were freed` in the output.  

- Failure (Memory Leak):
```
definitely lost: 40 bytes in 1 blocks
```
This demonstrates that something has `malloc` but does not `free`. Valgrind will show the line.  

## (Trouble Shooting if you meet)
The terminal stuck, not giving more output. Ex:
```Bash
vscode ➜ /workspaces/c_thread_pool $ valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./c_thread_pool_demo
==47088== Memcheck, a memory error detector
==47088== Copyright (C) 2002-2022, and GNU GPL'd, by Julian Seward et al.
==47088== Using Valgrind-3.19.0 and LibVEX; rerun with -h for copyright info
==47088== Command: ./c_thread_pool_demo
==47088== 
Starting Chapter 5: Race Condition Stress Test...
Depositing $1 for 10000 times...


```
This maybe an issue of **Starvation**, caused by Valgrind charateristic versus our `main.c`. Valgrind simulates CPU, force our multithreading task **Serial Executed**. Which is, only a thread is running at the same time, Valgrind switches.  

Ensure this line in `src/main.c`:
```C
    for (int i = 0; i < task_count; i++) {
        while (thread_pool_add(pool, deposit_task, NULL) != 0) {
            // Queue is full，release CPU for Worker
            usleep(100); // <--- Ensure this line
        }
    }
```

If not added, 
- Main encounters Queue is full (`add` returns -2)
- Main releases lock (`unlock`)
- Valgrind switching simulation requires time, but `while` loop in Main is busy waiting. Main release lock and immediately runs the next `while` loop, Main gets the lock agian.
- Result in Starvation in Worker, while Main stucks in `while` loop

