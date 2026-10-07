#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PROCESSES 20
#define MAX_TIME 200
#define TIME_QUANTUM 2
#define AGING_THRESHOLD 5
#define AGING_BOOST 1

#define REAL_TIME 0
#define INTERACTIVE 1
#define BATCH 2

typedef struct {
    int pid;
    char name[10];
    int type;
    int arrival_time;
    int burst_time;
    int remaining_time;
    int priority;
    int completion_time;
    int waiting_time;
    int turnaround_time;
    int started;
    int finished;
    int wait_counter;
    int rr_slice;
} Process;

Process procs[MAX_PROCESSES];
int n;
int gantt[MAX_TIME];
int gantt_len = 0;

const char *type_name(int t) {
    if (t == REAL_TIME) return "RealTime";
    if (t == INTERACTIVE) return "Interactive";
    return "Batch";
}

void reset_state(Process *p) {
    p->remaining_time = p->burst_time;
    p->completion_time = 0;
    p->waiting_time = 0;
    p->turnaround_time = 0;
    p->started = 0;
    p->finished = 0;
    p->wait_counter = 0;
    p->rr_slice = TIME_QUANTUM;
}

void apply_aging(int current_time, int running_pid) {
    for (int i = 0; i < n; i++) {
        if (procs[i].type == BATCH &&
            !procs[i].finished &&
            procs[i].arrival_time <= current_time &&
            procs[i].pid != running_pid)
        {
            procs[i].wait_counter++;
            if (procs[i].wait_counter >= AGING_THRESHOLD) {
                if (procs[i].priority > 1)
                    procs[i].priority -= AGING_BOOST;
                procs[i].wait_counter = 0;
            }
        }
    }
}

int pick_realtime(int current_time) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (procs[i].type == REAL_TIME &&
            !procs[i].finished &&
            procs[i].arrival_time <= current_time)
        {
            if (best == -1 || procs[i].priority < procs[best].priority)
                best = i;
        }
    }
    return best;
}

static int rr_head = -1;
int pick_interactive(int current_time) {
    if (rr_head != -1 &&
        !procs[rr_head].finished &&
        procs[rr_head].type == INTERACTIVE &&
        procs[rr_head].arrival_time <= current_time &&
        procs[rr_head].rr_slice > 0)
    {
        return rr_head;
    }
    int start = (rr_head == -1) ? 0 : (rr_head + 1) % n;
    for (int j = 0; j < n; j++) {
        int i = (start + j) % n;
        if (procs[i].type == INTERACTIVE &&
            !procs[i].finished &&
            procs[i].arrival_time <= current_time)
        {
            procs[i].rr_slice = TIME_QUANTUM;
            rr_head = i;
            return i;
        }
    }
    return -1;
}

int pick_batch(int current_time) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (procs[i].type == BATCH &&
            !procs[i].finished &&
            procs[i].arrival_time <= current_time)
        {
            if (best == -1 || procs[i].remaining_time < procs[best].remaining_time)
                best = i;
        }
    }
    return best;
}

void run_hybrid_scheduler(void) {
    int time = 0;
    int done = 0;
    rr_head = -1;
    for (int i = 0; i < n; i++) reset_state(&procs[i]);
    while (done < n && time < MAX_TIME) {
        int idx = -1;
        idx = pick_realtime(time);
        if (idx == -1)
            idx = pick_interactive(time);
        if (idx == -1)
            idx = pick_batch(time);
        if (idx == -1) {
            gantt[gantt_len++] = -1;
            time++;
            continue;
        }
        Process *p = &procs[idx];
        if (!p->started) p->started = 1;
        gantt[gantt_len++] = p->pid;
        p->remaining_time--;
        if (p->type == INTERACTIVE) {
            p->rr_slice--;
            if (p->rr_slice == 0)
                rr_head = -1;
        }
        apply_aging(time, p->pid);
        time++;
        if (p->remaining_time == 0) {
            p->finished = 1;
            p->completion_time = time;
            p->turnaround_time = p->completion_time - p->arrival_time;
            p->waiting_time = p->turnaround_time - p->burst_time;
            done++;
        }
    }
}

void run_pure_rr(float *avg_wt, float *avg_tat, float *avg_util) {
    int rem[MAX_PROCESSES], comp[MAX_PROCESSES];
    int slice[MAX_PROCESSES];
    for (int i = 0; i < n; i++) {
        rem[i] = procs[i].burst_time;
        comp[i] = 0;
        slice[i] = TIME_QUANTUM;
    }
    int time = 0, done = 0, head = 0;
    while (done < n && time < MAX_TIME) {
        int found = 0;
        for (int j = 0; j < n; j++) {
            int i = (head + j) % n;
            if (rem[i] > 0 && procs[i].arrival_time <= time) {
                rem[i]--;
                time++;
                if (rem[i] == 0) {
                    comp[i] = time;
                    done++;
                    head = (i + 1) % n;
                    found = 1;
                    break;
                }
                slice[i]--;
                if (slice[i] == 0) {
                    slice[i] = TIME_QUANTUM;
                    head = (i + 1) % n;
                }
                found = 1;
                break;
            }
        }
        if (!found) time++;
    }
    float total_wt = 0, total_tat = 0;
    int total_burst = 0, max_time = 0;
    for (int i = 0; i < n; i++) {
        int tat = comp[i] - procs[i].arrival_time;
        int wt = tat - procs[i].burst_time;
        total_tat += tat;
        total_wt += (wt < 0) ? 0 : wt;
        total_burst += procs[i].burst_time;
        if (comp[i] > max_time) max_time = comp[i];
    }
    *avg_wt = total_wt / n;
    *avg_tat = total_tat / n;
    *avg_util = (max_time > 0) ? ((float)total_burst / max_time) * 100 : 0;
}

void run_pure_sjf(float *avg_wt, float *avg_tat, float *avg_util) {
    int rem[MAX_PROCESSES], comp[MAX_PROCESSES], done_flag[MAX_PROCESSES];
    for (int i = 0; i < n; i++) {
        rem[i] = procs[i].burst_time;
        comp[i] = 0;
        done_flag[i] = 0;
    }
    int time = 0, done = 0;
    while (done < n && time < MAX_TIME) {
        int best = -1;
        for (int i = 0; i < n; i++) {
            if (!done_flag[i] && procs[i].arrival_time <= time) {
                if (best == -1 || rem[i] < rem[best])
                    best = i;
            }
        }
        if (best == -1) { time++; continue; }
            time += rem[best];
        comp[best] = time;
        done_flag[best] = 1;
        done++;
    }
    float total_wt = 0, total_tat = 0;
    int total_burst = 0, max_time = 0;
    for (int i = 0; i < n; i++) {
        int tat = comp[i] - procs[i].arrival_time;
        int wt = tat - procs[i].burst_time;
        total_tat += tat;
        total_wt += (wt < 0) ? 0 : wt;
        total_burst += procs[i].burst_time;
        if (comp[i] > max_time) max_time = comp[i];
    }
    *avg_wt = total_wt / n;
    *avg_tat = total_tat / n;
    *avg_util = (max_time > 0) ? ((float)total_burst / max_time) * 100 : 0;
}

void print_gantt(void) {
    printf("\n--- GANTT CHART ---\n");
    printf("CPU: ");
    for (int t = 0; t < gantt_len; t++) {
        if (gantt[t] == -1)
            printf("[--]");
        else
            printf("[P%d]", gantt[t]);
    }
    printf("\n");
    printf("Time: ");
    for (int t = 0; t <= gantt_len; t++) printf("%-4d", t);
    printf("\n");
}

void print_metrics(void) {
    printf("\n--- PERFORMANCE METRICS (Hybrid) ---\n");
    printf("%-6s %-12s %-8s %-8s %-8s %-12s %-8s\n",
        "PID", "Type", "Arrival", "Burst", "Finish", "Turnaround", "Waiting");
    printf("-------------------------------------------------------------\n");
    float total_wt = 0, total_tat = 0;
    int total_burst = 0;
    int max_time = 0;
    for (int i = 0; i < n; i++) {
        printf("%-6d %-12s %-8d %-8d %-8d %-12d %-8d\n",
            procs[i].pid,
            type_name(procs[i].type),
            procs[i].arrival_time,
            procs[i].burst_time,
            procs[i].completion_time,
            procs[i].turnaround_time,
            procs[i].waiting_time);
        total_wt += procs[i].waiting_time;
        total_tat += procs[i].turnaround_time;
        total_burst += procs[i].burst_time;
        if (procs[i].completion_time > max_time)
            max_time = procs[i].completion_time;
    }
    float avg_wt = total_wt / n;
    float avg_tat = total_tat / n;
    float cpu_util = (max_time > 0) ? ((float)total_burst / max_time) * 100 : 0;
    printf("-------------------------------------------------------------\n");
    printf("Average Waiting Time : %.2f\n", avg_wt);
    printf("Average Turnaround Time : %.2f\n", avg_tat);
    printf("CPU Utilization : %.2f%%\n", cpu_util);
}

void print_comparison(void) {
    float hybrid_wt = 0, hybrid_tat = 0;
    int total_burst = 0, max_time = 0;
    for (int i = 0; i < n; i++) {
        hybrid_wt += procs[i].waiting_time;
        hybrid_tat += procs[i].turnaround_time;
        total_burst += procs[i].burst_time;
        if (procs[i].completion_time > max_time)
            max_time = procs[i].completion_time;
    }
    hybrid_wt /= n;
    hybrid_tat /= n;
    float hybrid_util = (max_time > 0) ? ((float)total_burst / max_time) * 100 : 0;
    float rr_wt, rr_tat, rr_util;
    float sjf_wt, sjf_tat, sjf_util;
    run_pure_rr (&rr_wt, &rr_tat, &rr_util);
    run_pure_sjf(&sjf_wt, &sjf_tat, &sjf_util);
    printf("\n--- COMPARISON WITH STANDARD ALGORITHMS ---\n");
    printf("%-22s %-12s %-18s %-10s\n", "Scheduler", "Avg Wait", "Avg Turnaround", "CPU Util");
    printf("--------------------------------------------------------------\n");
    printf("%-22s %-12.2f %-18.2f %.2f%%\n", "Hybrid (Ours)", hybrid_wt, hybrid_tat, hybrid_util);
    printf("%-22s %-12.2f %-18.2f %.2f%%\n", "Pure Round Robin", rr_wt, rr_tat, rr_util);
    printf("%-22s %-12.2f %-18.2f %.2f%%\n", "Pure SJF", sjf_wt, sjf_tat, sjf_util);
    printf("--------------------------------------------------------------\n");
}

void load_sample_processes(void) {
    n = 6;
    procs[0] = (Process){1, "P1", REAL_TIME, 0, 4, 4, 1};
    procs[1] = (Process){2, "P2", REAL_TIME, 2, 3, 3, 2};
    procs[2] = (Process){3, "P3", INTERACTIVE, 1, 6, 6, 3};
    procs[3] = (Process){4, "P4", INTERACTIVE, 3, 4, 4, 3};
    procs[4] = (Process){5, "P5", BATCH, 0, 8, 8, 5};
    procs[5] = (Process){6, "P6", BATCH, 4, 5, 5, 5};
    for (int i = 0; i < n; i++) {
        procs[i].completion_time = 0;
        procs[i].waiting_time = 0;
        procs[i].turnaround_time = 0;
        procs[i].started = 0;
        procs[i].finished = 0;
        procs[i].wait_counter = 0;
        procs[i].rr_slice = TIME_QUANTUM;
    }
}

int main(void) {
    printf("\n=== Intelligent CPU Scheduler: 3-Level Hybrid | OS Assignment 1 ===\n");
    load_sample_processes();
    printf("\n--- Process Input Table ---\n");
    printf("%-6s %-12s %-12s %-8s %-8s\n", "PID", "Name", "Type", "Arrival", "Burst");
    printf("--------------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("%-6d %-12s %-12s %-8d %-8d\n",
            procs[i].pid, procs[i].name,
            type_name(procs[i].type),
            procs[i].arrival_time,
            procs[i].burst_time);
    }
    run_hybrid_scheduler();
    print_gantt();
    print_metrics();
    print_comparison();
    printf("\nSimulation complete.\n\n");
    return 0;
}
