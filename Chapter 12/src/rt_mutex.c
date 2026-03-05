#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler.h"

/*
 * rt_mutex — Mutex with Priority Inheritance Protocol (PIP)
 *
 * When a high-priority task (H) blocks on a mutex held by a low-priority
 * task (L), PIP temporarily boosts L's priority to H's level so that
 * medium-priority tasks cannot preempt L and cause unbounded blocking.
 *
 * This is the exact bug that crashed the Mars Pathfinder in 1997.
 */

void rt_mutex_init(rt_mutex_t *m, const char *name)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&m->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    pthread_cond_init(&m->cond, NULL);
    m->owner = NULL;
    m->waiters = 0;
    m->name = name;
}

void rt_mutex_lock(rt_mutex_t *m, task_t *caller)
{
    pthread_mutex_lock(&m->lock);

    while (m->owner != NULL && m->owner != caller)
    {
        /* ── Priority Inheritance ── */
        if (caller->effective_priority > m->owner->effective_priority)
        {
            printf("[PIP]  '%s' (prio=%d) boosts '%s' (prio=%d→%d) holding '%s'\n",
                   caller->name, caller->effective_priority,
                   m->owner->name, m->owner->effective_priority,
                   caller->effective_priority,
                   m->name);
            m->owner->effective_priority = caller->effective_priority;
        }

        m->waiters++;
        caller->state = TASK_BLOCKED;
        pthread_cond_wait(&m->cond, &m->lock);
        caller->state = TASK_RUNNING;
        m->waiters--;
    }

    m->owner = caller;
    pthread_mutex_unlock(&m->lock);
}

void rt_mutex_unlock(rt_mutex_t *m, task_t *caller)
{
    pthread_mutex_lock(&m->lock);

    if (m->owner != caller)
    {
        fprintf(stderr, "[PIP]  ERROR: '%s' tried to unlock mutex it doesn't own!\n",
                caller->name);
        pthread_mutex_unlock(&m->lock);
        return;
    }

    /* Restore original priority */
    if (caller->effective_priority != caller->priority)
    {
        printf("[PIP]  '%s' priority restored (%d→%d) after releasing '%s'\n",
               caller->name, caller->effective_priority,
               caller->priority, m->name);
        caller->effective_priority = caller->priority;
    }

    m->owner = NULL;
    pthread_cond_broadcast(&m->cond); /* wake all waiters, highest wins */
    pthread_mutex_unlock(&m->lock);
}