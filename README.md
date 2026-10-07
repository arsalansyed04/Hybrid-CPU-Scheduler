# Hybrid CPU Scheduler Simulator (C)

A small CPU scheduling simulator written in C for an Operating Systems assignment. It runs a **three-level hybrid scheduler** for a mixed workload of real-time, interactive and batch processes, prints a text Gantt chart and per-process metrics, and compares the result against plain Round Robin and plain SJF.

## Problem

Classic schedulers (FCFS, SJF, Priority, Round Robin) each do well on one kind of workload and badly on another. Real systems mix **real-time tasks**, **interactive applications** and **batch jobs**, so a single fixed policy gives either poor responsiveness or starvation.

## How it works

Every time unit, the scheduler picks the next process from the highest non-empty tier:

| Tier | Process type | Policy inside the tier |
| :--- | :--- | :--- |
| 1 (highest) | Real-time | Priority (lowest number wins), re-evaluated every tick, so a more urgent arrival takes the CPU |
| 2 | Interactive | Round Robin with a time quantum of 2 |
| 3 (lowest) | Batch | Shortest Remaining Time First |

Batch processes also have an **aging** counter. While a batch process waits, the counter ticks, and every 5 ticks (`AGING_THRESHOLD`) its priority value is lowered by 1 (`AGING_BOOST`).

Settings live at the top of `scheduler.c`: `MAX_PROCESSES` (20), `MAX_TIME` (200), `TIME_QUANTUM` (2), `AGING_THRESHOLD` (5), `AGING_BOOST` (1).

## Build and run

```bash
gcc -o scheduler scheduler.c
./scheduler
```

No input is needed. The workload is hard-coded in `load_sample_processes()`; edit it to try your own.

| PID | Type | Arrival | Burst | Priority |
| :-: | :--- | :-: | :-: | :-: |
| P1 | Real-time | 0 | 4 | 1 |
| P2 | Real-time | 2 | 3 | 2 |
| P3 | Interactive | 1 | 6 | 3 |
| P4 | Interactive | 3 | 4 | 3 |
| P5 | Batch | 0 | 8 | 5 |
| P6 | Batch | 4 | 5 | 5 |

## Output

Gantt chart (one box per time unit):

```
CPU: [P1][P1][P1][P1][P2][P2][P2][P3][P3][P3][P3][P3][P3][P4][P4][P4][P4][P6][P6][P6][P6][P6][P5][P5][P5][P5][P5][P5][P5][P5]
Time: 0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  25  26  27  28  29  30
```

Per-process metrics (hybrid scheduler):

| PID | Type | Arrival | Burst | Finish | Turnaround | Waiting |
| :-: | :--- | :-: | :-: | :-: | :-: | :-: |
| 1 | Real-time | 0 | 4 | 4 | 4 | 0 |
| 2 | Real-time | 2 | 3 | 7 | 5 | 2 |
| 3 | Interactive | 1 | 6 | 13 | 12 | 6 |
| 4 | Interactive | 3 | 4 | 17 | 14 | 10 |
| 5 | Batch | 0 | 8 | 30 | 30 | 22 |
| 6 | Batch | 4 | 5 | 22 | 18 | 13 |

Comparison with standard algorithms:

| Scheduler | Avg waiting | Avg turnaround | CPU utilisation |
| :--- | :-: | :-: | :-: |
| **Hybrid** | 8.83 | 13.83 | 100% |
| Round Robin (q = 2) | 15.17 | 20.17 | 100% |
| SJF (non-preemptive) | 8.33 | 13.33 | 100% |

**Reading the results.** The hybrid scheduler cuts average waiting time by about 42% compared to plain Round Robin, and real-time tasks are served first (waits of 0 and 2 time units). It is slightly worse than plain SJF on these averages (8.83 vs 8.33). That is plausible, since SJF is known for low average waiting time, but it ignores process type, so a real-time task with a long burst would get no priority. This is a single six-process example, so treat the numbers as an illustration, not a benchmark. CPU utilisation is 100% because the sample workload has no idle gaps.

Metrics are computed as turnaround = finish - arrival, waiting = turnaround - burst, and utilisation = total burst / last finish time.

## Known issues and limitations

These are things I found while documenting the code. They are listed here rather than hidden.

1. **The interactive tier is not really Round Robin yet.** When a quantum expires, `rr_head` is reset to `-1` (line 148), and the next search then restarts from process index 0. P3 is always found first, so it keeps the CPU until it finishes. The Gantt chart shows this: P3 runs for 6 units in a row before P4 starts. Deleting that reset (so rotation continues from the last-served process) gives the expected `P3 P3 P4 P4 P3 P3 P4 P4 P3 P3` interleaving. With that change, interactive waiting becomes P3 = 10 and P4 = 8, and hybrid average waiting rises to 9.17 (turnaround 14.17). The results above are from the code as submitted.
2. **Aging does not affect scheduling yet.** The aging counter lowers a batch process's `priority` value, but batch selection uses only remaining time (`pick_batch`), and tiers are strict, so batch jobs can still be starved by a steady stream of real-time or interactive work. Wiring aging into batch selection, or promoting an aged batch process a tier, would make the starvation protection real.
3. **Process types are given, not detected.** Each process is labelled real-time, interactive or batch in the input. The scheduler does not infer type from burst-time patterns or arrival rate.
4. **Real-time handling is priority only.** There are no deadlines and no schedulability checks.
5. **Fixed workload and limits.** The sample is hard-coded, with at most 20 processes and 200 time units.
6. **Comparison baselines ignore process type.** Round Robin and SJF treat all processes equally, which is what makes them "standard", but it also means the comparison is only on average wait and turnaround, not on responsiveness per class.

## Possible next steps

- Fix the Round Robin rotation and make aging influence batch selection.
- Read the workload from a file or generate random workloads, and average over many runs.
- Add per-class metrics (response time for interactive tasks, deadline misses for real-time tasks).
- Add FCFS and Priority baselines.
- Draw a real Gantt chart (for example with matplotlib) from the simulator output.

## Files

```
.
├── scheduler.c   # simulator: hybrid scheduler, Round Robin and SJF baselines, output
└── README.md
```
