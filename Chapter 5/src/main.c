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
        while (thread_pool_add(pool, deposit_task_safe, NULL) != 0)
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
