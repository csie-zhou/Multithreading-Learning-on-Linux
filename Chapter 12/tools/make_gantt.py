#!/usr/bin/env python3
"""
make_gantt.py — Generate Gantt chart from rt-scheduler trace CSV
Usage: python3 tools/make_gantt.py <trace.csv> <output.png> "<title>"
Example:
    python3 tools/make_gantt.py trace_periodic.csv trace_periodic_gantt.png "Gantt — Periodic (RM)"
    python3 tools/make_gantt.py trace_overload.csv trace_overload_gantt.png "Gantt — Overloaded"
"""
import csv, sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

COLORS = ['#4C9BE8', '#56C271', '#E8924C', '#C45FE0', '#4CE8D8']
MISS   = '#FF2020'
BG, GRID, TEXT, AXIS = '#0D1117', '#21262D', '#E6EDF3', '#8B949E'

def style(ax):
    ax.set_facecolor(BG)
    ax.tick_params(colors=AXIS, labelsize=9)
    ax.xaxis.label.set_color(AXIS)
    ax.yaxis.label.set_color(AXIS)
    ax.title.set_color(TEXT)
    for s in ax.spines.values():
        s.set_edgecolor(GRID)
    ax.grid(axis='x', color=GRID, linewidth=0.5, linestyle='--')

if len(sys.argv) < 4:
    print("Usage: python3 tools/make_gantt.py <trace.csv> <output.png> <title>")
    sys.exit(1)

csv_file, out_file, title = sys.argv[1], sys.argv[2], sys.argv[3]

rows = []
with open(csv_file) as f:
    for r in csv.DictReader(f):
        rows.append({
            'n': r['task_name'],
            's': float(r['start_ms']),
            'e': float(r['end_ms']),
            'd': float(r['deadline_ms']),
            'm': int(r['deadline_missed'])
        })

tasks  = list(dict.fromkeys(r['n'] for r in rows))
tidx   = {t: i for i, t in enumerate(tasks)}
cmap   = {t: COLORS[i % len(COLORS)] for i, t in enumerate(tasks)}
window = min(max(r['e'] for r in rows), 1000)

fig, ax = plt.subplots(figsize=(12, 3 + len(tasks) * 0.9))
fig.patch.set_facecolor(BG)
style(ax)

for r in rows:
    if r['s'] > window:
        continue
    y = tidx[r['n']]
    c = MISS if r['m'] else cmap[r['n']]
    ax.barh(y, r['e'] - r['s'], left=r['s'], height=0.5,
            color=c, edgecolor='#ffffff22', linewidth=0.4)
    if r['d'] <= window:
        n = len(tasks)
        ax.axvline(r['d'],
                   ymin=(y + 0.05) / n,
                   ymax=(y + 0.55) / n,
                   color='#FF6B6B', lw=0.8, alpha=0.7, ls=':')

ax.set_yticks(range(len(tasks)))
ax.set_yticklabels(tasks, color=TEXT, fontsize=10, fontfamily='monospace')
ax.set_xlim(0, window)
ax.set_xlabel('Time (ms)')
ax.set_title(title)

patches = [mpatches.Patch(color=cmap[t], label=t) for t in tasks]
patches.append(mpatches.Patch(color=MISS, label='Deadline Miss'))
ax.legend(handles=patches, loc='upper right',
          facecolor='#161B22', edgecolor=GRID, labelcolor=TEXT, fontsize=8)

plt.tight_layout(pad=1.5)
plt.savefig(out_file, dpi=150, bbox_inches='tight', facecolor=BG)
plt.close()
print(f"[VIZ] Saved → {out_file}")
