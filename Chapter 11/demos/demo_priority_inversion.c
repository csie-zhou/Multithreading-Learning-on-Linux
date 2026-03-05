#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "scheduler.h"

/*
 * Demo 2: Priority Inversion & Priority Inheritance Protocol
 * -----------------------------------------------------------
 * Classic three-task priority inversion scenario:
 *
 *   HIGH   (prio=80): needs shared resource, blocks on mutex
 *   MEDIUM (prio=50): CPU-bound, no mutex needed — preempts LOW
 *   LOW    (prio=20): holds mutex, gets preempted by MEDIUM
 *
 * WITHOUT PIP: HIGH waits while MEDIUM runs → inversion unbounded.
 * WITH    PIP: LOW is boosted to 80, finishes, releases mutex → HIGH runs.
 *
 * This is the Mars Pathfinder bug (1997), fixed in flight by enabling
 * priority inheritance on VxWorks mutexes.
 */

static rt_mutex_t shared_resource;

/* ── LOW priority task: acquires mutex, does long work ─────────── */
static void *task_low(void *arg)
{
    task_t *self = (task_t *)arg;
    printf("  [%s] acquiring shared_resource...\n", self->name);
    rt_mutex_lock(&shared_resource, self);

    printf("  [%s] GOT mutex — doing slow work (80ms)...\n", self->name);
    task_sleep_ms(80);   /* simulate long critical section */

    printf("  [%s] releasing shared_resource\n", self->name);
    rt_mutex_unlock(&shared_resource, self);
    return NULL;
}

/* ── MEDIUM priority task: CPU hog, no mutex ───────────────────── */
static void *task_medium(void *arg)
{
    task_t *self = (task_t *)arg;
    printf("  [%s] running CPU-bound work (no mutex needed)...\n", self->name);
    /* Without PIP, this preempts LOW and blocks HIGH indefinitely */
    uint64_t end = now_ms() + 60;
    volatile uint64_t x = 0;
    while (now_ms() < end) x++;
    (void)x;
    printf("  [%s] done\n", self->name);
    return NULL;
}

/* ── HIGH priority task: wants mutex, should not wait long ─────── */
static void *task_high(void *arg)
{
    task_t *self = (task_t *)arg;
    task_sleep_ms(20);   /* let LOW acquire mutex first */

    uint64_t t0 = now_ms();
    printf("  [%s] waiting for shared_resource...\n", self->name);
    rt_mutex_lock(&shared_resource, self);

    uint64_t wait = now_ms() - t0;
    printf("  [%s] GOT mutex after %llu ms (PIP kept this bounded)\n",
           self->name, (unsigned long long)wait);
    task_sleep_ms(10);
    rt_mutex_unlock(&shared_resource, self);
    printf("  [%s] done\n", self->name);
    return NULL;
}

int main(void)
{
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Demo 2: Priority Inversion + PIP Fix    ║\n");
    printf("║  (Mars Pathfinder scenario, 1997)        ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    rt_mutex_init(&shared_resource, "shared_resource");
    scheduler_init();

    task_t *low    = task_create("LowTask",    20, 0, 500, 80, task_low,    NULL);
    task_t *medium = task_create("MediumTask", 50, 0, 500, 60, task_medium, NULL);
    task_t *high   = task_create("HighTask",   80, 0, 500, 30, task_high,   NULL);

    low->arg    = low;
    medium->arg = medium;
    high->arg   = high;

    printf("\nWatch PIP boost LowTask priority when HighTask blocks:\n\n");
    scheduler_start();
    sleep(1);
    scheduler_stop();

    scheduler_print_stats();
    return 0;
}
