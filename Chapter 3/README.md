# Chapter 3. Generic Args & Memory Safety
Thread Pool in C, we are using the task functin with: `void (*function)(void *arg)`  

Issues:
1. If we need to pass multiple variables like `int a`, `float b`, `char *name`, but we only have `void *` as the function argument  
2. If we pass **Stack Variable**. When main function ends but the worker thread hasn't execute, the pointer points to **Garbage**, the system breaks down.

## Key Tasks
In Chapter 2, we wrote `thread_pool_add(..., &ids[i]);`. This is safe in `main`, because `main` exists permanantly. But in realistic:
1. Function A announce variable `x` on Stack.
2. Function A passes `&x` to Thread Pool, and Function A returns.
3. Function A Stack Frame is destroyed/rewritten.
4. A Worker in Thread Pool wakes up, read `&x`.
5. **!!! SegFault !!!**  

==> Solution: variables should be allocated on **Heap** by `malloc` until task done, then `free` by the task.  
(Heap is a shared resource in Multithreading, while stack isn't)  
### 1. Define a structure with complicate variables
Add this at the top of `src/main.c`:
```C
typedef struct {
    int operator_id;
    int operand_a;
    int operand_b;
} math_args_t;
```
### 2. Write a function to Free task
Continue to modify in `src/main.c`. We are writing a new function `heavy_calculation`. **Note:** This function is responsible for releasing `arg` after finishing task (**Ownership Transfer**).  
```C
void heavy_calculation(void *arg)
{
    /* 1. Let void* be the desired struct pointer */
    math_args_t *args = (math_args_t *)arg;

    /* 2. Do the calculation */
    int result = args->operand_a + args->operand_b;

    printf("  [Worker] Task %d: %d + %d = %d\n", args->operator_id, args->operand_a, args->operand_b, result);

    /* 3. IMPORTANT: free memory */
    /* Cause we get variables from malloc, need to free after used */
    free(args);
}
```

### 3. Add Header File
Add this header file in `src/main.c`:
```C
#include <stdlib.h>  // Library for malloc & free
```

### 4. Implement Haep Allocation
Modify the main fuction in `src/main.c` for this chapter:
```C
int main() {
    printf("Starting Day 3: Memory Safety Test...\n");

    thread_pool_t *pool = thread_pool_create(4, 10);
    if (!pool) return 1;

    for (int i = 0; i < 5; i++) {
        /* 1. Allocate on Heap Mem. (KEY!!!) */
        math_args_t *args = (math_args_t *)(malloc(sizeof(math_args_t)));

        /* Check if malloc fails */
        if (args == NULL)
        {
            perror("Failed to allocate args.");
            continue;
        }

        /* 2. Fill the parameters */
        args->operator_id = i;
        args->operand_a = i * 10;
        args->operand_b = i * 2;

        /* 3. Send mission */
        printf("Adding Task %d to thread pool...\n", i);
        int res = thread_pool_add(pool, heavy_calculation, (void *)args); //

        /* Trouble shooting: If queue is full, */
        if (res != 0)
        {
            fprintf(stderr, "Failed to add task %d\n", i);
            free(args);
        }
    }

    /* Manually execute all tasks (Don't have worker thread yet) */
    printf("\n--- Processing Tasks ---\n");
    while (thread_pool_run_pending_task(pool) == 0) {
        // Keep running until queue is empty
    }

    thread_pool_destroy(pool);
    
    /* TODO: Chapter 8. Check Memory Leak using valgrind */
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
```
Adding Task 0 to pool...
Adding Task 1 to pool...
...
--- Processing Tasks ---
  [Worker] Task 0: 0 + 0 = 0
  [Worker] Task 1: 10 + 2 = 12
  [Worker] Task 2: 20 + 4 = 24
...
```

 
