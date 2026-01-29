#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_pool.h"

typedef struct
{
    int operator_id;
    int operand_a;
    int operand_b;
} math_args_t;

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

int main() {
    /* Day 4: Worker Thread */
    printf("Starting Chapter 4: Multi-threading Test...\n");

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
