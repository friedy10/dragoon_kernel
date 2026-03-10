/*
 * Dragoon Microkernel - Per-CPU Data
 *
 * Each CPU core has its own percpu_data struct for task tracking,
 * idle context, and scheduler state.
 */
#ifndef DRAGOON_PERCPU_H
#define DRAGOON_PERCPU_H

#include "types.h"
#include "task.h"

#define MAX_CPUS 4

struct percpu_data {
    int cpu_id;
    int current_task_id;
    struct task_context idle_ctx;
    struct task_context dead_ctx;    /* scratch context for exiting tasks */
    volatile int reschedule_needed;
} __aligned(64);  /* cache-line aligned to avoid false sharing */

extern struct percpu_data percpu[MAX_CPUS];

/* Get current CPU ID from MPIDR_EL1 */
static inline int cpu_id(void)
{
    u64 mpidr = read_sysreg(mpidr_el1);
    return (int)(mpidr & 0xFF);
}

/* Get this CPU's per-CPU data */
static inline struct percpu_data *this_cpu(void)
{
    return &percpu[cpu_id()];
}

#endif /* DRAGOON_PERCPU_H */
