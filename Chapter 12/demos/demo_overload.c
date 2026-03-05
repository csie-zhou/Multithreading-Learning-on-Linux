#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "scheduler.h"

/*
 * Demo 3: Deadline Miss Detection Under Overload
 * -----------------------------------------------
 * Intentionally overloads the system by creating a task set where
 * total CPU utilization > 1.0, causing deadline misses.
 *
 * U = 40/100 + 60/150 + 80/200 = 0.40 + 0.40 + 0.40 = 1.20 > 1.0  ✗
 *
 * The scheduler detects and logs each miss with exact overrun time (µs).
 * This demonstrates why utilization analysis matters in RTOS design.
 */

static void burn_cpu_ms(uint64_t ms)
{
    uint64_t end = now_ms() + ms;
    volatile uint64_t x = 0;
    while (now_ms() < end)
        x++;
    (void)x;
}

static void *task_a(void *arg)
{
    task_t *self = (task_t *)arg;
    burn_cpu_ms(self->wcet_ms);
    return NULL;
}

static void *task_b(void *arg)
{
    task_t *self = (task_t *)arg;
    burn_cpu_ms(self->wcet_ms);
    return NULL;
}

static void *task_c(void *arg)
{
    task_t *self = (task_t *)arg;
    burn_cpu_ms(self->wcet_ms);
    return NULL;
}

int main(void)
{
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Demo 3: Deadline Miss Under Overload    ║\n");
    printf("║  CPU Utilization = 1.20 (> 1.0)  FAIL   ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");
    printf("Overloaded task set — expect deadline misses:\n\n");

    scheduler_init();

    task_t *a = task_create("TaskA_40ms", 80, 100, 100, 40, task_a, NULL);
    task_t *b = task_create("TaskB_60ms", 60, 150, 150, 60, task_b, NULL);
    task_t *c = task_create("TaskC_80ms", 40, 200, 200, 80, task_c, NULL);

    a->arg = a;
    b->arg = b;
    c->arg = c;

    printf("Total utilization: 40/100 + 60/150 + 80/200 = 1.20 (OVERLOADED)\n\n");

    scheduler_start();
    sleep(2);
    scheduler_stop();

    scheduler_print_stats();
    trace_dump_csv("trace_overload.csv");

    printf("\n[ANALYSIS] Deadline misses are expected and detected correctly.\n");
    printf("[ANALYSIS] Fix: reduce WCET or increase periods to bring U ≤ ln(2).\n");
    return 0;
}