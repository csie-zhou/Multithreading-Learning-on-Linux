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
