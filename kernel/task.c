/*
 * Dragoon Microkernel - Task Management
 *
 * Each task has its own stack, capability table, and saved context.
 * Tasks run at EL1 (same privilege as kernel) for simplicity.
 */
#include "task.h"
#include "mm.h"
#include "vm.h"
#include "spinlock.h"
#include "percpu.h"
#include "printf.h"

static struct task tasks[MAX_TASKS];
static int num_tasks = 0;
static struct spinlock task_lock __unused = SPINLOCK_INIT;

/* Per-CPU data array */
struct percpu_data percpu[MAX_CPUS];

/* External: assembly context switch and task trampoline */
extern void context_switch(struct task_context *old, struct task_context *new);
extern void task_trampoline(void);

/* Called by task_trampoline when a task's entry function returns */
void task_exit_handler(void)
{
    int id = this_cpu()->current_task_id;
    kprintf("[task] task %d '%s' exited\n", id, tasks[id].name);
    tasks[id].state = TASK_DEAD;
    num_tasks--;

    /* Yield to scheduler - never returns */
    extern void schedule(void);
    while (1)
        schedule();
}

void task_init(void)
{
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].id = i;
        tasks[i].state = TASK_DEAD;
        tasks[i].num_caps = 0;
        tasks[i].stack_base = 0;
    }
    num_tasks = 0;

    /* Initialize per-CPU data */
    for (int c = 0; c < MAX_CPUS; c++) {
        percpu[c].cpu_id = c;
        percpu[c].current_task_id = -1;
        percpu[c].reschedule_needed = 0;
    }

    kprintf("[task] task subsystem initialized, max tasks: %d\n", MAX_TASKS);
}

int task_create(const char *name, void (*entry)(void))
{
    /* Find free slot */
    int id = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD) {
            id = i;
            break;
        }
    }
    if (id < 0) {
        kprintf("[task] ERROR: no free task slots\n");
        return -1;
    }

    struct task *t = &tasks[id];

    /* Copy name */
    int j;
    for (j = 0; j < 31 && name[j]; j++)
        t->name[j] = name[j];
    t->name[j] = '\0';

    /* Create per-task address space */
    if (vm_create(&t->as) < 0) {
        kprintf("[task] ERROR: failed to create address space\n");
        t->state = TASK_DEAD;
        return -1;
    }

    /* Allocate stack (4 contiguous pages = 16KB) */
    void *stack = pages_alloc(TASK_STACK_SIZE / PAGE_SIZE);
    if (!stack) {
        kprintf("[task] ERROR: out of memory for stack\n");
        vm_destroy(&t->as);
        t->state = TASK_DEAD;
        return -1;
    }
    t->stack_base = (u64)stack;

    /* Stack pointer: top of stack, 16-byte aligned */
    u64 sp = t->stack_base + TASK_STACK_SIZE;
    sp &= ~0xFULL;

    /* Set up initial context:
     * - x30 (LR) = task_trampoline (context_switch ret's here)
     * - x19 = real entry function (task_trampoline calls via blr x19)
     */
    t->ctx.sp = sp;
    t->ctx.x30 = (u64)task_trampoline;
    t->ctx.x29 = 0;
    t->ctx.x19 = (u64)entry;
    t->ctx.x20 = 0;
    t->ctx.x21 = 0;
    t->ctx.x22 = 0;
    t->ctx.x23 = 0;
    t->ctx.x24 = 0;
    t->ctx.x25 = 0;
    t->ctx.x26 = 0;
    t->ctx.x27 = 0;
    t->ctx.x28 = 0;

    /* Clear capability table */
    for (int c = 0; c < MAX_CAPS_PER_TASK; c++)
        t->cap_table[c] = -1;
    t->num_caps = 0;

    t->state = TASK_READY;
    num_tasks++;

    kprintf("[task] created task '%s' (id=%d, entry=%p, stack=%p)\n",
            name, id, entry, (void *)t->stack_base);

    return id;
}

void task_destroy(int task_id)
{
    if (task_id < 0 || task_id >= MAX_TASKS)
        return;
    struct task *t = &tasks[task_id];
    if (t->state == TASK_DEAD)
        return;

    /* Free stack pages */
    pages_free((void *)t->stack_base, TASK_STACK_SIZE / PAGE_SIZE);

    /* Destroy address space */
    vm_destroy(&t->as);

    t->state = TASK_DEAD;
    num_tasks--;
    kprintf("[task] destroyed task '%s' (id=%d)\n", t->name, task_id);
}

struct task *task_get(int task_id)
{
    if (task_id < 0 || task_id >= MAX_TASKS)
        return NULL;
    if (tasks[task_id].state == TASK_DEAD)
        return NULL;
    return &tasks[task_id];
}

struct task *task_current(void)
{
    int id = this_cpu()->current_task_id;
    if (id < 0)
        return NULL;
    return &tasks[id];
}

int task_current_id(void)
{
    return this_cpu()->current_task_id;
}

void task_set_current(int id)
{
    this_cpu()->current_task_id = id;
}

int task_count(void)
{
    return num_tasks;
}

/* Add a capability to a task */
int task_add_cap(int task_id, int cap_id)
{
    struct task *t = task_get(task_id);
    if (!t)
        return -1;
    if (t->num_caps >= MAX_CAPS_PER_TASK)
        return -1;
    t->cap_table[t->num_caps++] = cap_id;
    return 0;
}
