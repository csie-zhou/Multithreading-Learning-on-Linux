#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "scheduler.h"

/*
 * Demo 1: Periodic Task Scheduling
 * ---------------------------------
 * Three tasks with different periods and priorities run concurrently.
 * The scheduler enforces SCHED_FIFO priorities (when root) and tracks
 * deadline misses and activation jitter using CLOCK_MONOTONIC.
 *
 * Task set satisfies Rate Monotonic (RM) schedulability:
 *   U = 20/100 + 30/200 + 50/500 = 0.55 < ln(2) ≈ 0.693  ✓
 */

/* Simulate different workload sizes */
static void burn_cpu_ms(uint64_t ms)
{
    uint64_t end = now_ms() + ms;
    volatile uint64_t x = 0;
    while (now_ms() < end)
        x++;
    (void)x;
}

static void *task_sensor(void *arg)
{
    task_t *self = (task_t *)arg;
    printf("  [%s] sampling sensor (wcet=%llu ms)\n",
           self->name, (unsigned long long)self->wcet_ms);
    burn_cpu_ms(self->wcet_ms);
    return NULL;
}

static void *task_control(void *arg)
{
    task_t *self = (task_t *)arg;
    printf("  [%s] running PID loop (wcet=%llu ms)\n",
           self->name, (unsigned long long)self->wcet_ms);
    burn_cpu_ms(self->wcet_ms);
    return NULL;
}

static void *task_logger(void *arg)
{
    task_t *self = (task_t *)arg;
    printf("  [%s] flushing log buffer (wcet=%llu ms)\n",
           self->name, (unsigned long long)self->wcet_ms);
    burn_cpu_ms(self->wcet_ms);
    return NULL;
}

int main(void)
{
    printf("╔══════════════════════════════════════════╗\n");
    printf("║  Demo 1: Periodic Task Scheduling        ║\n");
    printf("║  Rate Monotonic — Utilization = 0.55     ║\n");
    printf("╚══════════════════════════════════════════╝\n\n");

    scheduler_init();

    task_t *sensor = task_create("SensorTask", 80, 100, 100, 20, task_sensor, NULL);
    task_t *control = task_create("ControlTask", 60, 200, 200, 30, task_control, NULL);
    task_t *logger = task_create("LoggerTask", 40, 500, 500, 50, task_logger, NULL);

    /* pass self-reference as arg */
    sensor->arg = sensor;
    control->arg = control;
    logger->arg = logger;

    printf("\nRunning for 2 seconds...\n\n");
    scheduler_start();
    sleep(2);
    scheduler_stop();

    scheduler_print_stats();
    trace_dump_csv("trace_periodic.csv");
    return 0;
}