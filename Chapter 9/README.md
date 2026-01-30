# Chapter 9. Zero-Copy I/O (mmap)
In Driver Development (ex. Camera/GPU) and High Performance Cluster (HPC) Server (ex. Nginx/Kafka), **Zero-Copy** and **Memory Mapped I/O (mmap)** can be used to enhance performance.  

Concept:  
- `read/write`: Slow. A Read File contains 2 large amount of data trasfer (`read` + `malloc`).
  1. **Disk -> Kernel Buffer**: Read data from Disk to OS Kernel Buffer
  2. **Kernel Buffer -> User Buffer**: CPU **copy** the data from Kernel to `malloc` memory.  
- `mmap`: Projection of the file onto memory.
  1. **No User-Kernel Copy**: Pointer points directly to Kernel Page Cache
  2. **Operate File as Array**: `ptr[i] = 'A'` is writing into file directly
  3. **Suitable for Large Files**: ex. 4K video or SQL Sever Log

## Key Tasks
1. Simulate a Multithreading Encrypt Engine.

## Simulate a Multithreading Encrypt Engine
We simulate a File Encrypter. Steps:  
1. Build a sever file with **64MB**.
2. Use `mmap` to map to memory.
3. Divide 64MB into 4 equal parts.
4. Place the 4 tasks into Thread Pool, let Worker **Parallel Computing** these 4 blocks (XOR computation to simulate encrypt).
5. Beacuse of using `mmap`, Worker modifies to memory, the data is also modified in the file. Don't need to `write` or `free`.

### 1. Modify `src/main.c`
There are lots of Linux System Call here. Copy paste this to `src/main.c`:  
```C
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>      // open
#include <sys/mman.h>   // mmap, munmap
#include <sys/stat.h>   // fstat
#include <string.h>     // memset
#include <sys/time.h>   // for timer
#include "thread_pool.h"

#define FILE_SIZE (64 * 1024 * 1024) // 64 MB
#define FILENAME "test_data.bin"

/* Define a task Struct：Handle the address in memory */
typedef struct {
    unsigned char *start_ptr;
    size_t length; // unsigned long (under 64 bit)
    int chunk_id;
} crypto_args_t;

/* --- Worker Task：XOR encrypt computing --- */
void encrypt_task(void *arg) {
    crypto_args_t *args = (crypto_args_t *)arg;
    
    // Simple XOR encrypt (Do XOR 0xAA to every byte)
    // This is CPU-bound operation
    for (size_t i = 0; i < args->length; i++) {
        args->start_ptr[i] ^= 0xAA; 
    }

    printf("  [Worker] Chunk %d encrypted (%lu bytes)\n", 
           args->chunk_id, args->length);
    
    free(args); // Remember to free "args" struct
}

/* Secondary Function：To Compute Time Difference (ms) */
long long current_timestamp() {
    struct timeval te; 
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

int main() {
    printf("Starting Chapter 9: Zero-Copy Parallel Encryption (mmap)...\n");

    /* 1. Prepare 64MB space for file (should be empty) */
    int fd = open(FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); return 1; } 

    // Extend the file size to 64MB (file is empty now)
    // ftruncate: set size to FILE_SIZE and truncate the rest space
    if (ftruncate(fd, FILE_SIZE) != 0) { perror("ftruncate"); return 1; } 
    
    printf("[Main] Created 64MB file: %s\n", FILENAME);

    /* 2. KEY!!!：mmap mapping */
    /* * PROT_READ | PROT_WRITE: Can Read and Write
     * MAP_SHARED: Modify will write back (Optional: MAP_PRIVATE, modify only in memory)
     */
    unsigned char *map_ptr = mmap(NULL, FILE_SIZE, 
                                  PROT_READ | PROT_WRITE, 
                                  MAP_SHARED, fd, 0);
    
    if (map_ptr == MAP_FAILED) { perror("mmap"); return 1; }
    
    // For Demonstration, we fill file with 'A' (0x41)
    // Notice：We use MAP_SHARED, all modifications in memory = in file
    printf("[Main] Initializing memory with 'A'...\n");
    memset(map_ptr, 'A', FILE_SIZE);

    /* 3. Activate Thread Pool */
    int num_threads = 4;
    thread_pool_t *pool = thread_pool_create(num_threads, 10);
    if (!pool) return 1;

    long long start_time = current_timestamp();

    /* 4. Chunk the tasks and distribute to threads */
    size_t chunk_size = FILE_SIZE / num_threads;
    
    for (int i = 0; i < num_threads; i++) {
        crypto_args_t *args = (crypto_args_t *)malloc(sizeof(crypto_args_t));
        
        // Zero-Copy Essense：We only pass pointer (Pointer Arithmetic)
        // No malloc huge memory，no memcpy
        args->start_ptr = map_ptr + (i * chunk_size);
        args->length = chunk_size;
        args->chunk_id = i;

        thread_pool_add(pool, encrypt_task, args); // args will be free when thread execute the task
    }

    /* 5. Wait till done (join in destroy function) */
    sleep(5); // Avoid main runs too quickly
    thread_pool_destroy(pool);

    long long end_time = current_timestamp();
    printf("[Main] Encryption finished in %lld ms.\n", end_time - start_time);

    /* 6. Verify the result */
    // 'A' (0x41) XOR 0xAA should equal 0xEB
    if (map_ptr[0] == 0xEB && map_ptr[FILE_SIZE - 1] == 0xEB) {
        printf("[Result] Verification SUCCESS! Data modified in place.\n");
    } else {
        printf("[Result] Verification FAILED! Expected 0xEB, got 0x%02X\n", map_ptr[0]);
    }

    /* 7. Clean resiurce */
    // msync(map_ptr, FILE_SIZE, MS_SYNC); // Optional：Force to write back to disk (syn)
    munmap(map_ptr, FILE_SIZE); // Unmap
    close(fd);
    
    // Delete test file (If you would to to observe hex, deannotate this line)
    unlink(FILENAME); 

    return 0;
}
```
In main:
1. `int fd = open(FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0644);`:
    - `fd`: File Descriptor (0: Input, 1: Output, 2: Error, so new open files mostly 3, 4, 5...)
    - `O_RDWR`: Read/Write
    - `O_CREAT`: If the file doesn't exist, then create
    - `O_TRUNC`: (KEY!!!) If the file already exists, then clear it to size 0.
    - `0644`: Permission
      - `6` (Owner): Read + Write
      - `4` (Group): Read only
      - `4` (Others): Read only
     
### 2. Compile and Execute
1. Compile: `make`
2. Execute: `./c_thread_pool_demo`
3. If you use `top`, you can find out that Process **RES (Resident Memory)** won't increase to 64 MB. It's because Linux using Page Fault Mechanism, which `mmap` is efficient. Once Page Fault, OS reads another page, don't need to upload the whole file.

## Pro Tip
If `mmap` is that good, why we are still using `read`, `write`?  

`mmap` still has disadvantages:
1. **Page Fault Overhead**: If the file is huge and Random Access, then the overhead of Page Fault does not bring the efficency.
2. **TLB thrashing**: Extreme operations of mapping cause CPU TLB thrashing (too much page fault cause too much time switching).

So `mmap` is optimal for **large files** and **frequently access the same block**. Otherwise, `read` may be better.  

