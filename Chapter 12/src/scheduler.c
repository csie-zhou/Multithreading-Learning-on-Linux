#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>

#include "scheduler.h"

/* ── Portable absolute sleep (replaces clock_nanosleep TIMER_ABSTIME) ── */
static void sleep_until(const struct timespec *abs)
{
    struct timespec now, rem;
    clock_gettime(CLOCK_MONOTONIC, &now);
    rem.tv_sec = abs->tv_sec - now.tv_sec;
    rem.tv_nsec = abs->tv_nsec - now.tv_nsec;
    if (rem.tv_nsec < 0)
    {
        rem.tv_sec--;
        rem.tv_nsec += 1000000000L;
    }
    if (rem.tv_sec < 0)
        return; /* already past deadline */
    nanosleep(&rem, NULL);
}

/* ── Globals ─────────────────────────────────────────────────────── */
task_t *g_tasks[MAX_TASKS];
int g_task_count = 0;
volatile int g_scheduler_running = 0;

trace_event_t g_trace[MAX_TRACE_EVENTS];
int g_trace_count = 0;
static struct timespec g_epoch;
static pthread_mutex_t g_trace_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t g_table_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_tick_thread;

/* ── Time Helpers ────────────────────────────────────────────────── */
uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

void task_sleep_ms(uint64_t ms)
{
    struct timespec ts = {
        .tv_sec = (time_t)(ms / 1000),
        .tv_nsec = (long)((ms % 1000) * 1000000L)};
    nanosleep(&ts, NULL);
}

static void timespec_add_ms(struct timespec *ts, uint64_t ms)
{
    ts->tv_nsec += (long)(ms % 1000) * 1000000L;
    ts->tv_sec += (time_t)(ms / 1000) + ts->tv_nsec / 1000000000L;
    ts->tv_nsec %= 1000000000L;
}

static int64_t timespec_diff_us(const struct timespec *a,
                                const struct timespec *b)
{
    return (int64_t)(a->tv_sec - b->tv_sec) * 1000000LL + (int64_t)(a->tv_nsec - b->tv_nsec) / 1000LL;
}

/* ── Task Wrapper ────────────────────────────────────────────────── */
static void *task_wrapper(void *arg)
{
    task_t *t = (task_t *)arg;

    /* Apply POSIX real-time scheduling if running as root */
    struct sched_param sp = {.sched_priority = t->priority % 99 + 1};
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0)
    {
        /* fallback: use nice value proportional to priority */
        setpriority(PRIO_PROCESS, 0, 19 - (t->priority * 19 / 99));
    }

    clock_gettime(CLOCK_MONOTONIC, &t->release_time);
    t->absolute_deadline = t->release_time;
    timespec_add_ms(&t->absolute_deadline, t->deadline_ms ? t->deadline_ms
                                                          : t->period_ms);

    while (g_scheduler_running)
    {
        struct timespec start, finish;
        clock_gettime(CLOCK_MONOTONIC, &start);

        /* measure jitter vs expected release */
        t->last_jitter_us = timespec_diff_us(&start, &t->release_time);

        t->state = TASK_RUNNING;
        t->activations++;

        t->func(t->arg); /* ← user task body */

        clock_gettime(CLOCK_MONOTONIC, &finish);
        t->state = TASK_SLEEPING;

        /* check deadline */
        int missed = (timespec_diff_us(&finish, &t->absolute_deadline) > 0);
        if (missed)
        {
            t->deadline_misses++;
            fprintf(stderr, "[SCHED] !! DEADLINE MISS: task='%s' overrun=%lld us\n",
                    t->name, (long long)timespec_diff_us(&finish, &t->absolute_deadline));
        }

        /* record trace event */
        pthread_mutex_lock(&g_trace_lock);
        if (g_trace_count < MAX_TRACE_EVENTS)
        {
            trace_event_t *ev = &g_trace[g_trace_count++];
            memcpy(ev->task_name, t->name, TASK_NAME_LEN - 1);
            ev->task_name[TASK_NAME_LEN - 1] = '\0';
            ev->task_id = t->id;
            /* Use signed arithmetic to avoid uint64 wraparound on nsec subtraction */
            ev->start_ms = (uint64_t)((int64_t)(start.tv_sec - g_epoch.tv_sec) * 1000LL + (int64_t)(start.tv_nsec - g_epoch.tv_nsec) / 1000000LL);
            ev->end_ms = (uint64_t)((int64_t)(finish.tv_sec - g_epoch.tv_sec) * 1000LL + (int64_t)(finish.tv_nsec - g_epoch.tv_nsec) / 1000000LL);
            ev->deadline_missed = missed;
            ev->deadline_ms = (uint64_t)((int64_t)(t->absolute_deadline.tv_sec - g_epoch.tv_sec) * 1000LL + (int64_t)(t->absolute_deadline.tv_nsec - g_epoch.tv_nsec) / 1000000LL);
        }
        pthread_mutex_unlock(&g_trace_lock);

        if (t->period_ms == 0)
            break; /* aperiodic: run once */

        /* sleep until next period */
        timespec_add_ms(&t->release_time, t->period_ms);
        timespec_add_ms(&t->absolute_deadline, t->period_ms);

        sleep_until(&t->release_time);
    }

    t->state = TASK_DEAD;
    return NULL;
}

/* ── Scheduler Tick (watchdog) ───────────────────────────────────── */
static void *tick_thread(void *arg)
{
    (void)arg;
    while (g_scheduler_running)
    {
        task_sleep_ms(TICK_MS);
        /* future: preemption hooks, load balancing, etc. */
    }
    return NULL;
}

/* ── Public API ──────────────────────────────────────────────────── */
void scheduler_init(void)
{
    memset(g_tasks, 0, sizeof(g_tasks));
    g_task_count = 0;
    g_scheduler_running = 0;
    printf("[SCHED] Initialized (tick=%d ms, max_tasks=%d)\n",
           TICK_MS, MAX_TASKS);
}

task_t *task_create(const char *name, int priority,
                    uint64_t period_ms, uint64_t deadline_ms,
                    uint64_t wcet_ms,
                    void *(*func)(void *), void *arg)
{
    pthread_mutex_lock(&g_table_lock);
    if (g_task_count >= MAX_TASKS)
    {
        pthread_mutex_unlock(&g_table_lock);
        fprintf(stderr, "[SCHED] ERROR: task table full\n");
        return NULL;
    }

    task_t *t = calloc(1, sizeof(task_t));
    if (!t)
    {
        pthread_mutex_unlock(&g_table_lock);
        return NULL;
    }

    strncpy(t->name, name, TASK_NAME_LEN - 1);
    t->id = g_task_count;
    t->priority = priority;
    t->effective_priority = priority;
    t->state = TASK_READY;
    t->period_ms = period_ms;
    t->deadline_ms = deadline_ms ? deadline_ms : period_ms;
    t->wcet_ms = wcet_ms;
    t->func = func;
    t->arg = arg;

    g_tasks[g_task_count++] = t;
    pthread_mutex_unlock(&g_table_lock);

    printf("[SCHED] Task created: %-20s prio=%-3d period=%-6llu ms  deadline=%-6llu ms\n",
           name, priority,
           (unsigned long long)period_ms,
           (unsigned long long)t->deadline_ms);
    return t;
}

void scheduler_start(void)
{
    g_scheduler_running = 1;
    g_trace_count = 0;
    clock_gettime(CLOCK_MONOTONIC, &g_epoch);
    printf("[SCHED] Starting %d task(s)...\n\n", g_task_count);

    pthread_create(&g_tick_thread, NULL, tick_thread, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    for (int i = 0; i < g_task_count; i++)
    {
        task_t *t = g_tasks[i];
        if (pthread_create(&t->thread, &attr, task_wrapper, t) != 0)
        {
            fprintf(stderr, "[SCHED] Failed to start task '%s'\n", t->name);
        }
    }
    pthread_attr_destroy(&attr);
}

void scheduler_stop(void)
{
    g_scheduler_running = 0;
    for (int i = 0; i < g_task_count; i++)
    {
        pthread_join(g_tasks[i]->thread, NULL);
    }
    pthread_join(g_tick_thread, NULL);
    printf("\n[SCHED] All tasks stopped.\n");
}

void scheduler_print_stats(void)
{
    printf("\n╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║                   SCHEDULER STATISTICS                          ║\n");
    printf("╠═══════════════════╦═══════╦═══════════╦══════════════╦══════════╣\n");
    printf("║ Task              ║ Prio  ║ Activations║ DL Misses   ║ Jitter   ║\n");
    printf("╠═══════════════════╬═══════╬═══════════╬══════════════╬══════════╣\n");
    for (int i = 0; i < g_task_count; i++)
    {
        task_t *t = g_tasks[i];
        printf("║ %-17s ║  %-4d ║  %-9llu║  %-12llu║  %+6lld us║\n",
               t->name, t->priority,
               (unsigned long long)t->activations,
               (unsigned long long)t->deadline_misses,
               (long long)t->last_jitter_us);
    }
    printf("╚═══════════════════╩═══════╩═══════════╩══════════════╩══════════╝\n");
}

void trace_dump_csv(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        perror("trace_dump_csv");
        return;
    }
    fprintf(f, "task_name,task_id,start_ms,end_ms,deadline_ms,deadline_missed\n");
    for (int i = 0; i < g_trace_count; i++)
    {
        trace_event_t *ev = &g_trace[i];
        fprintf(f, "%s,%d,%llu,%llu,%llu,%d\n",
                ev->task_name, ev->task_id,
                (unsigned long long)ev->start_ms,
                (unsigned long long)ev->end_ms,
                (unsigned long long)ev->deadline_ms,
                ev->deadline_missed);
    }
    fclose(f);
    printf("[TRACE] Written %d events to '%s'\n", g_trace_count, path);
}