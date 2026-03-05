# RT-Scheduler — Real-Time Task Scheduler on Linux

A preemptive, fixed-priority real-time task scheduler implemented in C using POSIX threads and `CLOCK_MONOTONIC`. Demonstrates core RTOS concepts — periodic task scheduling, deadline monitoring, the Priority Inheritance Protocol (PIP), and CPU utilization analysis — entirely in software on Linux or macOS (via VS Code Dev Container).

---

## Motivation

Real-time systems are defined not just by *what* they compute, but *when*. Missing a deadline in a flight controller or antilock brake system is a failure regardless of correctness. This project implements the scheduling infrastructure that underpins such systems and demonstrates two critical failure modes: **priority inversion** and **CPU overload**.

The priority inversion scenario directly mirrors the **Mars Pathfinder bug (1997)**, where a low-priority task holding a mutex was preempted by medium-priority tasks, starving the high-priority communications task and causing repeated system resets — resolved in flight by enabling priority inheritance on VxWorks.

---

## Project Structure

```
rt-scheduler/
├── include/
│   └── scheduler.h               # Task Control Block, rt_mutex, trace API
├── src/
│   ├── scheduler.c               # Preemptive scheduler, deadline tracking, trace recorder
│   └── rt_mutex.c                # Mutex with Priority Inheritance Protocol (PIP)
├── demos/
│   ├── demo_periodic.c           # Rate Monotonic task set → trace_periodic.csv
│   ├── demo_priority_inversion.c # PIP demo (Mars Pathfinder scenario)
│   └── demo_overload.c           # Overloaded task set → trace_overload.csv
├── tools/
│   ├── make_gantt.py             # Gantt chart generator (reads CSV → PNG)
│   └── make_cpuutil.py           # CPU utilization chart generator (reads CSV → PNG)
└── Makefile
```

---

## Task Control Block (TCB)

Each task is described by:

| Field | Description |
|---|---|
| `priority` | Fixed priority (0–99) |
| `effective_priority` | Runtime priority (raised by PIP when blocking a higher-priority task) |
| `period_ms` | Activation period |
| `deadline_ms` | Relative deadline |
| `wcet_ms` | Worst-Case Execution Time |
| `activations` | Total number of times this task has run |
| `deadline_misses` | Cumulative miss counter |
| `last_jitter_us` | Signed activation jitter in µs vs expected release time |

---

## Build & Run

### Requirements

- GCC, Make, `libpthread`
- Python 3 + matplotlib (`pip3 install matplotlib`) for visualization
- Linux native **or** macOS via VS Code Dev Container (Ubuntu)

> **Note:** `librt` is Linux-only. The Makefile detects the OS automatically and omits `-lrt` on macOS.

### Build

```bash
git clone https://github.com/YOUR_USERNAME/rt-scheduler
cd rt-scheduler
make clean && make all
```

### Run all demos

```bash
./demo_periodic
./demo_priority_inversion
./demo_overload
```

Or use the Makefile shortcuts:

```bash
make run_periodic
make run_pip
make run_overload
```

For true POSIX real-time priorities (`SCHED_FIFO`) on Linux:

```bash
sudo ./demo_periodic
```

---

## Demos

### Demo 1 — Periodic Scheduling (Rate Monotonic)

Three periodic tasks with priorities assigned by Rate Monotonic Analysis: shorter period → higher priority. The task set satisfies the RM schedulability bound:

```
U = 20/100 + 30/200 + 50/500 = 0.20 + 0.15 + 0.10 = 0.55 < ln(2) ≈ 0.693  ✓
```

| Task | Priority | Period | WCET |
|---|---|---|---|
| SensorTask | 80 | 100ms | 20ms |
| ControlTask | 60 | 200ms | 30ms |
| LoggerTask | 40 | 500ms | 50ms |

Per-activation jitter is measured using `CLOCK_MONOTONIC` and reported in the stats table. On Linux with `sudo`, jitter is typically under 100µs. On macOS, expect 1–5ms due to the general-purpose scheduler.

After running, generates `trace_periodic.csv` for visualization.

---

### Demo 2 — Priority Inversion & Priority Inheritance Protocol

Classic three-task inversion scenario. Without mitigation, `MediumTask` preempts `LowTask` while `LowTask` holds a mutex that `HighTask` needs — blocking `HighTask` for an unbounded duration despite being the highest-priority task in the system.

```
LowTask    prio=20  acquires mutex, runs 80ms critical section
MediumTask prio=50  CPU-bound, no mutex — preempts LowTask
HighTask   prio=80  needs mutex → blocks → triggers PIP boost
```

The Priority Inheritance Protocol boosts `LowTask` to priority 80 so `MediumTask` can no longer preempt it, bounding `HighTask`'s wait time.

Expected terminal output:
```
[PIP]  'HighTask' (prio=80) boosts 'LowTask' (prio=20→80) holding 'shared_resource'
[PIP]  'LowTask' priority restored (80→20) after releasing 'shared_resource'
[HighTask] GOT mutex after 60 ms (PIP kept this bounded)
```

> This demo is best shown as a **terminal screenshot** — the live log output is more expressive than a chart for this scenario.

---

### Demo 3 — Deadline Miss Detection Under Overload

Intentionally overloaded task set guaranteed to exceed CPU capacity:

```
U = 40/100 + 60/150 + 80/200 = 0.40 + 0.40 + 0.40 = 1.20 > 1.0  ✗
```

The scheduler detects each deadline miss with microsecond precision and logs the exact overrun to stderr. After running, generates `trace_overload.csv` for visualization.

> **macOS note:** Modern Macs run pthreads across multiple cores simultaneously, so U=1.2 may not produce deadline misses on macOS even though the math says it should. The CPU utilization chart will still correctly show total U > 100%. On a single-core Linux system (`taskset -c 0 ./demo_overload`) misses appear as expected.

---

## Visualization

After running the demos, use the Python tools to generate charts from the trace CSV files.

### Requirements

```bash
pip3 install matplotlib
```

### Generate all charts

```bash
# Step 1: run demos to produce CSV traces
./demo_periodic
./demo_overload

# Step 2: generate Gantt charts
python3 tools/make_gantt.py trace_periodic.csv trace_periodic_gantt.png "Gantt Chart — Periodic Scheduling (RM, U=0.55)"
python3 tools/make_gantt.py trace_overload.csv trace_overload_gantt.png "Gantt Chart — Overloaded Task Set (U=1.20)"

# Step 3: generate CPU utilization charts
python3 tools/make_cpuutil.py trace_periodic.csv trace_periodic_cpuutil.png "CPU Utilization — Periodic Scheduling (RM, U=0.55)"
python3 tools/make_cpuutil.py trace_overload.csv trace_overload_cpuutil.png "CPU Utilization — Overloaded (U=1.20)"
```

> CSV files are written to whichever directory you run the binary from. Run `./demo_periodic` from the project root, then the chart commands from the same directory.

### What the charts show

**Gantt chart** — Each horizontal bar is one task activation. The x-axis is wall-clock time in ms. Dotted red vertical lines mark absolute deadlines. Red bars (if any) indicate deadline misses.

**CPU utilization chart** — Per-task CPU usage as a percentage of total observation time. The orange dotted line is the Rate Monotonic schedulability bound (69.3%). The red dashed line is 100%. A healthy task set stays left of the orange line. An overloaded set crosses the red line.

### How tracing works

Every task activation is recorded to a global trace buffer in `scheduler.c`. Timestamps are captured with `CLOCK_MONOTONIC` and stored relative to `scheduler_start()`. After `scheduler_stop()`, `trace_dump_csv()` writes the buffer to a CSV file. All timestamp arithmetic uses `int64_t` to avoid uint64 wraparound on nanosecond subtraction across second boundaries.

---

## Key Implementation Details

**Portable sleep:** `clock_nanosleep()` with `TIMER_ABSTIME` is Linux-only and unavailable on macOS. The scheduler uses a portable `sleep_until()` that computes remaining time and calls `nanosleep()`, achieving drift-free periodic activation on both platforms.

**Deadline monitoring:** Absolute deadlines are computed at task creation and advanced each period. After each activation, `clock_gettime()` is compared against the deadline; overruns are logged with exact microsecond precision.

**Priority Inheritance:** `rt_mutex_lock()` inspects the owner's `effective_priority` at block time and raises it if the caller has higher priority. `rt_mutex_unlock()` restores the original priority and broadcasts to all waiters.

**Trace recorder:** Each activation records start time, end time, absolute deadline, and whether the deadline was missed into a global ring buffer. A global epoch (`g_epoch`) is captured at `scheduler_start()` so all timestamps are relative to t=0.

---

## Concepts Demonstrated

| Concept | Where |
|---|---|
| Fixed-priority preemptive scheduling | `scheduler.c` |
| Rate Monotonic Analysis (RMA) | Demo 1 |
| Activation jitter measurement (µs) | `scheduler.c` — `last_jitter_us` |
| Deadline miss detection | `scheduler.c` — `deadline_misses` |
| Priority Inversion (Mars Pathfinder scenario) | Demo 2 |
| Priority Inheritance Protocol (PIP) | `rt_mutex.c` |
| CPU utilization analysis | Demo 3 |
| Execution trace recording | `scheduler.c` — `trace_dump_csv()` |
| Gantt chart visualization | `tools/make_gantt.py` |
| Cross-platform portability (Linux + macOS) | `Makefile`, `sleep_until()` |

---

## References

- Liu & Layland, *"Scheduling Algorithms for Multiprogramming in a Hard Real-Time Environment"*, JACM 1973
- Mars Pathfinder priority inversion incident — [Mike Jones, Microsoft Research, 1997](http://research.microsoft.com/~mbj/Mars_Pathfinder/)
- POSIX.1-2008 — `pthread_setschedparam`, `clock_gettime`, `SCHED_FIFO`
- Burns & Wellings, *Real-Time Systems and Programming Languages*, 4th ed.
