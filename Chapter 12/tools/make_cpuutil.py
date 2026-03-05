#!/usr/bin/env python3
"""
make_cpuutil.py — Generate CPU utilization chart from rt-scheduler trace CSV
Usage: python3 tools/make_cpuutil.py <trace.csv> <output.png> "<title>"
Example:
    python3 tools/make_cpuutil.py trace_periodic.csv trace_periodic_cpuutil.png "CPU Util — Periodic"
    python3 tools/make_cpuutil.py trace_overload.csv trace_overload_cpuutil.png "CPU Util — Overloaded"
"""
import csv, sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

COLORS = ['#4C9BE8', '#56C271', '#E8924C', '#C45FE0', '#4CE8D8']
BG, GRID, TEXT, AXIS = '#0D1117', '#21262D', '#E6EDF3', '#8B949E'

if len(sys.argv) < 4:
    print("Usage: python3 tools/make_cpuutil.py <trace.csv> <output.png> <title>")
    sys.exit(1)

csv_file, out_file, title = sys.argv[1], sys.argv[2], sys.argv[3]

rows = []
with open(csv_file) as f:
    for r in csv.DictReader(f):
        rows.append({
            'n': r['task_name'],
            's': float(r['start_ms']),
            'e': float(r['end_ms'])
        })

tasks  = list(dict.fromkeys(r['n'] for r in rows))
total  = max(r['e'] for r in rows)
utils  = {t: sum(r['e'] - r['s'] for r in rows if r['n'] == t) / total * 100
          for t in tasks}
total_u = sum(utils.values())

fig, ax = plt.subplots(figsize=(8, 4))
fig.patch.set_facecolor(BG)
ax.set_facecolor(BG)

bars = ax.barh(tasks,
               [utils[t] for t in tasks],
               color=[COLORS[i % len(COLORS)] for i in range(len(tasks))],
               height=0.5)

ax.axvline(100,  color='#FF4444', lw=1.5, ls='--', label='U = 100%')
ax.axvline(69.3, color='#FFB347', lw=1.2, ls=':', alpha=0.9, label='RM schedulable bound 69.3%')

for bar, t in zip(bars, tasks):
    ax.text(bar.get_width() + 1, bar.get_y() + bar.get_height() / 2,
            f'{utils[t]:.1f}%', va='center', color=TEXT, fontsize=9)

ax.set_xlabel('CPU Utilization (%)', color=AXIS)
ax.set_title(f'{title}\nTotal utilization: {total_u:.1f}%', color=TEXT)
ax.tick_params(colors=AXIS)
ax.set_xlim(0, max(130, total_u * 1.25))

for s in ax.spines.values():
    s.set_edgecolor(GRID)

ax.legend(facecolor='#161B22', edgecolor=GRID, labelcolor=TEXT, fontsize=8)

plt.tight_layout()
plt.savefig(out_file, dpi=150, bbox_inches='tight', facecolor=BG)
plt.close()
print(f"[VIZ] Saved → {out_file}")
