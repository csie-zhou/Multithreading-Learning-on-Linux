#include <stdio.h>
#include <stdlib.h>  // Library for malloc & free
#include "thread_pool.h"

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
