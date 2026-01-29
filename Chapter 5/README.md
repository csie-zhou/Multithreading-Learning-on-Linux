# Chapter 5. Race Condition and Deadlock
In Chapter 4, we have 4 threads accessing the same variable `count++`, which is unsafe.  

In Assembly perspection, `count++` requires 3 steps: **LOAD**, **ADD** and **STORE**. If two or more threads accessing the same data, may cause Race Condition.  
## Key Tasks
1. Write an Unsafe task to verify errors caused by Race Condition.
2. Differentinate the "Pool Lock" and the "User Lock"

## Race Condition
### 1. Write an Unsafe Code
We simulate depositing to the bank. We create 10,000 tasks, each task deposits $1. The final balance should be 10,000.  

Copy and paste this in `src/main.c`:  
```C
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_pool.h"

/* --- Global variable (eg. bank account) --- */
int g_balance = 0;

/* --- An Unsafe task with no locks --- */
void deposit_task(void *arg)
{
    (void)arg; // Ignore arguments

    /* Make a Race Condition */
    /* Read -> Delay(for context switch) -> Write */
    int local_balance = g_balance;

    // usleep(1); // If no race condition, open this line
    local_balance += 1;
    g_balance = local_balance;
}

int main() {
    /* Chapter 5. Race Condition */
    printf("Starting Chapter 5: Race Condition Stress Test...\n");

    /* 1. Build Pool */
    thread_pool_t *pool = thread_pool_create(4, 100); // Large Queue
    if (!pool)
        return 1;

    /* 2. Send 10,000 tasks */
    int task_count = 10000;
    printf("Depositing $1 for %d times...\n", task_count);

    for (int i = 0; i < task_count; i++)
    {
        /* If Queue is full, there will be add error. Use while retry */
        while (thread_pool_add(pool, deposit_task, NULL) != 0)
        {
            usleep(100);
        }
    }

    /* 3. Wait for all tasks done */
    /* We use pool request here, should use atomic counter instead */
    while (1)
    {
        pthread_mutex_lock(&(pool->lock));
        int pending = pool->count;
        pthread_mutex_unlock(&(pool->lock));

        if (pending == 0)
            break;
        usleep(1000);
    }

    sleep(60);

    thread_pool_destroy(pool);

    /* 4. See the result */
    printf("--------------------------------------\n");
    printf("Expected Balance: %d\n", task_count);
    printf("Actual Balance:   %d\n", g_balance);

    if (g_balance == task_count)
    {
        printf("Result: PERFECT match! (Lucky?)\n");
    }
    else
    {
        printf("Result: RACE CONDITION DETECTED! (Lost %d)\n", task_count - g_balance);
    }
    printf("--------------------------------------\n");

    return 0;
}
```
#### Execute and Test
```
make
./c_thread_pool_demo
```
1. Expectations: `Actual Balance` should be less than 10,000 (eg. 9982). This proves that the Queue or Thread Pool is Safe, but the user data `g_balance` is unsafe.

### 2. Introduce User-Level Lock
`pool->lock` only protects the queue in Thread Pool, but the user data is not protected.

==> Solution: If user will modify **shared variable**, prepare a **new lock** for it.  

Modify `src/main.c` again, add a lock for `g_balance`:
```C
/* --- Global variable (eg. bank account) --- */
int g_balance = 0;
pthread_mutex_t g_balance_lock = PTHREAD_MUTEX_INITIALIZER; // Add this lock to ensure no race condition

/* --- Safe task with lock --- */
void deposit_task_safe(void *arg)
{
    (void)arg; // Ignore arguments

    /* No Race Condition */
    pthread_mutex_lock(&(g_balance_lock));
    g_balance++;
    pthread_mutex_unlock(&(g_balance_lock));
}
```
And modify the `deposit_task` to `deposit_task_safe` in `main`. Re-execute and test, should get **PERFECT match**.

## Deadlock
**Deadlock** may caused by the Worker Threads not releasing the resource while main function terminates. Check out the incorrect version of `thread_pool_destroy` that may cause deadlock:
```C
/* --- Incorrect Version --- */
int thread_pool_destroy(thread_pool_t *pool) {
    pthread_mutex_lock(&(pool->lock)); // 1. Get the Lock
    
    pool->shutdown = 1;
    pthread_cond_broadcast(&(pool->notify));

    /* Wait for Workers end (join) */
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL); // 2. Hold the Lock while waiting other threads to join
    }

    pthread_mutex_unlock(&(pool->lock)); // 3. Holding Lock until all threads join, then release.
    // ...
}
```
Above code causes deadlock due to:
1. **Main Thread** holds the `lock`, waiting in `pthread_join` till all Worker Threads join.
2. **Worker Threads** awaken by `broadcast` from the status `wait`, trying to take the lock by `pthread_mutex_lock`. But the lock holds by Main Thread.
3. Main is waiting Worker to join, Worker is waiting Main to release lock ==> **Circular Waiting** ==> **Deadlock**

Ensure `pthread_join` runs **after** `pthread_mutex_unlock`:
```C
    /* --- Correct Version (Order Matters) --- */
    pthread_mutex_lock(&(pool->lock));
    pool->shutdown = 1;
    pthread_cond_broadcast(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock)); // <--- KEY!!! Release the Lock, then the resource

    /* Wait till all threads join without holding Lock */
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
```
