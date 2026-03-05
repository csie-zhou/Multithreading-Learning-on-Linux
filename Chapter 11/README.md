# RT-Scheduler — Real-Time Task Scheduler on Linux

A preemptive, fixed-priority real-time task scheduler implemented in C using POSIX threads and `CLOCK_MONOTONIC`. Demonstrates core RTOS concepts — periodic task scheduling, deadline monitoring, and the Priority Inheritance Protocol (PIP) — entirely in software on Linux/macOS (via Docker).

---

## Motivation

Real-time systems are defined not just by *what* they compute, but *when*. Missing a deadline in a flight controller or antilock brake system is a failure regardless of correctness. This project implements the scheduling infrastructure that underpins such systems and demonstrates two critical failure modes: **priority inversion** and **CPU overload**.

The priority inversion scenario directly mirrors the **Mars Pathfinder bug (1997)**, where a low-priority task holding a mutex was preempted by medium-priority tasks, starving the high-priority communications task and causing repeated system resets — resolved in flight by enabling priority inheritance on VxWorks.

---

## Architecture

```
rt-scheduler/
├── include/
│   └── scheduler.h          # Task Control Block, rt_mutex, public API
├── src/
│   ├── scheduler.c          # Preemptive scheduler, SCHED_FIFO, deadline tracking
│   └── rt_mutex.c           # Mutex with Priority Inheritance Protocol (PIP)
├── demos/
│   ├── demo_periodic.c      # Rate Monotonic task set, jitter measurement
│   ├── demo_priority_inversion.c  # PIP demo (Mars Pathfinder scenario)
│   └── demo_overload.c      # Deadline miss detection under overloaded U > 1
└── Makefile
```

### Task Control Block (TCB)

Each task is described by:

| Field | Description |
|---|---|
| `priority` | Fixed priority (0–99) |
| `effective_priority` | Runtime priority (raised by PIP) |
| `period_ms` | Activation period |
| `deadline_ms` | Relative deadline |
| `wcet_ms` | Worst-Case Execution Time |
| `deadline_misses` | Cumulative miss counter |
| `last_jitter_us` | Signed activation jitter (µs) vs expected release |

---

## Demos

### Demo 1 — Periodic Scheduling (Rate Monotonic)

Three periodic tasks with priorities assigned by Rate Monotonic Analysis (RMA): shorter period → higher priority. The task set is schedulable: **U = 0.20 + 0.15 + 0.10 = 0.55 < ln(2) ≈ 0.693**.

```
SensorTask   prio=80  period=100ms  wcet=20ms
ControlTask  prio=60  period=200ms  wcet=30ms
LoggerTask   prio=40  period=500ms  wcet=50ms
```

Jitter is measured per-activation using `CLOCK_MONOTONIC` and reported in the stats table.

```bash
make run_periodic
```

---

### Demo 2 — Priority Inversion & PIP Fix

Classic three-task inversion scenario. Without mitigation, `MediumTask` can preempt `LowTask` while `LowTask` holds a mutex that `HighTask` needs — blocking `HighTask` for an unbounded duration despite being the highest-priority task.

**The Priority Inheritance Protocol** temporarily boosts `LowTask`'s priority to match `HighTask`'s, preventing `MediumTask` from interposing.

```
LowTask    prio=20  acquires mutex, runs long critical section
MediumTask prio=50  CPU-bound, no mutex
HighTask   prio=80  needs mutex → blocks → triggers PIP boost
```

Expected output:
```
[PIP] 'HighTask' (prio=80) boosts 'LowTask' (prio=20→80) holding 'shared_resource'
[PIP] 'LowTask' priority restored (80→20) after releasing 'shared_resource'
```

```bash
make run_pip
```

---

### Demo 3 — Deadline Miss Detection Under Overload

Intentionally overloaded task set with **U = 1.20 > 1.0**, guaranteed to produce deadline misses. The scheduler detects each miss with microsecond precision and logs the exact overrun.

```bash
make run_overload
```

This demo also serves as a correctness test: if the deadline monitor reports zero misses on an overloaded system, the timing logic is wrong.

---

## Build & Run

### Requirements

- GCC, Make, pthreads (`libpthread`), `librt`
- Linux (or macOS via Docker — see below)

### Native (Linux)

```bash
git clone https://github.com/YOUR_USERNAME/rt-scheduler
cd rt-scheduler
make all
make run_periodic
make run_pip
make run_overload
```

For SCHED_FIFO (true real-time priorities):
```bash
sudo ./demo_periodic
```

### macOS via Docker (Dev Container)

```bash
# Install Docker + VS Code Dev Containers extension
# Open repo in VS Code → "Reopen in Container" → Ubuntu
make all
```

---

## Key Implementation Details

**Scheduling**: Each task runs in a `pthread` with `SCHED_FIFO` scheduling (when root) or `setpriority()`-based nice values (unprivileged). Periodic activation uses `clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, ...)` to avoid drift accumulation.

**Deadline monitoring**: Absolute deadlines are computed at task creation and advanced each period. After each activation, `clock_gettime()` is compared against the deadline; overruns are logged with the exact overrun in microseconds.

**Priority Inheritance**: `rt_mutex_lock()` inspects the owner's `effective_priority` at block time and raises it if the caller has higher priority. `rt_mutex_unlock()` restores the original priority and broadcasts to all waiters.

---

## Concepts Demonstrated

| Concept | Where |
|---|---|
| Fixed-priority preemptive scheduling | `scheduler.c` |
| Rate Monotonic Analysis (RMA) | Demo 1 |
| POSIX real-time clocks & timers | `scheduler.c` — `clock_nanosleep` |
| Activation jitter measurement | `scheduler.c` — `last_jitter_us` |
| Deadline miss detection | `scheduler.c` — `deadline_misses` |
| Priority Inversion | Demo 2 |
| Priority Inheritance Protocol (PIP) | `rt_mutex.c` |
| CPU utilization analysis | Demo 3 |

---

## References

- Liu & Layland, *"Scheduling Algorithms for Multiprogramming in a Hard Real-Time Environment"*, JACM 1973
- Mars Pathfinder priority inversion incident — [Mike Jones, Microsoft Research, 1997](http://research.microsoft.com/~mbj/Mars_Pathfinder/)
- POSIX.1-2008 — `pthread_setschedparam`, `clock_nanosleep`, `SCHED_FIFO`
- Burns & Wellings, *Real-Time Systems and Programming Languages*, 4th ed.
