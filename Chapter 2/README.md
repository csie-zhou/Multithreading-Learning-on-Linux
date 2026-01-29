# Chapter 2. Implement Circular Buffer
## Key Tasks
Make sure the tasks in the queue is circular.  
1. **Add:** add task into `tail` position, `tail` move forward  
2. **Pop:** consume a task from `head`, `head` move forward  

### 1. Add tasks (`thread_pool_add`)
Three things to focus:
1. **Locking:** multithreads consuming the tasks in task queue, we must lock (`pthread_mutex_lock`)  
2. **Full Check:** if queue is full, return fail. (In Chapter 6, we will use **Condition Variable** for waiting)  
3. **Wrap Around:** if `tail` reaches the end of queue, return to 0. `(tail + 1) % size`.  

Add this in `src/thread_pool.c` (before `thread_pool_destroy`):
```C
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
```
### 2. Add test API for Chapter 2
In Chapter 4, we will learn how to let Worker Threads handle tasks. Now we need a "manually execute" function to test the Ring Buffer logic.  

Add this function at the bottom of `src/thread_pool.c`.  
```C
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
```
Also add this line in `include/thread_pool.h`, so that `main` can call.
```C
/* Under thread_pool_destroy */
int thread_pool_run_pending_task(thread_pool_t *pool);
```

### 3. Modify Main
We aim to test the logic works for Ring Buffer. Set Queue size to 3.
1. Add 1 (OK)  
2. Add 2 (OK)  
3. Add 3 (OK)  
4. Add 4 (Should Fail - Full)
5. Run 1 (Pop, Head move forward)
6. Add 5 (Should Success - Because Head moved，Tail can move forward，that's Ring Buffer!)  

Open `src/main.c` and paste this:
```C
#include <stdio.h>
#include <unistd.h>
#include "thread_pool.h"

/* Define a dummy function for task */
void dummy_task(void *arg) {
    int id = *(int*)arg;
    printf("  [Task executing] Processing Item %d\n", id);
}

int main() {
    printf("Starting Chapter 2: Ring Buffer Logic Test...\n");

    /* Build a small Pool，Queue size is 3 */
    thread_pool_t *pool = thread_pool_create(4, 3);
    if (!pool) return 1;

    int ids[] = {1, 2, 3, 4, 5};

    /* Test 1: Fill the Queue */
    printf("\n--- Test 1: Filling the Queue (Size 3) ---\n");
    for (int i = 0; i < 3; i++) {
        int res = thread_pool_add(pool, dummy_task, &ids[i]);
        printf("Adding Task %d... Result: %d (0=Success)\n", ids[i], res);
    }

    /* Test 2: Add the 4th into queue (should fail) */
    printf("\n--- Test 2: Overfilling ---\n");
    int res = thread_pool_add(pool, dummy_task, &ids[3]);
    printf("Adding Task 4 (Should Fail)... Result: %d\n", res);
    if (res == -2) printf("  -> Pass: Queue correctly reported full.\n");
    else printf("  -> Fail: Queue should be full!\n");

    /* Test 3: Consume a task (to create space) */
    printf("\n--- Test 3: Consuming one task ---\n");
    thread_pool_run_pending_task(pool);
    printf("  -> Consumed task at head. Queue count should be 2 now.\n");

    /* Test 4: Add 1 task (Should be success) */
    printf("\n--- Test 4: Wrapping Around ---\n");
    res = thread_pool_add(pool, dummy_task, &ids[4]);
    printf("Adding Task 5 (Should Success)... Result: %d\n", res);
    if (res == 0) printf("  -> Pass: Ring Buffer wrap-around works!\n");
    else printf("  -> Fail: Could not add task even after space was freed.\n");

    /* Clean all tasks left */
    printf("\n--- Cleaning up ---\n");
    while (thread_pool_run_pending_task(pool) == 0);

    thread_pool_destroy(pool);
    return 0;
}
```

### 4. Execute and Test
1. Compile
```
make
```
2. Execute
```
./c_thread_pool_demo
```

## Pro Tip
Our Ring Buffer uses `%` (Module) to wrap around, but in some low-level embedded system, `/` and `%` is slow. To speed up, we can define the Queue size to **Power of 2**, then we can use **Bitwise AND (`&`)** to replace `%`. 
