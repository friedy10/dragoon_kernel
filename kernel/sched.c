/*
 * Dragoon Microkernel - Round-Robin Scheduler
 *
 * Cooperative scheduling: tasks yield via schedule() or SYS_YIELD.
 * Timer sets reschedule_needed flag but doesn't preempt directly.
 */
#include "sched.h"
#include "task.h"
#include "printf.h"
#include "types.h"

extern void context_switch(struct task_context *old, struct task_context *new);
extern volatile int reschedule_needed;

/* Idle context (kernel main's context) */
static struct task_context idle_ctx;
static int scheduler_running = 0;

void sched_init(void)
{
    scheduler_running = 0;
    kprintf("[sched] scheduler initialized (round-robin)\n");
}

/* Scratch context for dead/exiting tasks (context is saved but discarded) */
static struct task_context dead_ctx;

void schedule(void)
{
    int cur = task_current_id();
    int next = -1;
    int start = (cur >= 0) ? cur : 0;

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
            old_ctx = &dead_ctx;
        }
    } else {
        old_ctx = &idle_ctx;
    }

    if (next < 0) {
        /* No ready tasks - return to idle */
        if (cur >= 0) {
            task_set_current(-1);
            context_switch(old_ctx, &idle_ctx);
        }
        return;
    }

    if (next == cur)
        return;

    struct task *next_task = task_get(next);
    next_task->state = TASK_RUNNING;
    task_set_current(next);
    context_switch(old_ctx, &next_task->ctx);
}

void sched_yield(void)
{
    reschedule_needed = 0;
    schedule();
}

void sched_start(void)
{
    scheduler_running = 1;
    kprintf("[sched] starting scheduler\n");

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
