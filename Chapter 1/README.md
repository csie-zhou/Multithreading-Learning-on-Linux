# Chapter 1. Achitecture and Memory Management
## Key Tasks
1. Define `thread_pool_t` Struct Layout  
2. Implement `create` and `destroy`, correctly handle `malloc` failure condition  
3. Write `Makefile` (Not using CMake but gcc)  
## Achitecture Build
### 1. Build project directory structure
Under Linux Enviornment (in `c_thread_pool`)  
```bash
mkdir src include tests
touch Makefile src/main.c src/thread_pool.c include/thread_pool.h
```
Should look like
```
Makefile
include/
    thread_pool.h
src/
    main.c
    thread_pool.c
```
### 2. Define Header File
Open `include/thread_pool.h`. We have to define 2 kernel structure: `task` and `pool`.  
(User can know how much the memory is needed by good definition of Struct)

```C
#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

/*  1. Define thread task structure
    function: a func pointer, points to the task
    argument: arguments passing to function (void*)
 */
typedef struct {
    void (*function)(void *);
    void *argument;
} thread_task_t;

/*  2. Define thread pool structure
    With Sync (Lock/Cond), Ring Buffer(Task Queue) and array of threads
*/
typedef struct
{
    pthread_mutex_t lock;  // Mutex Lock of Queue
    pthread_cond_t notify; // Conditional Variable of worker thread
    pthread_t *threads;    // Array of thread ID (Dynamic allocate)
    thread_task_t *queue;  // Task Queue
    int thread_count;      // Numbers of threads
    int queue_size;        // Size of Queue
    int head;              // Ring Buffer Head (taking next task)
    int tail;              // Ring Buffer Tail (adding a task)
    int count;             // Number of threads
    int shutdown;          // Flag (0: operate, 1: shutdown)
} thread_pool_t;

/* API Declaration */
thread_pool_t* thread_pool_create(int thread_count, int queue_size);
int thread_pool_destroy(thread_pool_t *pool);

#endif // THREAD_POOL_H
```

### 3. Memory allocation implemnetation
Open `src/thread_pool.c`. 
(Use `goto` for ERROR Handling. In Linux Kernel or Driver, using `goto` can avoid duplicate `free`)
```C
#include "thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

thread_pool_t* thread_pool_create(int thread_count, int queue_size) {
    thread_pool_t *pool;

    if (thread_count <= 0 || queue_size <= 0) {
        return NULL;
    }

    /* 1. Allocate thread pool */
    pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
    if (pool == NULL) {
        perror("Failed to allocate thread pool"); // 印出系統錯誤訊息
        return NULL;
    }

    /* 2. Initialize variables */
    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = 0;

    /* 3. Allocate Arrays (Threads & Queue) */
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    pool->queue = (thread_task_t *)malloc(sizeof(thread_task_t) * queue_size);

    /* (Check memory after every malloc) */
    if (pool->threads == NULL || pool->queue == NULL) {
        perror("Failed to allocate threads or queue");
        goto err_cleanup; // Use goto to error handling
    }

    /* 4. Initialize Lock & Conditional Variable */
    if (pthread_mutex_init(&(pool->lock), NULL) != 0 ||
        pthread_cond_init(&(pool->notify), NULL) != 0) {
        perror("Failed to init mutex or cond");
        goto err_cleanup;
    }

    /* * TODO: Chapter 4. Call pthread_create to activate worker thread */

    pool->thread_count = thread_count;

    return pool;

err_cleanup:
    /* Release resource */
    if (pool->threads) free(pool->threads);
    if (pool->queue) free(pool->queue);
    free(pool);
    return NULL;
}

int thread_pool_destroy(thread_pool_t *pool) {
    if (pool == NULL) return -1;

    /* Release resource (Order Matters!!!) */
    /* From pthread.h library */
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));

    /* Free Memory */
    free(pool->queue);
    free(pool->threads);
    free(pool);
    
    return 0;
}
```
### 4. Main.c
Open `src/main.c`. Ensure `create` and `destroy` won't cause **Memory Leak**.
```C
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
```

### 5. Makefile (Build System)
```Makefile
CC = gcc
# -Wall: Open warning, -g: including error infos (for Valgrind/GDB), -pthread: support POSIX Threads
CFLAGS = -Wall -Wextra -g -pthread -Iinclude

# Target files
TARGET = c_thread_pool_demo
SRC = src/main.c src/thread_pool.c
OBJS = src/main.o src/thread_pool.o

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compiling
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

```

### 6. Execute and Test
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
Starting C Thread Pool Demo...
Thread Pool created successfully!
Managed Pointers:
 - Pool Struct: 0x... (Memory Address)
 - Threads Array: 0x...
 - Task Queue: 0x...
Thread Pool destroyed. Memory freed.
```
