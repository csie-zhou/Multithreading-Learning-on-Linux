#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define MAX_TASKS        16
#define TASK_NAME_LEN    32
#define TICK_MS          10       /* scheduler tick in milliseconds */

/* ── Task States ─────────────────────────────────────────────────── */
typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

/* ── Task Control Block (TCB) ────────────────────────────────────── */
typedef struct task {
    char            name[TASK_NAME_LEN];
    int             id;
    int             priority;           /* 0 = lowest, 99 = highest  */
    int             effective_priority; /* raised by priority inheritance */
    task_state_t    state;

    uint64_t        period_ms;          /* 0 = aperiodic              */
    uint64_t        deadline_ms;        /* relative deadline (ms)     */
    uint64_t        wcet_ms;            /* worst-case execution time  */

    /* stats */
    uint64_t        deadline_misses;
    uint64_t        activations;
    int64_t         last_jitter_us;     /* signed: negative = early   */

    struct timespec release_time;       /* absolute release time      */
    struct timespec absolute_deadline;

    void           *(*func)(void *);
    void           *arg;
    pthread_t       thread;
} task_t;

/* ── Mutex with Priority Inheritance ─────────────────────────────── */
typedef struct rt_mutex {
    pthread_mutex_t  lock;
    pthread_cond_t   cond;
    task_t          *owner;
    int              waiters;
    const char      *name;
} rt_mutex_t;

/* ── Scheduler API ───────────────────────────────────────────────── */
void     scheduler_init(void);
task_t  *task_create(const char *name, int priority,
                     uint64_t period_ms, uint64_t deadline_ms,
                     uint64_t wcet_ms,
                     void *(*func)(void *), void *arg);
void     scheduler_start(void);
void     scheduler_stop(void);
void     scheduler_print_stats(void);

/* rt_mutex API */
void     rt_mutex_init(rt_mutex_t *m, const char *name);
void     rt_mutex_lock(rt_mutex_t *m, task_t *caller);
void     rt_mutex_unlock(rt_mutex_t *m, task_t *caller);

/* helpers for tasks */
void     task_sleep_ms(uint64_t ms);
uint64_t now_ms(void);
uint64_t now_us(void);

/* global task table (read-only outside scheduler) */
extern task_t  *g_tasks[MAX_TASKS];
extern int      g_task_count;
extern volatile int g_scheduler_running;

#endif /* SCHEDULER_H */
