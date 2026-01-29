# Chapter 7. Linux System Speciality - CPU Affinity
In Embedded System Development, we often concern about **"Cahche Miss"**. Imagine your OS let Thread running between different cores, then L1/L2 Cache in CPU seems useless.  

## Key Tasks
1. Use Linux System Call to simulate CPU Affinity
2. Verify by Linux Command

## Assign thread
In `thread_pool_create`, We are going to assign threads to the specific cores.  This can be done by System Call: `pthread_setaffinity_np`
 
### 1. Add Header
This is a particular function in Linux, we have to define `_GNU_SOURCE` in order to use. 

Add these lines on the top of `src/thread_pool.c`:
```C
#define _GNU_SOURCE // Enalble Linux Extension
#include <sched.h>  // Marcos like: CPU_SET, CPU_ZERO
// ... Origin include ....
```

### 2. Modify loop in create
Target: Let 0th thread assigned to Core 0, 1st thread to Core 1th, ... (Round Robin).  

```C
    /* TODO: Chapter 7. Get the cores of CPU / Add CPU Affinity */
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    for (int i = 0; i < thread_count; i++)
    {
        if (pthread_create(&(pool->threads[i]), NULL, thread_pool_worker, (void *)pool) != 0)
        {
            // Trouble shooting
            return NULL;
        }

        /* Add CPU Affinity */
        /* 7-1: Announce spu_set_t variable (Bitmask) */
        cpu_set_t cpuset;

        /* 7-2: Clean set */
        CPU_ZERO(&cpuset);

        /* 7-3: Define which core to allocate (Round Robin) */
        int target_core = i % num_cores;

        /* 7-4: Add target core into set */
        CPU_SET(target_core, &cpuset);

        /* 7-5: Call Linux Affinity */
        /* Parameters:
         * 1. thread ID
         * 2. size of cpuset
         * 3. pointer to cpuset
         */
        int rc = pthread_setaffinity_np(pool->threads[i], sizeof(cpu_set_t), &cpuset);

        if (rc != 0)
        {
            perror("Failed to set affinity (non-fatal)");
        }
        else
        {
            // printf("Thread %d bound to Core %d\n", i, target_core);
        }
    }
```

## Verify
### 1. Monitor
We don't use printf here. Alternatively, we use Linux command.  
1. Compile: `make`
2. Modify `main.c`: let main run longer (ex. `sleep(10)`) to easily observe
3. Execute: `./c_thread_pool_demo &` (add `&` to run in background)
4. Use `pidof` to find **Process ID**:
```
pidof c_thread_pool_demo
```
(Eg. PID 12345)
5. Observe Command: `top`, for info in Thread  
```
top -H -p 12345
```
- `-H`: Show Threads (Otherwise only show 1 Process)
- `-p`: Assigned PID
6. After `top`, press `f` key to enter Fields Management Interface  
7. Use up/down arrow to move to `P = Last Used Cpu`
8. Press Space and `*` will show in front of `P`. Then press `ESC`
- A `P` will appear in the column, shows every thread in their core.
9. (If you want to modify the `P` column order, right arrow on `P` before `ESC`, then use up/down arrow to move to the order you desired.)

(Tip: add more sleep time before `thread_pool_destroy` in `main` function to have more time to observe the results.)
### 2. Output Explanation
```
Threads:   5 total,   0 running,   5 sleeping,   0 stopped,   0 zombie
```
- `5 total`: shows that there is 1 Main Thread + 4 Worker Threads, which is correct.
```
%Cpu(s):  0.3 us,  0.4 sy,  0.0 ni, 99.2 id
```
- `99.2 id`: Idle, which means the process is waiting, does not stuck in busy waiting wasting CPU. The Condition Variable (`pthread_cond_wait`) works well.
```
  PID  P USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND                                                                                     
10021  4 vscode    20   0   35220   1152   1152 S   0.0   0.0   0:00.01 c_thread_pool_d                                                                             
10026  0 vscode    20   0   35220   1152   1152 S   0.0   0.0   0:00.00 c_thread_pool_d                                                                             
10027  1 vscode    20   0   35220   1152   1152 S   0.0   0.0   0:00.00 c_thread_pool_d                                                                             
10028  2 vscode    20   0   35220   1152   1152 S   0.0   0.0   0:00.00 c_thread_pool_d                                                                             
10029  3 vscode    20   0   35220   1152   1152 S   0.0   0.0   0:00.00 c_thread_pool_d                                                                              3
```
- **PID(Actually TID here, Thread ID)**: all are  Threads.
- **P**: core number. 
- **S (Status)**: `S` refers to Sleeping. This is common, it turns into `R` (Running) when a task is in. Our case it runs immediately.
- **COMMAND**: both are `c_thread_pool_d`, which belongs to the same process.
  
## (Trouble Shooting if needed)
### 1. Core Does not show in order
If your output is like:  
```
[Worker Debug] Thread ID 281473659105696 bound to Core 3
[Worker Debug] Thread ID 281473650651552 bound to Core 1
[Worker Debug] Thread ID 281473642197408 bound to Core 2
[Worker Debug] Thread ID 281473633743264 bound to Core 6
```
Which is expected to be 0, 1, 2, 3. This can be **Race Condition** in printf.  
OS assigns thread in the order, but scheduling differs by each core. (Ex. Core 3 is currently available, so Thread 3 is waken and printf is executed.)  
### 2. Core Number Error
`sysconf` can show how many cores are in your enviornment, if 8 cores than core number can be 0 ~ 7.  
1. Though we use `i % num_cores`, the number of core should be in 0 ~ 3, the **Core 6** may caused by the **VM** or **Docker (VS Code Dev Container)**. Host OS assigns certain core to container.  
2. Another reason may caused by `pthread_setaffinity_np` failure but does not show the error.  
This error does not matter that much in this exercise.  


