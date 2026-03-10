/*
 * Dragoon Microkernel - Capability System
 *
 * Global capability pool. Each capability is a typed object granting
 * access to a kernel resource (memory region, IPC endpoint, IRQ, MMIO, task).
 * Tasks hold indices into this pool via their cap_table.
 */
#include "cap.h"
#include "spinlock.h"
#include "printf.h"

static struct capability cap_pool[CAP_POOL_SIZE];
static int cap_pool_used[CAP_POOL_SIZE];
static struct spinlock cap_lock = SPINLOCK_INIT;

void cap_init(void)
{
    for (int i = 0; i < CAP_POOL_SIZE; i++) {
        cap_pool[i].type = CAP_NONE;
        cap_pool[i].refcount = 0;
        cap_pool_used[i] = 0;
    }
    kprintf("[cap] capability system initialized, pool size: %d\n", CAP_POOL_SIZE);
}

int cap_alloc(struct capability *cap_out)
{
    spin_lock(&cap_lock);
    for (int i = 0; i < CAP_POOL_SIZE; i++) {
        if (!cap_pool_used[i]) {
            cap_pool_used[i] = 1;
            cap_pool[i] = *cap_out;
            cap_pool[i].refcount = 1;
            spin_unlock(&cap_lock);
            return i;
        }
    }
    spin_unlock(&cap_lock);
    kprintf("[cap] ERROR: pool exhausted\n");
    return -1;
}

int cap_destroy(int cap_id)
{
    if (cap_id < 0 || cap_id >= CAP_POOL_SIZE)
        return -1;
    spin_lock(&cap_lock);
    if (!cap_pool_used[cap_id]) {
        spin_unlock(&cap_lock);
        return -1;
    }

    cap_pool[cap_id].refcount--;
    if (cap_pool[cap_id].refcount == 0) {
        cap_pool[cap_id].type = CAP_NONE;
        cap_pool_used[cap_id] = 0;
    }
    spin_unlock(&cap_lock);
    return 0;
}

struct capability *cap_lookup(int cap_id)
{
    if (cap_id < 0 || cap_id >= CAP_POOL_SIZE)
        return NULL;
    if (!cap_pool_used[cap_id])
        return NULL;
    return &cap_pool[cap_id];
}

int cap_check(int cap_id, u32 expected_type)
{
    struct capability *cap = cap_lookup(cap_id);
    if (!cap)
        return -1;
    if (cap->type != expected_type)
        return -1;
    return 0;
}

/* Create initial kernel capabilities */
int cap_create_memory(u64 base, u64 pages)
{
    struct capability cap = {
        .type = CAP_MEMORY,
        .mem = { .base = base, .pages = pages }
    };
    return cap_alloc(&cap);
}

int cap_create_ipc(u32 ep_id, u32 rights)
{
    struct capability cap = {
        .type = CAP_IPC,
        .ipc = { .ep_id = ep_id, .rights = rights }
    };
    return cap_alloc(&cap);
}

int cap_create_irq(u32 irq_num)
{
    struct capability cap = {
        .type = CAP_IRQ,
        .irq = { .irq_num = irq_num }
    };
    return cap_alloc(&cap);
}

int cap_create_io(u64 base, u64 size)
{
    struct capability cap = {
        .type = CAP_IO,
        .io = { .base = base, .size = size }
    };
    return cap_alloc(&cap);
}

int cap_create_task(u32 task_id)
{
    struct capability cap = {
        .type = CAP_TASK,
        .task = { .task_id = task_id }
    };
    return cap_alloc(&cap);
}
