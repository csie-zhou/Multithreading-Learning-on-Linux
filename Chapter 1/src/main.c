#include <stdio.h>
#include "thread_pool.h"

int main() {
    printf("Starting C Thread Pool Demo...\n");

    /* 1. Build Thread Pool: 4 worker threads, Queue size 10 */
    thread_pool_t *pool = thread_pool_create(4, 10);
    
    if (pool == NULL) {
        fprintf(stderr, "Failed to create thread pool\n");
        return 1;
    }

    printf("Thread Pool created successfully!\n");
    printf("Managed Pointers:\n");
    printf(" - Pool Struct: %p\n", (void*)pool);
    printf(" - Threads Array: %p\n", (void*)pool->threads);
    printf(" - Task Queue: %p\n", (void*)pool->queue);

    /* 2. Destroy Thread Pool */
    thread_pool_destroy(pool);
    printf("Thread Pool destroyed. Memory freed.\n");

    return 0;
}
