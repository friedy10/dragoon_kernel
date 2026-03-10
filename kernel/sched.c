/*
 * Dragoon Microkernel - SMP Round-Robin Scheduler
 *
 * Global run queue with spinlock. Each CPU runs its own scheduler loop.
 * Timer sets per-CPU reschedule_needed flag.
 */
#include "sched.h"
#include "task.h"
#include "spinlock.h"
#include "percpu.h"
#include "printf.h"
#include "types.h"

extern void context_switch(struct task_context *old, struct task_context *new_ctx, u64 ttbr0);

static struct spinlock sched_lock = SPINLOCK_INIT;
static int scheduler_running = 0;

void sched_init(void)
{
    scheduler_running = 0;
    kprintf("[sched] scheduler initialized (round-robin, SMP)\n");
}

void schedule(void)
{
    struct percpu_data *pcpu = this_cpu();
    int cur = pcpu->current_task_id;
    int next = -1;
    int start = (cur >= 0) ? cur : 0;

    u64 flags = spin_lock_irqsave(&sched_lock);

    /* Find next READY task (round-robin) */
    for (int i = 1; i <= MAX_TASKS; i++) {
        int id = (start + i) % MAX_TASKS;
        struct task *t = task_get(id);
        if (t && t->state == TASK_READY) {
            next = id;
            break;
        }
    }

    /* Get old context pointer */
    struct task_context *old_ctx;
    if (cur >= 0) {
        struct task *old_task = task_get(cur);
        if (old_task) {
            if (old_task->state == TASK_RUNNING)
                old_task->state = TASK_READY;
            old_ctx = &old_task->ctx;
        } else {
            /* Current task is dead, use scratch context */
            old_ctx = &pcpu->dead_ctx;
        }
    } else {
        old_ctx = &pcpu->idle_ctx;
    }

    if (next < 0) {
        /* No ready tasks - return to idle */
        if (cur >= 0) {
            pcpu->current_task_id = -1;
            spin_unlock_irqrestore(&sched_lock, flags);
            context_switch(old_ctx, &pcpu->idle_ctx, 0);
        } else {
            spin_unlock_irqrestore(&sched_lock, flags);
        }
        return;
    }

    if (next == cur) {
        struct task *t = task_get(cur);
        if (t) t->state = TASK_RUNNING;
        spin_unlock_irqrestore(&sched_lock, flags);
        return;
    }

    struct task *next_task = task_get(next);
    next_task->state = TASK_RUNNING;
    pcpu->current_task_id = next;

    /* Compute TTBR0 for the new task (0 = no switch, keep current) */
    u64 ttbr0_val = 0;
    if (next_task->as.pgd) {
        ttbr0_val = (u64)next_task->as.pgd | ((u64)next_task->as.asid << 48);
    }

    /* Release lock BEFORE context_switch — the task is already marked RUNNING
     * so no other CPU will pick it up */
    spin_unlock_irqrestore(&sched_lock, flags);

    context_switch(old_ctx, &next_task->ctx, ttbr0_val);
}

void sched_yield(void)
{
    this_cpu()->reschedule_needed = 0;
    schedule();
}

void sched_start(void)
{
    scheduler_running = 1;
    kprintf("[sched] CPU %d starting scheduler\n", cpu_id());

    /* Run scheduler loop */
    while (1) {
        if (task_count() == 0) {
            kprintf("[sched] no tasks, halting\n");
            while (1)
                __asm__ volatile("wfe");
        }

        schedule();

        /* If we return here, no tasks are ready. Wait for interrupt. */
        __asm__ volatile("wfe");
    }
}
